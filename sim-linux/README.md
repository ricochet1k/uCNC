# sim-linux — my_cnc config on upstream's EMULATOR_LINUX

This builds the machine's config (5-axis cartesian, S-curve runtime `$14`) on
upstream's `EMULATOR_LINUX` virtual MCU, with the Grbl protocol on a PTY
instead of stdin/stdout. It keeps the production serial path: the SimpleCNC
server opens the PTY with `CNC_TRANSPORT=serial`, exactly like a real USB-UART.

## Build & run

```sh
# PlatformIO (native platform, no board toolchain needed)
pio run -e EMULATOR_LINUX
.pio/build/EMULATOR_LINUX/program
# prints: SIM PTY: /dev/pts/N (link /tmp/ucnc-sim-tty)
```

The emulator creates the PTY pair; the master stays inside the emulator and
`/tmp/ucnc-sim-tty` is a symlink to the slave. Point the server at it:

```sh
cd simplecnc/server
CNC_TRANSPORT=serial CNC_SERIAL_PATH=/tmp/ucnc-sim-tty \
  ./node_modules/.bin/vite-node ./app.ts
```

`SIM_PTY_LINK` overrides the symlink path (default `/tmp/ucnc-sim-tty`).

## Controls

stdin (line commands):

```
probe 0|1
limit X|Y|Z|A|B 0|1
estop 0|1
fhold 0|1
door 0|1
reset
status
quit
```

The emulator also serves an HTTP dashboard on `127.0.0.1:8888` with input-pin
injection (`virtual_io_set_input` is shared by both paths).

## What is compiled

- `MCU_VIRTUAL_LINUX`, `BOARD=BOARD_CUSTOM`, `AXIS_COUNT=5`.
- `SIM_SKIP_BOARDMAP_OVERRIDES`: skip the SKR pin map; the virtual MCU provides
  pins. `SIM_HAL_OVERRIDES_FILE`: use `sim-linux/sim_hal_overrides.h` instead of
  the machine's TMC/SD HAL overrides.
- Hardware modules (`sd_card_v2`, `tmc_driver`) are excluded; the core machine
  config (axes, steps/mm, limits, probe, spindle) is the real one.
- Sockets stay enabled (HTTP dashboard), with `ENABLE_SOCKETS` from
  `mcumap_virtual.h`; UART2 is redirected to the PTY under `SIM_PTY`.

## Notes

- `$RST=*` then `$X` is the safe bring-up (blank EEPROM). `$10=7` reports
  `Ln` + `Buf` (the machine's saved `$10=1` has no `Buf`).
- The old `my_cnc-linux-sim` branch has a separate TCP platform port; this
  branch uses upstream's emulator instead and should replace it.
- `ptsname_r` on the slave can fail with ENOTTY here; the code uses
  `ttyname_r` and symlinks the result.
