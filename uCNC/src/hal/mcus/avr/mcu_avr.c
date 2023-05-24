/*
		Name: mcu_avr.c
		Description: Implements mcu interface on AVR.
				Besides all the functions declared in the mcu.h it also implements the code responsible
				for handling:
						interpolator.h
								void mcu_step_cb();
								void mcu_step_reset_cb();
						serial.h
								void mcu_com_rx_cb(unsinged char c);
								char mcu_com_tx_cb();
						trigger_control.h
								void mcu_limits_changed_cb();
								void mcu_controls_changed_cb();
				void mcu_probe_changed_cb(uint8_t probe);

		Copyright: Copyright (c) João Martins
		Author: João Martins
		Date: 01/11/2019

		µCNC is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version. Please see <http://www.gnu.org/licenses/>

		µCNC is distributed WITHOUT ANY WARRANTY;
		Also without the implied warranty of	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
		See the	GNU General Public License for more details.
*/

#include "../../../cnc.h"

#if (MCU == MCU_AVR)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#ifndef BAUDRATE
#define BAUDRATE 115200
#endif

// define the mcu internal servo variables
#if SERVOS_MASK > 0
static uint8_t mcu_servos_val[6];
static uint8_t mcu_servos[6];
static uint8_t mcu_servos_loops[6];

static FORCEINLINE void mcu_clear_servos()
{
	// disables the interrupt of OCIEB (leaves only OCIEA)
	RTC_TIMSK = (1U << RTC_OCIEA);
	RTC_TIFR = (1U << 2);
#if ASSERT_PIN(SERVO0)
	mcu_clear_output(SERVO0);
#endif
#if ASSERT_PIN(SERVO1)
	mcu_clear_output(SERVO1);
#endif
#if ASSERT_PIN(SERVO2)
	mcu_clear_output(SERVO2);
#endif
#if ASSERT_PIN(SERVO3)
	mcu_clear_output(SERVO3);
#endif
#if ASSERT_PIN(SERVO4)
	mcu_clear_output(SERVO4);
#endif
#if ASSERT_PIN(SERVO5)
	mcu_clear_output(SERVO5);
#endif
}

// naked ISR to reduce impact since doen't need to change any register (just an interrupt mask and pin outputs)
ISR(RTC_COMPB_vect, ISR_NOBLOCK)
{
	mcu_clear_servos();
}
#endif

// gets the mcu running time in ms
static volatile uint32_t mcu_runtime_ms;
ISR(RTC_COMPA_vect, ISR_BLOCK)
{
#if SERVOS_MASK > 0
	static uint8_t ms_servo_counter = 0;
	static uint8_t servo_loops;
	uint8_t servo_counter = ms_servo_counter;

	switch (servo_counter)
	{
#if ASSERT_PIN(SERVO0)
	case SERVO0_FRAME:
		RTC_OCRB = mcu_servos[0];
		servo_loops = mcu_servos_loops[0];
		mcu_set_output(SERVO0);
		break;
#endif
#if ASSERT_PIN(SERVO1)
	case SERVO1_FRAME:
		RTC_OCRB = mcu_servos[1];
		servo_loops = mcu_servos_loops[1];
		mcu_set_output(SERVO1);
		break;
#endif
#if ASSERT_PIN(SERVO2)
	case SERVO2_FRAME:
		RTC_OCRB = mcu_servos[2];
		servo_loops = mcu_servos_loops[2];
		mcu_set_output(SERVO2);
		break;
#endif
#if ASSERT_PIN(SERVO3)
	case SERVO3_FRAME:
		RTC_OCRB = mcu_servos[3];
		servo_loops = mcu_servos_loops[3];
		mcu_set_output(SERVO3);
		break;
#endif
#if ASSERT_PIN(SERVO4)
	case SERVO4_FRAME:
		RTC_OCRB = mcu_servos[4];
		servo_loops = mcu_servos_loops[4];
		mcu_set_output(SERVO4);
		break;
#endif
#if ASSERT_PIN(SERVO5)
	case SERVO5_FRAME:
		RTC_OCRB = mcu_servos[5];
		servo_loops = mcu_servos_loops[5];
		mcu_set_output(SERVO5);
		break;
#endif
	}

	if (!servo_loops--)
	{
		if (!RTC_OCRB)
		{
			mcu_clear_servos();
		}
		else
		{
			RTC_TIFR = 7;
			RTC_TIMSK |= (1 << RTC_OCIEB);
		}
	}
	servo_counter++;
	ms_servo_counter = (servo_counter != 20) ? servo_counter : 0;

#endif
	uint32_t millis = mcu_runtime_ms;
	millis++;
	mcu_runtime_ms = millis;
	mcu_rtc_cb(millis);
}

ISR(ITP_COMPA_vect, ISR_BLOCK)
{
	mcu_step_cb();
}

ISR(ITP_COMPB_vect, ISR_BLOCK)
{
	mcu_step_reset_cb();
}

#ifndef FORCE_SOFT_POLLING

#if (PCINTA_MASK & 1)
ISR(INT0_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTA_LIMITS_MASK & 1)
	mcu_limits_changed_cb();
#endif
#if (PCINTA_CONTROLS_MASK & 1)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRA & 1)
	mcu_probe_changed_cb();
#endif
#if (PCINTA_DIN_IO_MASK & 1)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTA_MASK & 4)
ISR(INT1_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTA_LIMITS_MASK & 4)
	mcu_limits_changed_cb();
#endif
#if (PCINTA_CONTROLS_MASK & 4)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRA & 4)
	mcu_probe_changed_cb();
#endif
#if (PCINTA_DIN_IO_MASK & 4)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTA_MASK & 16)
ISR(INT2_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTA_LIMITS_MASK & 16)
	mcu_limits_changed_cb();
#endif
#if (PCINTA_CONTROLS_MASK & 16)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRA & 16)
	mcu_probe_changed_cb();
#endif
#if (PCINTA_DIN_IO_MASK & 16)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTA_MASK & 64)
ISR(INT3_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTA_LIMITS_MASK & 64)
	mcu_limits_changed_cb();
#endif
#if (PCINTA_CONTROLS_MASK & 64)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRA & 64)
	mcu_probe_changed_cb();
#endif
#if (PCINTA_DIN_IO_MASK & 64)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTB_MASK & 1)
ISR(INT4_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTB_LIMITS_MASK & 1)
	mcu_limits_changed_cb();
#endif
#if (PCINTB_CONTROLS_MASK & 1)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRB & 1)
	mcu_probe_changed_cb();
#endif
#if (PCINTB_DIN_IO_MASK & 1)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTB_MASK & 4)
ISR(INT5_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTB_LIMITS_MASK & 4)
	mcu_limits_changed_cb();
#endif
#if (PCINTB_CONTROLS_MASK & 4)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRB & 4)
	mcu_probe_changed_cb();
#endif
#if (PCINTB_DIN_IO_MASK & 4)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTB_MASK & 16)
ISR(INT6_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTB_LIMITS_MASK & 16)
	mcu_limits_changed_cb();
#endif
#if (PCINTB_CONTROLS_MASK & 16)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRB & 16)
	mcu_probe_changed_cb();
#endif
#if (PCINTB_DIN_IO_MASK & 16)
	mcu_inputs_changed_cb();
#endif
}
#endif
#if (PCINTB_MASK & 64)
ISR(INT7_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINTB_LIMITS_MASK & 64)
	mcu_limits_changed_cb();
#endif
#if (PCINTB_CONTROLS_MASK & 64)
	mcu_controls_changed_cb();
#endif
#if (PROBE_ISRB & 64)
	mcu_probe_changed_cb();
#endif
#if (PCINTB_DIN_IO_MASK & 64)
	mcu_inputs_changed_cb();
#endif
}
#endif

#if (PCINT0_MASK != 0)
ISR(PCINT0_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINT0_LIMITS_MASK != 0)
	mcu_limits_changed_cb();
#endif
#if (PCINT0_CONTROLS_MASK != 0)
	mcu_controls_changed_cb();
#endif

#if (PROBE_ISR0 != 0)
	mcu_probe_changed_cb();
#endif

#if (PCINT0_DIN_IO_MASK != 0)
	mcu_inputs_changed_cb();
#endif
}
#endif

#if (PCINT1_MASK != 0)
ISR(PCINT1_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINT1_LIMITS_MASK != 0)
	mcu_limits_changed_cb();
#endif
#if (PCINT1_CONTROLS_MASK != 0)
	mcu_controls_changed_cb();

#endif

#if (PROBE_ISR1 != 0)
	mcu_probe_changed_cb();

#endif

#if (PCINT1_DIN_IO_MASK != 0)
	mcu_inputs_changed_cb();
#endif
}
#endif

#if (PCINT2_MASK != 0)
ISR(PCINT2_vect, ISR_BLOCK) // input pin on change service routine
{
#if (PCINT2_LIMITS_MASK != 0)
	mcu_limits_changed_cb();

#endif
#if (PCINT2_CONTROLS_MASK != 0)
	mcu_controls_changed_cb();

#endif

#if (PROBE_ISR2 != 0)
	mcu_probe_changed_cb();

#endif

#if (PCINT2_DIN_IO_MASK != 0)
	mcu_inputs_changed_cb();

#endif
}
#endif

#endif

#ifdef MCU_HAS_UART
ISR(COM_RX_vect, ISR_BLOCK)
{
	mcu_com_rx_cb(COM_INREG);
}
#ifndef ENABLE_SYNC_TX
ISR(COM_TX_vect, ISR_BLOCK)
{
	CLEARBIT(UCSRB_REG, UDRIE_BIT);
	mcu_com_tx_cb();
}
#endif
#endif

#if defined(MCU_HAS_UART2)
ISR(COM2_RX_vect, ISR_BLOCK)
{
#if !defined(UART2_DETACH_MAIN_PROTOCOL)
	mcu_com_rx_cb(COM2_INREG);
#else
#ifdef UART2_PASSTHROUGH
	mcu_uart_putc(COM2_INREG);
#endif
	mcu_uart_rx_cb(COM2_INREG);
#endif
}
#ifndef ENABLE_SYNC_TX
ISR(COM2_TX_vect, ISR_BLOCK)
{
	CLEARBIT(UCSRB_REG_2, UDRIE_BIT_2);
	mcu_com_tx_cb();
}
#endif
#endif

static void mcu_start_rtc();

void mcu_init(void)
{
	// disable WDT
	wdt_reset();
	MCUSR &= ~(1 << WDRF);
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	WDTCSR = 0x00;

	// configure all pins
	// autogenerated
	mcu_io_init();

	// Set COM port
	//  Set baud rate
	uint16_t UBRR_value;
#ifdef MCU_HAS_UART
#if BAUDRATE < 57600
	UBRR_value = (F_CPU / (16UL * BAUDRATE)) - 1;
	UCSRA_REG &= ~(1 << U2X_BIT); // baud doubler off  - Only needed on Uno XXX
#else
	UBRR_value = (F_CPU / (8UL * BAUDRATE)) - 1;
	UCSRA_REG |= (1 << U2X_BIT); // baud doubler on for high baud rates, i.e. 115200
#endif
	UBRRH_REG = UBRR_value >> 8;
	UBRRL_REG = UBRR_value;

	// enable rx, tx, and interrupt on complete reception of a byte and UDR empty
	UCSRB_REG |= (1 << RXEN_BIT | 1 << TXEN_BIT | 1 << RXCIE_BIT);
#endif

#ifdef MCU_HAS_UART2
#if BAUDRATE2 < 57600
	UBRR_value = (F_CPU / (16UL * BAUDRATE2)) - 1;
	UCSRA_REG_2 &= ~(1 << U2X_BIT_2); // baud doubler off  - Only needed on Uno XXX
#else
	UBRR_value = (F_CPU / (8UL * BAUDRATE2)) - 1;
	UCSRA_REG_2 |= (1 << U2X_BIT_2); // baud doubler on for high baud rates, i.e. 115200
#endif
	UBRRH_REG_2 = UBRR_value >> 8;
	UBRRL_REG_2 = UBRR_value;

	// enable rx, tx, and interrupt on complete reception of a byte and UDR empty
	UCSRB_REG_2 |= (1 << RXEN_BIT_2 | 1 << TXEN_BIT_2 | 1 << RXCIE_BIT_2);
#endif

// enable interrupts on pin changes
#ifndef FORCE_SOFT_POLLING
#if ((PCINT0_LIMITS_MASK | PCINT0_CONTROLS_MASK | PROBE_ISR0 | PCINT0_DIN_IO_MASK) != 0)
	SETBIT(PCICR, PCIE0);
#else
	CLEARBIT(PCICR, PCIE0);
#endif
#if ((PCINT1_LIMITS_MASK | PCINT1_CONTROLS_MASK | PROBE_ISR1 | PCINT1_DIN_IO_MASK) != 0)
#ifdef AVR6
	PCMSK1 <<= 1;
#endif
	SETBIT(PCICR, PCIE1);
#else
	CLEARBIT(PCICR, PCIE1);
#endif
#if ((PCINT2_LIMITS_MASK | PCINT2_CONTROLS_MASK | PROBE_ISR2 | PCINT2_DIN_IO_MASK) != 0)
	SETBIT(PCICR, PCIE2);
#else
	CLEARBIT(PCICR, PCIE2);
#endif
#if (EIMSK_VAL != 0)
	EIMSK = EIMSK_VAL;
#endif
#endif

	mcu_start_rtc();

#if SERVOS_MASK > 0
	uint8_t i = (RTC_OCRA >> 1);
	memset(mcu_servos, i, 6);
#endif

#ifdef MCU_HAS_SPI
	// enable SPI, set as master, and clock to fosc/128
	SPSR |= SPSR_VAL;
	SPCR = (1 << SPE) | (1 << MSTR) | (SPI_MODE << 2) | SPCR_VAL;
#endif

#ifdef MCU_HAS_I2C
	// configure as I2C master
	mcu_i2c_config(I2C_FREQ);
#endif

	// disable probe isr
	mcu_disable_probe_isr();
	// enable interrupts
	mcu_enable_global_isr();
}

// IO functions
void mcu_set_servo(uint8_t servo, uint8_t value)
{
#if SERVOS_MASK > 0
	uint8_t scaled = RTC_OCRA;
	mcu_servos_val[servo - SERVO_PINS_OFFSET] = value;
	if (value < 64)
	{
		mcu_servos_loops[servo - SERVO_PINS_OFFSET] = 0;
		scaled >>= 1;
		scaled = (uint8_t)(((uint16_t)(value * scaled)) >> 6) + scaled;
	}
	else if (value < 192)
	{
		value -= 64;
		mcu_servos_loops[servo - SERVO_PINS_OFFSET] = 1;
		scaled = (uint8_t)(((uint16_t)(value * scaled)) >> 7);
	}
	else
	{
		value -= 192;
		mcu_servos_loops[servo - SERVO_PINS_OFFSET] = 2;
		scaled >>= 1;
		scaled = (uint8_t)(((uint16_t)(value * scaled)) >> 6);
	}
	mcu_servos[servo - SERVO_PINS_OFFSET] = scaled;
#endif
}

/**
 * gets the pwm for a servo (50Hz with tON between 1~2ms)
 * can be defined either as a function or a macro call
 * */
uint8_t mcu_get_servo(uint8_t servo)
{
#if SERVOS_MASK > 0
	uint8_t offset = servo - SERVO_PINS_OFFSET;
	if ((1U << offset) & SERVOS_MASK)
	{
		return mcu_servos_val[offset];
	}
#endif
	return 0;
}

void mcu_putc(char c)
{
#ifdef MCU_HAS_UART
#ifdef ENABLE_SYNC_TX
	loop_until_bit_is_set(UCSRA_REG, UDRE_BIT);
#endif
	COM_OUTREG = c;
#ifndef ENABLE_SYNC_TX
	SETBIT(UCSRB_REG, UDRIE_BIT);
#endif
#endif
#if (defined(MCU_HAS_UART2) && !defined(UART2_DETACH_MAIN_PROTOCOL))
#ifdef ENABLE_SYNC_TX
	loop_until_bit_is_set(UCSRA_REG_2, UDRE_BIT);
#endif
	COM2_OUTREG = c;
#ifndef ENABLE_SYNC_TX
	SETBIT(UCSRB_REG_2, UDRIE_BIT_2);
#endif
#endif
}

// RealTime
void mcu_freq_to_clocks(float frequency, uint16_t *ticks, uint16_t *prescaller)
{
	if (frequency < F_STEP_MIN)
		frequency = F_STEP_MIN;
	if (frequency > F_STEP_MAX)
		frequency = F_STEP_MAX;

	uint32_t clocks = (uint32_t)floorf((float)F_CPU / frequency);
	*prescaller = (1 << 3); // CTC mode

#if (ITP_TIMER == 2)
	if (clocks <= ((1UL << 16) - 1))
	{
		*prescaller |= 1;
	}
	else if (clocks <= ((1UL << 19) - 1))
	{
		clocks >>= 3;
		*prescaller |= 2;
	}
	else if (clocks <= ((1UL << 21) - 1))
	{
		clocks >>= 5;
		*prescaller |= 3;
	}
	else if (clocks <= ((1UL << 22) - 1))
	{
		clocks >>= 6;
		*prescaller |= 4;
	}
	else if (clocks <= ((1UL << 23) - 1))
	{
		clocks >>= 7;
		*prescaller |= 5;
	}
	else if (clocks <= ((1UL << 24) - 1))
	{
		clocks >>= 8;
		*prescaller |= 6;
	}
	else
	{
		clocks >>= 10;
		*prescaller |= 7;
	}
#else
	if (clocks <= ((1UL << 16) - 1))
	{
		*prescaller |= 1;
	}
	else if (clocks <= ((1UL << 19) - 1))
	{
		clocks >>= 3;
		*prescaller |= 2;
	}
	else if (clocks <= ((1UL << 22) - 1))
	{
		clocks >>= 6;
		*prescaller |= 3;
	}
	else if (clocks <= ((1UL << 24) - 1))
	{
		clocks >>= 8;
		*prescaller |= 4;
	}
	else
	{
		clocks >>= 10;
		*prescaller |= 5;
	}
#endif
	clocks--;
	*ticks = (uint16_t)MIN(clocks, 0xFFFF);
}

float mcu_clocks_to_freq(uint16_t ticks, uint16_t prescaller)
{
	float freq;
#if (ITP_TIMER == 2)
	switch (prescaller & 0x07)
	{
	case 1:
		freq = (float)F_CPU;
		break;
	case 2:
		freq = (float)(F_CPU >> 3);
		break;
	case 3:
		freq = (float)(F_CPU >> 5);
		break;
	case 4:
		freq = (float)(F_CPU >> 6);
		break;
	case 5:
		freq = (float)(F_CPU >> 7);
		break;
	case 6:
		freq = (float)(F_CPU >> 8);
		break;
	case 7:
		freq = (float)(F_CPU >> 10);
		break;
	default:
		return 0;
	}
#else
	switch (prescaller & 0x07)
	{
	case 1:
		freq = (float)F_CPU;
		break;
	case 2:
		freq = (float)(F_CPU >> 3);
		break;
	case 3:
		freq = (float)(F_CPU >> 6);
		break;
	case 4:
		freq = (float)(F_CPU >> 8);
		break;
	case 5:
		freq = (float)(F_CPU >> 10);
		break;
	default:
		return 0;
	}
#endif

	return (freq / (float)(ticks + 1));
}
/*
		initializes the pulse ISR
		In Arduino this is done in TIMER1
		The frequency range is from 4Hz to F_PULSE
*/
void mcu_start_itp_isr(uint16_t clocks_speed, uint16_t prescaller)
{
	// stops timer
	ITP_TCCRB = 0;
	// CTC mode
	ITP_TCCRA = 0;
	// resets counter
	ITP_TCNT = 0;
	// set step clock
	ITP_OCRA = clocks_speed;
	// sets OCR0B to half
	// this will allways fire step_reset between pulses
	ITP_OCRB = ITP_OCRA >> 1;
	// clears interrupt flags by writing 1's
	ITP_TIFR = 7;
	// enable timer interrupts on both match registers
	ITP_TIMSK |= (1 << ITP_OCIEB) | (1 << ITP_OCIEA);

	// start timer in CTC mode with the correct prescaler
	ITP_TCCRB = (uint8_t)prescaller;
}

// se implementar amass deixo de necessitar de prescaler
void mcu_change_itp_isr(uint16_t clocks_speed, uint16_t prescaller)
{
	// stops timer
	// ITP_TCCRB = 0;
	ITP_OCRB = clocks_speed >> 1;
	ITP_OCRA = clocks_speed;
	// sets OCR0B to half
	// this will allways fire step_reset between pulses

	// reset timer
	// ITP_TCNT = 0;
	// start timer in CTC mode with the correct prescaler
	ITP_TCCRB = (uint8_t)prescaller;
}

void mcu_stop_itp_isr(void)
{
	ITP_TCCRB = 0;
	ITP_TIMSK &= ~((1 << ITP_OCIEB) | (1 << ITP_OCIEA));
}

// gets the mcu running time in ms
uint32_t mcu_millis()
{
	uint32_t val = mcu_runtime_ms;
	return val;
}

uint32_t mcu_micros()
{
	uint32_t rtc_elapsed = RTC_TCNT;
	uint32_t ms = mcu_runtime_ms;

	rtc_elapsed = ((rtc_elapsed * 1000) / RTC_OCRA) + (ms * 1000);
	return rtc_elapsed;
}

void mcu_start_rtc()
{
#if (F_CPU <= 16000000UL)
	uint8_t clocks = ((F_CPU / 1000) >> 6) - 1;
#else
	uint8_t clocks = ((F_CPU / 1000) >> 8) - 1;
#endif
	// stops timer
	RTC_TCCRB = 0;
	RTC_TCCRA = 0;
	// resets counter
	RTC_TCNT = 0;
	// set step clock
	RTC_OCRA = clocks;
	// CTC mode
	RTC_TCCRA |= 2;
	// clears interrupt flags by writing 1's
	RTC_TIFR = 7;
	// enable timer interrupts on both match registers
	RTC_TIMSK |= (1 << RTC_OCIEA);
// start timer in CTC mode with the correct prescaler
#if (F_CPU <= 16000000UL)
#if (RTC_TIMER != 2)
	RTC_TCCRB |= 3;
#else
	RTC_TCCRB |= 4;
#endif
#else
#if (RTC_TIMER != 2)
	RTC_TCCRB |= 4;
#else
	RTC_TCCRB |= 6;
#endif
#endif
}

void mcu_dotasks()
{
}

// This was copied from grbl
#ifndef EEPE
#define EEPE EEWE	//!< EEPROM program/write enable.
#define EEMPE EEMWE //!< EEPROM master program/write enable.
#endif

#ifndef SELFPRGEN
#define SELFPRGEN SPMEN
#endif

/* These two are unfortunately not defined in the device include files. */
#define EEPM1 5 //!< EEPROM Programming Mode Bit 1.
#define EEPM0 4 //!< EEPROM Programming Mode Bit 0.

uint8_t mcu_eeprom_getc(uint16_t address)
{
	do
	{

	} while (EECR & (1 << EEPE)); // Wait for completion of previous write.
	EEAR = address;				  // Set EEPROM address register.
	EECR = (1 << EERE);			  // Start EEPROM read operation.
	return EEDR;				  // Return the byte read from EEPROM.
}

void mcu_eeprom_putc(uint16_t address, uint8_t value)
{
	uint8_t old_value; // Old EEPROM value.
	uint8_t diff_mask; // Difference mask, i.e. old value XOR new value.

	cli(); // Ensure atomic operation for the write operation.

	do
	{
	} while (EECR & (1 << EEPE)); // Wait for completion of previous write.

	do
	{
	} while (SPMCSR & (1 << SELFPRGEN)); // Wait for completion of SPM.

	EEAR = address;				   // Set EEPROM address register.
	EECR = (1 << EERE);			   // Start EEPROM read operation.
	old_value = EEDR;			   // Get old EEPROM value.
	diff_mask = old_value ^ value; // Get bit differences.
	// Check if any bits are changed to '1' in the new value.
	if (diff_mask & value)
	{
		// Now we know that _some_ bits need to be erased to '1'.
		// Check if any bits in the new value are '0'.
		if (value != 0xff)
		{
			// Now we know that some bits need to be programmed to '0' also.
			EEDR = value;										 // Set EEPROM data register.
			EECR = ((1 << EEMPE) | (0 << EEPM1) | (0 << EEPM0)); // Erase+Write mode.
			EECR |= (1 << EEPE);								 // Start Erase+Write operation.
		}
		else
		{
			// Now we know that all bits should be erased.
			EECR = ((1 << EEMPE) | (1 << EEPM0)); // Erase-only mode.
			EECR |= (1 << EEPE);				  // Start Erase-only operation.
		}
	}
	else
	{
		// Now we know that _no_ bits need to be erased to '1'.
		// Check if any bits are changed from '1' in the old value.
		if (diff_mask)
		{
			// Now we know that _some_ bits need to the programmed to '0'.
			EEDR = value;						  // Set EEPROM data register.
			EECR = ((1 << EEMPE) | (1 << EEPM1)); // Write-only mode.
			EECR |= (1 << EEPE);				  // Start Write-only operation.
		}
	}

	do
	{
	} while (EECR & (1 << EEPE)); // Wait for completion of previous write before enabling interrupts.

	sei(); // Restore interrupt flag state.
}

void mcu_eeprom_flush()
{
	// do nothing
}

#ifdef MCU_HAS_SPI
#ifndef mcu_spi_xmit
uint8_t mcu_spi_xmit(uint8_t data)
{
	// transmit dummy byte
	SPDR = data;
	// Wait for reception complete
	while (!(SPSR & (1 << SPIF)))
		;
	// return Data Register
	return SPDR;
}
#endif

void mcu_spi_config(uint8_t mode, uint32_t frequency)
{
	mode = CLAMP(0, mode, 4);
	// disable SPI
	uint8_t div = (uint8_t)(F_CPU / frequency);
	uint8_t spsr, spcr;
	if (div < 2)
	{
		spcr = 0;
		spsr = 1;
	}
	else if (div < 4)
	{
		spcr = 0;
		spsr = 0;
	}
	else if (div < 8)
	{
		spcr = 1;
		spsr = 1;
	}
	else if (div < 16)
	{
		spcr = 1;
		spsr = 0;
	}
	else if (div < 32)
	{
		spcr = 2;
		spsr = 1;
	}
	else if (div < 64)
	{
		spcr = 2;
		spsr = 0;
	}
	else
	{
		spcr = 3;
		spsr = 0;
	}

	// clear speed and mode
	SPCR = 0;
	SPSR |= spsr;
	SPCR = (1 << SPE) | (1 << MSTR) | (mode << 2) | spcr;
}
#endif

#ifdef MCU_HAS_I2C
#include <util/twi.h>
static uint8_t mcu_i2c_write(uint8_t data, bool send_start, bool send_stop)
{
	if (send_start)
	{
		// init
		TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
		while (!(TWCR & (1 << TWINT)))
			;
		if (TW_STATUS != TW_START)
		{
			return I2C_NOTOK;
		}
	}

	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)))
		;

	switch (TW_STATUS)
	{
	case TW_MT_SLA_ACK:
	case TW_MT_DATA_ACK:
	case TW_MR_SLA_ACK:
	case TW_MR_DATA_ACK:
		break;
	default:
		TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
		while (TWCR & (1 << TWSTO))
			;
		return I2C_NOTOK;
	}

	if (send_stop)
	{
		TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
		while (TWCR & (1 << TWSTO))
			;
	}

	return I2C_OK;
}

static uint8_t mcu_i2c_read(bool with_ack, bool send_stop)
{
	uint8_t c = 0xFF;

	TWCR = (1 << TWINT) | (1 << TWEN) | ((!with_ack) ? 0 : (1 << TWEA));
	while (!(TWCR & (1 << TWINT)))
		;
	c = TWDR;

	if (send_stop)
	{
		TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
		while (TWCR & (1 << TWSTO))
			;
	}

	return c;
}

#ifndef mcu_i2c_send
// master sends command to slave
uint8_t mcu_i2c_send(uint8_t address, uint8_t *data, uint8_t datalen)
{
	if (datalen)
	{
		datalen--;
		if (mcu_i2c_write(address << 1, true, false) == I2C_OK) // start, send address, write
		{
			// send data, stop
			for (uint8_t i = 0; i < datalen; i++)
			{
				if (mcu_i2c_write(data[i], false, false) != I2C_OK)
				{
					return I2C_NOTOK;
				}
			}

			return mcu_i2c_write(data[datalen], false, true);
		}
	}

	return I2C_NOTOK;
}
#endif
#ifndef mcu_i2c_receive
// master receive response from slave
uint8_t mcu_i2c_receive(uint8_t address, uint8_t *data, uint8_t datalen)
{
	if (datalen)
	{
		datalen--;
		if (mcu_i2c_write((address << 1) | 0x01, true, false) == I2C_OK) // start, send address, write
		{
			for (uint8_t i = 0; i < datalen; i++)
			{
				data[i] = mcu_i2c_read(true, false);
			}

			data[datalen] = mcu_i2c_read(false, true);
			return I2C_OK;
		}
	}

	return I2C_NOTOK;
}
#endif

#ifndef mcu_i2c_config
void mcu_i2c_config(uint32_t frequency)
{
#if I2C_ADDRESS != 0
	TWAR = (I2C_ADDRESS << 1) | 1;
#endif
	// disable TWI
	TWCR &= ~(1 << TWEN);
	// set freq
	uint8_t div = 0;
	if ((frequency < 5000UL))
	{
		div = 3;
	}
	else if ((frequency < 20000UL))
	{
		div = 2;
	}
	else if ((frequency < 80000UL))
	{
		div = 1;
	}

	TWSR = div;
	TWBR = (uint8_t)((F_CPU / (frequency << (div << 1)))) & 0xFF;
	// enable TWI
	TWCR = (1 << TWINT) | (1 << TWEN);
#if I2C_ADDRESS != 0
	TWCR |= (1 << TWIE) | (1 << TWEA);
#endif
}
#endif

#if I2C_ADDRESS != 0
uint8_t mcu_i2c_buffer[I2C_SLAVE_BUFFER_SIZE];

ISR(TWI_vect)
{
	static uint8_t index = 0;

	uint8_t i = index;

	switch (TW_STATUS)
	{
	/*slave receiver*/
	case TW_SR_SLA_ACK:			   // addressed, returned ack
	case TW_SR_GCALL_ACK:		   // addressed generally, returned ack
	case TW_SR_ARB_LOST_SLA_ACK:   // lost arbitration, returned ack
	case TW_SR_ARB_LOST_GCALL_ACK: // lost arbitration, returned ack
		index = 0;
		TWCR |= (1 << TWEA);
		break;
	case TW_SR_DATA_ACK:
		index++;
		__attribute__((fallthrough));
	case TW_SR_STOP: // stop or repeated start condition received
		// sends the data
		if (i < I2C_SLAVE_BUFFER_SIZE)
		{
			mcu_i2c_buffer[i] = TWDR;
			TWCR |= (1 << TWEA);
		}
		if (TW_STATUS == TW_SR_STOP)
		{
			index = 0;
			mcu_i2c_buffer[i] = 0;
			// unlock ISR and process the info request
			mcu_enable_global_isr();
			mcu_i2c_slave_cb(mcu_i2c_buffer, i);
		}
		break;
	/*slave trasnmitter*/
	case TW_ST_SLA_ACK:			 // addressed, returned ack
	case TW_ST_ARB_LOST_SLA_ACK: // arbitration lost, returned ack
		i = 0;
		TWCR |= (1 << TWEA);
		__attribute__((fallthrough));
	case TW_ST_DATA_ACK: // byte sent, ack returned
		// copy data to output register
		TWDR = mcu_i2c_buffer[i++];
		// if there is more to send, ack, otherwise nack
		if (i < I2C_SLAVE_BUFFER_SIZE)
		{
			TWCR |= (1 << TWEA);
		}
		index = i;
		break;
	case TW_ST_DATA_NACK: // received nack, we are done
	case TW_ST_LAST_DATA: // received ack, but we are done already!
		index = 0;
		TWCR |= (1 << TWEA);
		break;
	case TW_BUS_ERROR: // bus error, illegal stop/start
		TWCR |= (1 << TWSTO);
		break;
	}

	// clear and reenable I2C ISR by default this falls to NACK if ACK is not set
	TWCR |= (1 << TWIE) | (1 << TWINT) | (1 << TWEN);
}
#endif
#endif

#if (defined(MCU_HAS_UART2) && defined(UART2_DETACH_MAIN_PROTOCOL))
#ifndef mcu_uart_putc
void mcu_uart_putc(uint8_t c)
{
	loop_until_bit_is_set(UCSRA_REG_2, UDRE_BIT_2);
	COM2_OUTREG = c;
}
#endif
#ifndef mcu_uart_getc
int16_t mcu_uart_getc(uint32_t timeout)
{
	timeout += mcu_millis();
	while (!CHECKBIT(UCSRA_REG_2, RXC_BIT_2))
	{
		if (timeout < mcu_millis())
		{
			return -1;
		}
	}
	return COM2_INREG;
}
#endif
#endif

#ifdef MCU_HAS_ONESHOT_TIMER

ISR(ONESHOT_COMPA_vect, ISR_NOBLOCK)
{
	// disable ISR
	ONESHOT_TIMSK = 0;
	if (mcu_timeout_cb)
	{
		mcu_timeout_cb();
	}
}

/**
 * configures a single shot timeout in us
 * */
#ifndef mcu_config_timeout

void mcu_config_timeout(mcu_timeout_delgate fp, uint32_t timeout)
{
	uint32_t clocks = timeout * (F_CPU / 1000000UL);
	uint8_t pres = (1 << 3); // CTC mode

	mcu_timeout_cb = fp;

#if (ONESHOT_TIMER == 2)
	if (clocks <= ((1UL << 8) - 1))
	{
		pres |= 1;
	}
	else if (clocks <= ((1UL << 11) - 1))
	{
		clocks >>= 3;
		pres |= 2;
	}
	else if (clocks <= ((1UL << 13) - 1))
	{
		clocks >>= 5;
		pres |= 3;
	}
	else if (clocks <= ((1UL << 14) - 1))
	{
		clocks >>= 6;
		pres |= 4;
	}
	else if (clocks <= ((1UL << 15) - 1))
	{
		clocks >>= 7;
		pres |= 5;
	}
	else if (clocks <= ((1UL << 16) - 1))
	{
		clocks >>= 8;
		pres |= 6;
	}
	else
	{
		clocks >>= 10;
		pres |= 7;
	}
#else
	if (clocks <= ((1UL << 8) - 1))
	{
		pres |= 1;
	}
	else if (clocks <= ((1UL << 11) - 1))
	{
		clocks >>= 3;
		pres |= 2;
	}
	else if (clocks <= ((1UL << 14) - 1))
	{
		clocks >>= 6;
		pres |= 3;
	}
	else if (clocks <= ((1UL << 16) - 1))
	{
		clocks >>= 8;
		pres |= 4;
	}
	else
	{
		clocks >>= 10;
		pres |= 5;
	}
#endif

	// stops timer
	ONESHOT_TCCRB = 0;
	// CTC mode
	ONESHOT_TCCRA = 0;
	// resets counter
	ONESHOT_TCNT = 0;
	// set step clock
	ONESHOT_OCRA = ((uint8_t)(clocks & 0xFF)) - 1;
	// clears interrupt flags by writing 1's
	ONESHOT_TIFR = 0x7;
	// start timer in CTC mode with the correct prescaler
	ONESHOT_TCCRB = pres;
}
#endif

/**
 * starts the timeout. Once hit the the respective callback is called
 * */
#ifndef mcu_start_timeout
void mcu_start_timeout()
{
}
#endif
#endif

#endif