# uCNC Linux simulator (`sim-linux/`)

Runs the **real `my_cnc` firmware core** (parser, planner, interpolator,
motion control, Grbl protocol) on a Linux PC for testing the SimpleCNC
client without risking the machine.

Branch: `my_cnc-linux-sim` (based on `origin/my_cnc`).
The `my_cnc` branch itself is untouched; only two shared headers gained
backward-compatible opt-in guards (stock builds unaffected):

- `uCNC/src/hal/boards/boarddefs.h` — `SIM_SKIP_BOARDMAP_OVERRIDES`
  skips the SKR machine's `boardmap_overrides.h` (real pins/MCU id).
- `uCNC/src/cnc_hal_config_helper.h` — `SIM_HAL_OVERRIDES_FILE` selects
  `sim-linux/sim_hal_overrides.h` instead of the machine's
  `cnc_hal_overrides.h` (which pulls in TMC2209 + SD-card modules that
  need real hardware).

New files:

- `uCNC/src/hal/mcus/virtual/mcu_virtual_linux.c` — POSIX port of the
  (Windows-only) virtual MCU. Reuses `MCU_VIRTUAL_WIN` (99) and the stock
  `mcumap_virtual.h` pin map, so no firmware IDs/headers changed.
- `sim-linux/Makefile` — plain-`gcc` build, no PlatformIO needed.
- `sim-linux/sim_hal_overrides.h` — minimal HAL overrides (no modules);
  compiles the S-curve path (`S_CURVE_ACCELERATION_LEVEL -1`, `$14`) to match
  the real machine. TMC/SD modules are still absent.
- `sim-linux/*.py`, `run-sim.sh` — helpers and tests (below).

Simulated machine: 5-axis cartesian (X Y Z A B), like the SKR build.

## Build & run

```sh
cd sim-linux
make            # -> build/ucnc-sim
./run-sim.sh 34000   # TCP Grbl console on 127.0.0.1:34000
# or: ./build/ucnc-sim -p 34000   (UCNC_SIM_PORT also works)
```

Talk to it with `nc 127.0.0.1 34000`, `sim-console.py`, or the
SimpleCNC server (`CNC_TRANSPORT=tcp`, see simplecnc `server/`).

First boot has a blank EEPROM, so (like the safe-settings feature on
the real board) it comes up in `Alarm` asking for `$RST=*`:

```
$RST=*
0x18       # soft-reset: re-syncs position state (see quirk below)
$X
$10=7        # status mask with WPos/MPos + Buf: planner + RX free bytes
?            # <Idle|MPos:...|FS:...|Ln:0|Buf:20,130>
G1 X10 F600
```

> **Bring-up quirk (real firmware behavior, not a sim bug):** on a
> blank-EEPROM boot the settings RAM is all zeros, and the boot-time
> `mc_sync_position()` computes `0 steps / 0 steps/mm = NaN`, poisoning
> the tracked position. `$RST=*` restores the settings but nothing
> re-syncs afterwards, so the first moves get NaN distances/feeds and
> crawl as a single whole-block segment (also breaking feed-hold
> response). Soft-reset (`0x18`, runs `cnc_reset` → re-sync) or a
> restart with the persisted `virtualeeprom` file heals it. Same flow
> exists upstream — blank EEPROM + RST + X without reset/homing would
> crawl on hardware too. For tests, either keep the persisted file or
> use the RST → `0x18` → X sequence above.
>
> Note: `0x18` mid-motion leaves the drive in Alarm (`ALARM:3`)
> and repeated `$X` will not unlock it in this snapshot (verified
> 3× idle-reset unlocks fine, mid-motion reset never unlocks). Once it
> happens, the instance stays in `ALARM:3` for every later test — restart
> the simulator before running anything else.
> Whether that is sim timing or a genuine my_cnc quirk needs a
> hardware check (start a move, reset, `$X`) before reporting
> upstream. Committed test flows never reset mid-run, so nothing
> here depends on it.

`Buf:20,130` matches the real machine: 20 planner blocks free,
130 serial RX bytes free (`RX_BUFFER_CAPACITY` 128 + margin).
`Ln:` line numbers work (`N1 G0 ...`, `GCODE_PROCESS_LINE_NUMBERS`
is on). Realtime chars (`? ! ~ 0x18 0x85 0x90…`) are raw bytes, like
a real serial line.

## Simulator console (stdin)

Pin injection for probes/limits/controls, e.g. with
`tail -n 0 -f cmds.txt | ./build/ucnc-sim`:

```
probe 1 / probe 0
limit X 1 / limit X 0     (X Y Z A B)
estop 1 / fhold 1 / door 1 / reset 1   (0 to release)
status
quit
```

Changes are applied on the main thread (console only stages them),
so `G38.2` probing and hold/door interlocks behave like hardware.

## Design notes (read before "fixing" things)

- **Single-threaded core.** An earlier revision ran RTC/step callbacks
  on a pthread with a mutex; the firmware's asymmetric
  disable/enable idiom (`__ATOMIC__` in `utils.h`) made balanced
  locking impossible and deadlocked motion. Now `mcu_dotasks()`
  time-slices `ticksimul_1ms()` (1 kHz RTC) and step events from
  elapsed wall time — matching single-core MCU execution, where the
  flag is only save/restore bookkeeping.
- **`mcu_step_cb()` exits with the ISR flag disabled by design**
  (hardware re-enables it in the next RTC entry). The tick poll
  deliberately does NOT gate on the flag, or ticks would stall after
  the first step batch.
- **Catch-up timing.** Elapsed wall time is converted to RTC ticks
  and step events (capped at 1 s), so sim-time tracks wall-time even
  when a `dotasks` iteration is slow.
- **TX with no listener is dropped**, never blocks (a real UART with
  no terminal behaves the same). The first-run settings dump would
  otherwise wedge boot before any client connects.

## Known limitations

- Motion is slower than the real machine **when the bring-up quirk
  above bites** (blank EEPROM without reset). With valid settings
  (persisted file, or RST → `0x18` → X) it runs at proper federates.
  Do not use a poisoned run for federate validation; `sim-heal-test.py`
  distinguishes the two states.
- **No TMC/SD modules**: `M906/M350/M913/M914` and SD commands answer
  `error:20`. The client must tolerate per-command errors (it does —
  see simplecnc server tests).
- EEPROM is a `virtualeeprom` file in the working directory;
  `rm` it for a factory reset.
- Step/dir/pwm pins are virtual (in-memory); there is no
  IO GUI (yet) — drive pins via the stdin console.

## Tests

- `sim-console.py PORT [cmd...]` — raw Grbl console.
- `sim-persist-test.py PORT` — RST → unlock → move → poll on one
  connection (the core regression flow).
- `sim-ln-test.py PORT` — `N1` line-number tracking (`Ln:`).
- `sim-vel-test.py PORT` — timed move (use only on a healed run).
- `sim-rt-test.py PORT` — realtime `0x18`/`?` during motion.
- `sim-heal-test.py PORT` — blank-EEPROM crawl vs post-reset speed.

`make` doubles as the build test (the whole core must compile
warning-clean enough to link). The SimpleCNC repo holds the
client-side protocol tests against this simulator.
