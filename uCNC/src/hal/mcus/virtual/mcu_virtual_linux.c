/*
	Name: mcu_virtual_linux.c
	Description: Linux/POSIX port of the µCNC virtual MCU (see
	makefiles/virtual/mcu_virtual.cpp, which is Windows-only).

	Runs the REAL µCNC core (parser, planner, interpolator, protocol)
	from the my_cnc branch on a Linux host for simulation and testing.

	Transport:
	  - The Grbl protocol stream (UART2, the only stream enabled by
	    mcumap_virtual.h) is bridged to a TCP server socket
	    (default 127.0.0.1:34000, override with -p PORT or
	    UCNC_SIM_PORT). Raw bytes in both directions, so realtime
	    characters (?, !, ~, 0x18, 0x85, feed overrides, ...) work.
	  - Simulator control (probe/limits/controls injection) is via
	    stdin line commands (see print_banner): probe, limits, estop,
	    fhold, status, quit, ...

	Build: see sim-linux/Makefile (plain gcc, no PlatformIO needed).

	Copyright: Copyright (c) João Martins (original Windows version)
	Author: Linux port for SimpleCNC stability testing.
	Date: 2026-09-05

	µCNC is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. Please see <http://www.gnu.org/licenses/>

	µCNC is distributed WITHOUT ANY WARRANTY;
	Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the	GNU General Public License for more details.
*/
#include "../../../cnc.h"
#if (MCU == MCU_VIRTUAL_WIN) && defined(__linux__)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef SIM_TCP_PORT_DEFAULT
#define SIM_TCP_PORT_DEFAULT 34000
#endif

/**
 *
 * ISR emulation (single-threaded).
 *
 * The simulator runs the entire firmware core — main loop, RTC tick and
 * step-timer batches — on ONE thread (see sim_tick_poll(), driven from
 * mcu_dotasks()). That matches single-core MCU execution: callbacks can
 * never preempt each other mid-critical-section, so disable/enable are a
 * plain flag exactly like the firmware expects. An earlier revision ran
 * the RTC/step callbacks on a separate pthread with a mutex; the
 * firmware's asymmetric disable/enable idiom (utils.h __ATOMIC__ pairs an
 * unconditional disable with a conditional enable) made balanced locking
 * impossible and deadlocked motion. Single-threading removes the hazard
 * by construction.
 *
 * The stdin console thread never touches core state directly: it stages
 * pin changes into sim_pending_inputs (atomics) and the main thread
 * applies them in mcu_dotasks.
 *
 * **/
volatile bool global_isr_enabled = false;
// enables all interrupts on the mcu. Must be called to enable all IRS functions
void mcu_enable_global_isr(void)
{
	global_isr_enabled = true;
}
// disables all ISR functions
void mcu_disable_global_isr(void)
{
	global_isr_enabled = false;
}

bool mcu_get_global_isr(void)
{
	return global_isr_enabled;
}

/**
 *
 * Monotonic clock (replaces QueryPerformanceCounter)
 *
 * **/
static uint64_t sim_now_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

uint32_t mcu_micros(void)
{
	return (uint32_t)sim_now_us();
}

uint32_t mcu_millis(void)
{
	return (uint32_t)(sim_now_us() / 1000ULL);
}

uint32_t mcu_free_micros(void)
{
	return (uint32_t)(sim_now_us() % 1000ULL);
}

void virtual_delay_us(uint16_t delay)
{
	struct timespec ts;
	ts.tv_sec = delay / 1000000UL;
	ts.tv_nsec = (delay % 1000000UL) * 1000UL;
	nanosleep(&ts, NULL);
}

/**
 *
 * Communications emulation
 * UART2 (main Grbl protocol stream) -> TCP server socket
 *
 * **/
#ifndef UART2_TX_BUFFER_SIZE
#define UART2_TX_BUFFER_SIZE 64
#endif
DECL_BUFFER(uint8_t, uart2_tx, UART2_TX_BUFFER_SIZE);
DECL_BUFFER(uint8_t, uart2_rx, RX_BUFFER_SIZE);

static int sim_listen_fd = -1;
static int sim_client_fd = -1;

static void sim_client_close_locked(void)
{
	if (sim_client_fd >= 0)
	{
		close(sim_client_fd);
		sim_client_fd = -1;
		fprintf(stderr, "[sim] client disconnected\n");
	}
}

uint8_t mcu_uart2_getc(void)
{
	uint8_t c = 0;
	BUFFER_DEQUEUE(uart2_rx, &c);
	return c;
}

uint8_t mcu_uart2_available(void)
{
	return BUFFER_READ_AVAILABLE(uart2_rx);
}

void mcu_uart2_clear(void)
{
	BUFFER_CLEAR(uart2_rx);
}

void mcu_uart2_putc(uint8_t c)
{
	if (BUFFER_FULL(uart2_tx))
	{
		// No listener: drop rather than stall the firmware. A real
		// UART with no terminal attached behaves the same way.
			bool connected = (sim_client_fd >= 0);
			if (!connected)
		{
			BUFFER_CLEAR(uart2_tx);
			return;
		}
		mcu_uart2_flush();
	}
	BUFFER_ENQUEUE(uart2_tx, &c);
}

void mcu_uart2_flush(void)
{
	int fd = sim_client_fd;
	while (!BUFFER_EMPTY(uart2_tx) && fd >= 0)
	{
		uint8_t tmp[UART2_TX_BUFFER_SIZE + 1];
		memset(tmp, 0, sizeof(tmp));
		uint8_t r = 0;

		BUFFER_READ(uart2_tx, tmp, UART2_TX_BUFFER_SIZE, r);
		// NOTE: single-threaded core, so re-queueing below preserves
		// order: nothing can enqueue between the read and the put-back.
		ssize_t n = send(fd, tmp, r, MSG_NOSIGNAL);
		if (n < 0)
		{
			if (errno == EPIPE || errno == ECONNRESET)
			{
				// drop the rest of the TX buffer; client is gone
				BUFFER_CLEAR(uart2_tx);
				sim_client_close_locked();
				fd = -1;
			break;
			}
			// EAGAIN (slow reader): put everything back, retry later.
			// Dropping here would truncate lines mid-stream.
			for (int i = 0; i < r; i++)
			{
				uint8_t c = tmp[i];
				BUFFER_ENQUEUE(uart2_tx, &c);
			}
			break;
		}
		else if ((uint8_t)n < r)
		{
			// partial send (n == 0 included): re-queue the unsent tail
			for (int i = (n < 0 ? 0 : n); i < r; i++)
			{
				uint8_t c = tmp[i];
				BUFFER_ENQUEUE(uart2_tx, &c);
			}
			break;
		}
	}
}

// poll the TCP socket: accept new clients, feed RX bytes into the core
static void mcu_uart2_process(void)
{
	if (sim_listen_fd >= 0 && sim_client_fd < 0)
	{
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int fd = accept(sim_listen_fd, (struct sockaddr *)&addr, &len);
		if (fd >= 0)
		{
			int flags = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);
			sim_client_fd = fd;
			fprintf(stderr, "[sim] client connected from %s:%d\n",
					inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		}
	}
	int fd = sim_client_fd;

	if (fd < 0)
	{
		return;
	}

	bool got_data = false;
	for (;;)
	{
		uint8_t buf[64];
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}
					if (sim_client_fd == fd)
			{
				sim_client_close_locked();
			}
					break;
		}
		if (n == 0)
		{
					if (sim_client_fd == fd)
			{
				sim_client_close_locked();
			}
					break;
		}
		got_data = true;
		for (ssize_t i = 0; i < n; i++)
		{
			uint8_t c = buf[i];
			if (mcu_com_rx_cb(c))
			{
				if (BUFFER_FULL(uart2_rx))
				{
					c = OVF;
				}
				BUFFER_ENQUEUE(uart2_rx, &c);
			}
		}
	}
	(void)got_data;
}

static void sim_tick_poll(void);
static void sim_apply_pending_inputs(void);

void mcu_dotasks(void)
{
	sim_tick_poll();
	sim_apply_pending_inputs();
	mcu_uart2_process();
	mcu_uart2_flush();
	// The hosted main loop would otherwise spin at 100% CPU polling a
	// non-blocking socket. A short sleep bounds idle CPU without adding
	// meaningful latency (TCP has no baud-rate timing). It also bounds
	// the sim_tick_poll() granularity, which is fine at 100us vs the
	// 1ms RTC / 20ms step cadence.
	struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000}; // 100us
	nanosleep(&ts, NULL);
}

/**
 *
 * EEPROM emulation
 * Uses a local file to store values
 *
 *
 * **/
uint8_t mcu_eeprom_getc(uint16_t address)
{
	FILE *fp = fopen("virtualeeprom", "rb");
	uint8_t c = 0;

	if (fp != NULL)
	{
		if (!fseek(fp, address, SEEK_SET))
		{
			c = getc(fp);
		}

		fclose(fp);
	}

	return c;
}

void mcu_eeprom_putc(uint16_t address, uint8_t value)
{
	FILE *src = fopen("virtualeeprom", "rb+");

	if (!src)
	{
		FILE *dest = fopen("virtualeeprom", "wb");
		if (dest)
		{
			fclose(dest);
		}
		src = fopen("virtualeeprom", "rb+");
	}

	if (src)
	{
		fseek(src, address, SEEK_SET);
		putc((int)value, src);

		fflush(src);
		fclose(src);
	}
}

void mcu_eeprom_flush(void)
{
}

/**
 *
 * IO simulation, controlled via stdin commands (see control thread below).
 * Same VIRTUAL_MAP layout as the Windows named-pipe version so a future
 * GUI/extension can drive it over a socket with the identical struct.
 *
 * **/
typedef struct virtual_map_t
{
	uint32_t special_outputs;
	uint32_t outputs;
	uint8_t pwm[16];
	uint8_t servos[6];
	uint32_t special_inputs;
	uint32_t inputs;
	uint8_t analog[16];
} VIRTUAL_MAP;

static volatile VIRTUAL_MAP virtualmap;

// bit positions within special_inputs (must match the Windows version)
#define SIM_SPECIAL_LIMIT_BITS 0x1FFUL // X,Y,Z,X2,Y2,Z2,A,B,C
#define SIM_SPECIAL_PROBE_BIT 0x200UL
#define SIM_SPECIAL_CONTROL_BITS 0x3C00UL // ESTOP,DOOR,FHOLD,RES

// The stdin console thread stages pin changes here with atomic OR/AND;
// the main thread applies them (plus core callbacks) in
// sim_apply_pending_inputs() from mcu_dotasks(). Console commands never
// touch core state directly, so no locks are needed anywhere.
static volatile uint32_t sim_pending_set;
static volatile uint32_t sim_pending_clear;

// Stage bits to set/clear from the console thread.
static void sim_stage_inputs(uint32_t set_mask, uint32_t clear_mask)
{
	if (set_mask)
		__atomic_or_fetch(&sim_pending_set, set_mask, __ATOMIC_RELAXED);
	if (clear_mask)
		__atomic_or_fetch(&sim_pending_clear, clear_mask, __ATOMIC_RELAXED);
}

// Main-thread only: apply staged pin changes and notify the core.
static void sim_apply_pending_inputs(void)
{
	uint32_t set_mask = __atomic_exchange_n(&sim_pending_set, 0, __ATOMIC_ACQ_REL);
	uint32_t clear_mask = __atomic_exchange_n(&sim_pending_clear, 0, __ATOMIC_ACQ_REL);
	if (!set_mask && !clear_mask)
	{
		return;
	}
	uint32_t old = virtualmap.special_inputs;
	uint32_t v = (old | set_mask) & ~clear_mask;
	uint32_t diff = old ^ v;
	virtualmap.special_inputs = v;
	if (!diff)
	{
		return;
	}
	fprintf(stderr, "[sim] inputs now 0x%08lx (diff 0x%08lx)\n", (unsigned long)v, (unsigned long)diff);
	if (diff & SIM_SPECIAL_LIMIT_BITS)
		mcu_limits_changed_cb();
	if (diff & SIM_SPECIAL_PROBE_BIT)
		mcu_probe_changed_cb();
	if (diff & SIM_SPECIAL_CONTROL_BITS)
		mcu_controls_changed_cb();
}

static void print_banner(int port)
{
	fprintf(stderr,
			"\n"
			"uCNC Linux simulator (my_cnc branch) listening on 127.0.0.1:%d\n"
			"Connect the SimpleCNC server (CNC_TRANSPORT=tcp) or:  nc 127.0.0.1 %d\n"
			"Simulator console commands (stdin):\n"
			"  probe 0|1            set probe contact\n"
			"  limit X|Y|Z|A|B 0|1  set a limit switch\n"
			"  estop 0|1 | fhold 0|1 | door 0|1 | reset 0|1\n"
			"  status               dump pin state\n"
			"  quit                 stop the simulator\n"
			"\n",
			port, port);
}

static void sim_console_status(void)
{
	uint32_t s = virtualmap.special_inputs;
	fprintf(stderr, "[sim] special_inputs=0x%08lx probe=%d limits(X Y Z A B)=%d%d%d%d%d controls(R D H S)=%d%d%d%d\n",
			(unsigned long)s,
			!!(s & SIM_SPECIAL_PROBE_BIT),
			!!(s & 0x001UL), !!(s & 0x002UL), !!(s & 0x004UL), !!(s & 0x040UL), !!(s & 0x080UL),
			!!(s & 0x0400UL), !!(s & 0x0800UL), !!(s & 0x1000UL), !!(s & 0x2000UL));
}

static void *sim_console_thread(void *arg)
{
	(void)arg;
	char line[256];
	for (;;)
	{
		if (!fgets(line, sizeof(line), stdin))
		{
			// EOF: keep running (client may still be connected)
			struct timespec ts = {.tv_sec = 1, .tv_nsec = 0};
			nanosleep(&ts, NULL);
			clearerr(stdin);
			continue;
		}
		// strip trailing newline
		line[strcspn(line, "\r\n")] = '\0';

		if (!strncmp(line, "probe ", 6))
		{
			int v = atoi(line + 6) ? 1 : 0;
			if (v)
				sim_stage_inputs(SIM_SPECIAL_PROBE_BIT, 0);
			else
				sim_stage_inputs(0, SIM_SPECIAL_PROBE_BIT);
			fprintf(stderr, "[sim] probe=%d (staged)\n", v);
		}
		else if (!strncmp(line, "limit ", 6))
		{
			char axis = '?';
			int v = 0;
			if (sscanf(line + 6, "%c %d", &axis, &v) == 2)
			{
				uint32_t bit = 0;
				switch (axis)
				{
				case 'X':
				case 'x':
					bit = 0x001UL;
					break;
				case 'Y':
				case 'y':
					bit = 0x002UL;
					break;
				case 'Z':
				case 'z':
					bit = 0x004UL;
					break;
				case 'A':
				case 'a':
					bit = 0x040UL;
					break;
				case 'B':
				case 'b':
					bit = 0x080UL;
					break;
				default:
					fprintf(stderr, "[sim] unknown axis '%c' (use X Y Z A B)\n", axis);
					continue;
				}
				if (v)
					sim_stage_inputs(bit, 0);
				else
					sim_stage_inputs(0, bit);
				fprintf(stderr, "[sim] limit %c=%d (staged)\n", axis, !!v);
			}
			else
			{
				fprintf(stderr, "[sim] usage: limit X|Y|Z|A|B 0|1\n");
			}
		}
		else if (!strncmp(line, "estop ", 6))
		{
			int v = atoi(line + 6) ? 1 : 0;
			if (v)
				sim_stage_inputs(0x0400UL, 0);
			else
				sim_stage_inputs(0, 0x0400UL);
			fprintf(stderr, "[sim] estop=%d (staged)\n", v);
		}
		else if (!strncmp(line, "fhold ", 6))
		{
			int v = atoi(line + 6) ? 1 : 0;
			if (v)
				sim_stage_inputs(0x1000UL, 0);
			else
				sim_stage_inputs(0, 0x1000UL);
			fprintf(stderr, "[sim] fhold=%d (staged)\n", v);
		}
		else if (!strncmp(line, "door ", 5))
		{
			int v = atoi(line + 5) ? 1 : 0;
			if (v)
				sim_stage_inputs(0x0800UL, 0);
			else
				sim_stage_inputs(0, 0x0800UL);
			fprintf(stderr, "[sim] door=%d (staged)\n", v);
		}
		else if (!strncmp(line, "reset ", 6))
		{
			int v = atoi(line + 6) ? 1 : 0;
			if (v)
				sim_stage_inputs(0x2000UL, 0);
			else
				sim_stage_inputs(0, 0x2000UL);
			fprintf(stderr, "[sim] cs_res=%d (staged)\n", v);
		}
		else if (!strcmp(line, "status"))
		{
			sim_console_status();
		}
		else if (!strcmp(line, "help"))
		{
			print_banner(0);
		}
		else if (!strcmp(line, "quit") || !strcmp(line, "exit"))
		{
			fprintf(stderr, "[sim] quitting\n");
			exit(0);
		}
		else if (line[0] != '\0')
		{
			fprintf(stderr, "[sim] unknown command '%s' (try: probe, limit, estop, fhold, door, reset, status, quit)\n", line);
		}
	}
	return NULL;
}

uint8_t mcu_get_pin_offset(uint8_t pin)
{
	if (pin >= 1 && pin <= 24)
	{
		return pin - 1;
	}
	else if (pin >= 47 && pin <= 78)
	{
		return pin - 47;
	}
	if (pin >= 100 && pin <= 113)
	{
		return pin - 100;
	}
	else if (pin >= 130 && pin <= 161)
	{
		return pin - 130;
	}

	return (uint8_t)-1;
}

void mcu_config_input(uint8_t pin)
{
	(void)pin;
}

// NOTE: mcu_config_pullup is #defined to nothing by mcumap_virtual.h, so no
// function definition here (same as the Windows mcu_virtual.cpp).

void mcu_config_pwm(uint8_t pin, uint16_t freq)
{
	(void)pin;
	(void)freq;
}

uint8_t mcu_get_input(uint8_t pin)
{
	uint8_t offset = mcu_get_pin_offset(pin);
	if (offset > 31)
	{
		return 0;
	}
	uint32_t s = virtualmap.special_inputs;
	uint32_t inputs = virtualmap.inputs;
	if (pin >= DIN0)
	{
		return (inputs & (1UL << offset)) ? 1 : 0;
	}
	else
	{
		return (s & (1UL << offset)) ? 1 : 0;
	}

	return 0;
}

void mcu_config_output(uint8_t pin)
{
	(void)pin;
}

/**
 * gets the value of a digital output pin
 * can be defined either as a function or a macro call
 * */
uint8_t mcu_get_output(uint8_t pin)
{
	uint8_t offset = mcu_get_pin_offset(pin);
	if (offset > 31)
	{
		return 0;
	}

	uint32_t outputs = virtualmap.outputs;
	uint32_t special = virtualmap.special_outputs;

	if (pin >= DOUT0)
	{
		return (outputs & (1UL << offset)) ? 1 : 0;
	}
	else
	{
		return (special & (1UL << offset)) ? 1 : 0;
	}

	return 0;
}

/**
 * sets the value of a digital output pin to logical 1
 * can be defined either as a function or a macro call
 * */
void mcu_set_output(uint8_t pin)
{
	uint8_t offset = mcu_get_pin_offset(pin);
	if (offset > 31)
	{
		return;
	}

	if (pin >= DOUT0)
	{
		virtualmap.outputs |= (1UL << offset);
	}
	else
	{
		virtualmap.special_outputs |= (1UL << offset);
	}
}

/**
 * sets the value of a digital output pin to logical 0
 * can be defined either as a function or a macro call
 * */
void mcu_clear_output(uint8_t pin)
{
	uint8_t offset = mcu_get_pin_offset(pin);
	if (offset > 31)
	{
		return;
	}

	if (pin >= DOUT0)
	{
		virtualmap.outputs &= ~(1UL << offset);
	}
	else
	{
		virtualmap.special_outputs &= ~(1UL << offset);
	}
}

/**
 * toggles the value of a digital output pin
 * can be defined either as a function or a macro call
 * */
void mcu_toggle_output(uint8_t pin)
{
	uint8_t offset = mcu_get_pin_offset(pin);
	if (offset > 31)
	{
		return;
	}

	if (pin >= DOUT0)
	{
		virtualmap.outputs ^= (1UL << offset);
	}
	else
	{
		virtualmap.special_outputs ^= (1UL << offset);
	}
}

uint16_t mcu_get_analog(uint8_t channel)
{
	(void)channel;
	uint8_t v = 0;
	if (channel >= ANALOG0 && channel < ANALOG0 + 16)
	{
		v = virtualmap.analog[channel - ANALOG0];
	}
	return v;
}

// Outputs
void mcu_set_pwm(uint8_t pwm, uint8_t value)
{
	if (pwm >= PWM0 && pwm < PWM0 + 16)
	{
		virtualmap.pwm[pwm - PWM0] = value;
	}
}

uint8_t mcu_get_pwm(uint8_t pwm)
{
	uint8_t v = 0;
	if (pwm >= PWM0 && pwm < PWM0 + 16)
	{
		v = virtualmap.pwm[pwm - PWM0];
	}
	return v;
}

void mcu_set_servo(uint8_t servo, uint8_t value)
{
	if (servo >= SERVO0 && servo < SERVO0 + 6)
	{
		virtualmap.servos[servo - SERVO0] = value;
	}
}

uint8_t mcu_get_servo(uint8_t servo)
{
	uint8_t v = 0;
	if (servo >= SERVO0 && servo < SERVO0 + 6)
	{
		v = virtualmap.servos[servo - SERVO0];
	}
	return v;
}

void mcu_enable_probe_isr(void)
{
}
void mcu_disable_probe_isr(void)
{
}

/**
 *
 * Interpolator timer emulation (ported from mcu_virtual.cpp)
 *
 * **/

#ifndef ITP_SAMPLE_RATE
#define ITP_SAMPLE_RATE (F_STEP_MAX * 2)
#endif

#if defined(MCU_HAS_ONESHOT_TIMER)
static uint32_t virtual_oneshot_counter;
static uint32_t virtual_oneshot_reload;
static FORCEINLINE void mcu_gen_oneshot(void)
{
	if (virtual_oneshot_counter)
	{
		virtual_oneshot_counter--;
		if (!virtual_oneshot_counter)
		{
			if (mcu_timeout_cb)
			{
				mcu_timeout_cb();
			}
		}
	}
}
#endif

static volatile uint32_t mcu_itp_timer_reload;
static volatile bool mcu_itp_timer_running;
static unsigned long sim_step_fires;
static unsigned long sim_step_batches;
static FORCEINLINE void mcu_gen_step(void)
{
	static bool step_reset = true;
	static int32_t mcu_itp_timer_counter;

	// generate steps
	if (mcu_itp_timer_running)
	{
		// stream mode tick
		int32_t t = mcu_itp_timer_counter;
		bool reset = step_reset;
		t -= (int32_t)roundf(1000000.0f / (float)ITP_SAMPLE_RATE);
		if (t <= 0)
		{
			sim_step_fires++;
			if (!reset)
			{
				mcu_step_cb();
			}
			else
			{
				mcu_step_reset_cb();
			}
			step_reset = !reset;
			mcu_itp_timer_counter = mcu_itp_timer_reload + t;
		}
		else
		{
			mcu_itp_timer_counter = t;
		}
	}
}

/**
 * convert step rate to clock cycles
 * */
void mcu_freq_to_clocks(float frequency, uint16_t *ticks, uint16_t *prescaller)
{
	frequency = CLAMP((float)F_STEP_MIN, frequency, (float)F_STEP_MAX);
	// up and down counter (generates half the step rate at each event)
	uint32_t totalticks = (uint32_t)((500000.0f) / frequency);
	*prescaller = 1;
	while (totalticks > 0xFFFF)
	{
		(*prescaller) <<= 1;
		totalticks >>= 1;
	}

	*ticks = (uint16_t)totalticks;
}

float mcu_clocks_to_freq(uint16_t ticks, uint16_t prescaller)
{
	uint32_t totalticks = (uint32_t)ticks * prescaller;
	return 500000.0f / ((float)totalticks);
}

/**
 * starts the timer interrupt that generates the step pulses for the interpolator
 * */

void mcu_start_itp_isr(uint16_t ticks, uint16_t prescaller)
{
	if (getenv("SIM_ISR_DEBUG"))
		fprintf(stderr, "[isr] start ticks=%u presc=%u running=%d\n", ticks, prescaller, (int)mcu_itp_timer_running);
	if (!mcu_itp_timer_running)
	{
		mcu_itp_timer_reload = ticks * prescaller;
		mcu_itp_timer_running = true;
	}
	else
	{
		mcu_change_itp_isr(ticks, prescaller);
	}
}

/**
 * changes the step rate of the timer interrupt that generates the step pulses for the interpolator
 * */
void mcu_change_itp_isr(uint16_t ticks, uint16_t prescaller)
{
	if (getenv("SIM_ISR_DEBUG"))
		fprintf(stderr, "[isr] change ticks=%u presc=%u running=%d\n", ticks, prescaller, (int)mcu_itp_timer_running);
	if (mcu_itp_timer_running)
	{
		mcu_itp_timer_reload = ticks * prescaller;
	}
	else
	{
		mcu_start_itp_isr(ticks, prescaller);
	}
}

/**
 * stops the timer interrupt that generates the step pulses for the interpolator
 * */
void mcu_stop_itp_isr(void)
{
	if (getenv("SIM_ISR_DEBUG"))
		fprintf(stderr, "[isr] stop running=%d\n", (int)mcu_itp_timer_running);
	if (mcu_itp_timer_running)
	{
		mcu_itp_timer_running = false;
	}
}

/**
 * configures a single shot timeout in us
 * */
static uint32_t oneshot_timeout;
static uint32_t oneshot_alarm;
static bool oneshot_armed;
void mcu_config_timeout(mcu_timeout_delgate fp, uint32_t timeout)
{
	oneshot_timeout = timeout;
	mcu_timeout_cb = fp;
}

/**
 * starts the timeout. Once hit the the respective callback is called
 * */
void mcu_start_timeout(void)
{
	oneshot_alarm = mcu_micros() + oneshot_timeout;
	oneshot_armed = true;
}

static void ticksimul_1ms(void)
{
	if (oneshot_armed && mcu_timeout_cb)
	{
		// unsigned wraparound-safe comparison
		if ((int32_t)(mcu_micros() - oneshot_alarm) >= 0)
		{
			oneshot_armed = false;
			mcu_timeout_cb();
		}
	}

	mcu_rtc_cb(mcu_millis());
}

// (step-timer events are generated inline in sim_tick_poll, below)

// Time-sliced "ISR" polling, called from mcu_dotasks() on the single core
// thread. Runs the RTC callback at ~1kHz and the step-timer events at the
// hardware cadence, matching what real timer interrupts deliver.
// Because everything runs on one thread, no exclusion vs the main loop is
// needed (this was the source of deadlocks in the earlier pthread design).
//
// Catch-up: the workload per dotasks iteration varies (itp_run, socket
// IO), so a fixed 20ms batch per poll would run sim-time slower than the
// wall clock whenever iterations exceed 20ms. Instead, ELAPSED wall time
// is converted into events, so sim-time tracks wall-time regardless of
// host scheduling jitter. Elapsed is capped to keep pauses/breakpoints
// from injecting huge jumps.
static uint64_t sim_last_poll_us;
static unsigned long sim_poll_count;
static unsigned long sim_poll_last_report;

static void sim_tick_poll(void)
{
	// NOTE: deliberately NOT gated on global_isr_enabled. mcu_step_cb()
	// ends with the ISR flag disabled by design (it runs inside the step
	// interrupt on hardware, where the next RTC interrupt re-enables it
	// via mcu_rtc_cb). Gating the poll on the flag would stall all ticks
	// after the first step batch. Single-threaded, the flag is only
	// save/restore bookkeeping for __ATOMIC__ blocks (nothing preempts),
	// so polling unconditionally is both safe and required.
	uint64_t now = sim_now_us();
	uint64_t elapsed = now - sim_last_poll_us;
	sim_last_poll_us = now;
	if (elapsed == 0)
	{
		return;
	}
	if (elapsed > 1000000)
	{
		elapsed = 1000000; // cap catch-up at 1s of sim time
	}
	sim_poll_count++;
	if (getenv("SIM_TICK_DEBUG") && sim_poll_count - sim_poll_last_report >= 20000)
	{
		sim_poll_last_report = sim_poll_count;
		fprintf(stderr, "[tick] polls=%lu t=%.1fs itprunning=%d reload=%lu\n",
			sim_poll_count, now / 1e6,
			(int)mcu_itp_timer_running, (unsigned long)mcu_itp_timer_reload);
	}
	// RTC at 1kHz
	uint64_t rtc_ticks = elapsed / 1000;
	for (uint64_t i = 0; i < rtc_ticks; i++)
	{
		ticksimul_1ms();
	}
	// Step-timer events proportional to elapsed wall time
	uint64_t iters = elapsed * (uint64_t)ITP_SAMPLE_RATE / 1000000ULL;
	for (uint64_t i = 0; i < iters; i++)
	{
		mcu_gen_step();
	}
	sim_step_batches++;
	if (getenv("SIM_TICK_DEBUG") && (sim_step_batches % 250) == 0)
	{
		fprintf(stderr, "[steps] t=%.1fs batches=%lu fires=%lu running=%d reload=%lu\n",
			sim_now_us() / 1e6, sim_step_batches, sim_step_fires,
			(int)mcu_itp_timer_running, (unsigned long)mcu_itp_timer_reload);
	}
}

/**
 * Initialize the MCU
 * **/
void mcu_init(void)
{
	memset((void *)&virtualmap, 0, sizeof(virtualmap));
	sim_last_poll_us = sim_now_us();
	mcu_enable_global_isr();

	pthread_t console_tid;
	pthread_create(&console_tid, NULL, sim_console_thread, NULL);
	pthread_detach(console_tid);
}

static int sim_tcp_listen(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		perror("socket");
		return -1;
	}
	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons((uint16_t)port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		close(fd);
		return -1;
	}
	if (listen(fd, 1) < 0)
	{
		perror("listen");
		close(fd);
		return -1;
	}
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	return fd;
}

static void sim_prof_handler(int sig)
{
	(void)sig;
	void *frames[24];
	int n = backtrace(frames, 24);
	char msg[] = "[prof] main-thread sample:\n";
	write(STDERR_FILENO, msg, sizeof(msg) - 1);
	backtrace_symbols_fd(frames, n, STDERR_FILENO);
}

int main(int argc, char **argv)
{
	int port = SIM_TCP_PORT_DEFAULT;
	const char *env_port = getenv("UCNC_SIM_PORT");
	if (env_port && env_port[0])
	{
		port = atoi(env_port);
	}
	for (int i = 1; i < argc; i++)
	{
		if ((!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port")) && i + 1 < argc)
		{
			port = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
		{
			fprintf(stderr, "usage: %s [-p PORT]  (or UCNC_SIM_PORT env)\n", argv[0]);
			return 0;
		}
	}

	sim_listen_fd = sim_tcp_listen(port);
	if (sim_listen_fd < 0)
	{
		return 1;
	}
	if (getenv("SIM_PROFILE"))
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sim_prof_handler;
		sa.sa_flags = SA_RESTART;
		sigaction(SIGALRM, &sa, NULL);
		struct itimerval it = {.it_interval = {.tv_sec = 1, .tv_usec = 0},
							 .it_value = {.tv_sec = 1, .tv_usec = 0}};
		setitimer(ITIMER_REAL, &it, NULL);
		fprintf(stderr, "[sim] profiler on (SIGALRM 1Hz)\n");
	}
	print_banner(port);
	cnc_init();
	for (;;)
	{
		cnc_run();
	}
	return 0;
}

uint8_t itp_set_step_mode(uint8_t mode)
{
	(void)mode;
	return 0;
}

/**
 *Solve compiler issues
 */
void mcu_io_reset(void)
{
}
void nvm_start_read(uint16_t address)
{
	(void)address;
}
void nvm_start_write(uint16_t address)
{
	(void)address;
}
uint8_t nvm_getc(uint16_t address) { return mcu_eeprom_getc(address); }
void nvm_putc(uint16_t address, uint8_t c) { mcu_eeprom_putc(address, c); }
void nvm_end_read(void) {}
void nvm_end_write(void) { mcu_eeprom_flush(); }
#endif
