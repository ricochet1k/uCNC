# Upstream merge + PTY emulator — notes (2026-10-22)

This records why `my_cnc` is being brought onto current upstream and how the
simulator was migrated from the fork-local TCP port to upstream's
`EMULATOR_LINUX` virtual MCU with a PTY on UART2.

## Branch state

| ref | commit | meaning |
|---|---|---|
| `upstream/master` | `8c96148f` (2026-10-14) | real upstream, 676 commits ahead of the old my_cnc base |
| `my_cnc` / `origin/my_cnc` | `c7020bef` | machine branch, **untouched** |
| `my_cnc-pre-upstream-merge` | `c7020bef` | tag for rollback |
| `my_cnc-upstream-merge` | `fd4378a9` | staging: merge + emulator work (pushed) |

`my_cnc-upstream-merge` contains `59287bf0` (merge), `39c86af7` (emulator/PTY)
and `fd4378a9` (TMC module update + sim docs). The branch is based on the
machine branch, not on upstream master; the merge brings upstream forward.

## Merge conflict decisions

| file | resolution |
|---|---|
| `platformio.ini` | kept the machine `default_envs`, build flags and module list; removed the empty `board =` from `[env]` (PlatformIO rejects an empty board for the native emulator). |
| `uCNC/cnc_hal_overrides.h` | kept the machine overrides (TMC, SD, 5 axes). Added `IO_CONDITION_PROBE` combining `PROBE` and `PROBE2`, since upstream replaced the old probe body with a single condition macro. |
| `uCNC/src/cnc.c` | upstream version (formatting/flow). |
| `uCNC/src/core/io_control.c` | upstream version; dual-probe behaviour is preserved by the `IO_CONDITION_PROBE` macro. |
| `uCNC/src/core/parser.c` | upstream (`DBGLOG` instead of a hard `#undef DBGMSG`). |
| `uCNC/src/hal/boards/lpc176x/lpc176x.ini` | kept the SKR flash offsets; upstream renamed `BOARD` to `BOARDMAP` in the build flag. |
| `uCNC/src/hal/mcus/lpc176x/mcu_lpc176x.c` | upstream RTC ISR signalling a low-priority task instead of the fork's `lpc_global_isr_enabled`; the extern in `mcumap_lpc176x.h` is left as upstream has it. |
| `uCNC/src/interface/grbl_print.c` / `.h` | both from upstream (`prt_int` uses `uint32_t`; the merge had kept the old `int32_t` header). |

`PROBE2`/`PROBE2_INVERT` are machine-local; `PROBE2_INVERT` is disabled, so
OR-ing the two inputs then applying `probe_invert_mask` matches the old
behaviour.

## TMC module

The tracked `uCNC/src/modules/tmc_driver` was from the old core and does not
compile against the merged module API (`TMC##TYPE##_STEPPER_RW` expansion).
Replaced with the current module from the `uCNC-modules` master archive; the
SKR target now links.

## Emulator: upstream EMULATOR_LINUX + PTY UART2

Upstream's virtual MCU keeps UART2 (`MCU_HAS_UART2`) as stdin/stdout; UART1
(`MCU_HAS_UART`, `UART_PORT_NAME`) is the optional serial endpoint. That is why
the first PTY attempt produced no data: `serial_init()` is not called. The
fork-local simulator had always bridged **UART2** over TCP; we kept that stream
and changed only the transport:

- `virtual_linux.c`, `SIM_PTY`:
  - `sim_pty_init()` creates a PTY with `openpty`, keeps the master fd and
    prints/symlinks the slave (`/tmp/ucnc-sim-tty`, override `SIM_PTY_LINK`).
  - `sim_uart2_read/write` do non-blocking master I/O.
  - a stdin control thread parses `probe`, `limit X|Y|Z|A|B`, `estop`, `fhold`,
    `door`, `reset`, `status`, `quit`.
  - `ttyname_r` is used instead of `ptsname_r` (the latter returns `ENOTTY` on
    this host after `openpty`).
- `mcu_virtual.c`, `SIM_PTY`:
  - `mcu_init()` calls `sim_pty_init()`.
  - `mcu_uart2_flush()` writes to the PTY instead of stdout.
  - `mcu_uart2_process()` reads the PTY instead of stdin.
  - `virtual_io_set_input(pin, value)` is shared by the stdin console and the
    built-in HTTP dashboard (`127.0.0.1:8888`).
- `sim_hal_overrides.h`: no machine modules, runtime S-curve (`$14`), and
  `MCU_CYCLES_LOOP_OVERHEAD` for the renamed upstream MCU cycles macro.
- `boarddefs.h` / `cnc_hal_config_helper.h`: opt-in guards to skip the SKR
  boardmap and machine HAL overrides.
- `virtual.ini` EMULATOR_LINUX: `AXIS_COUNT=5`, `SIM_SKIP_BOARDMAP_OVERRIDES`,
  `SIM_HAL_OVERRIDES_FILE`, `SIM_PTY`, `_GNU_SOURCE`, `-lutil`; excludes
  `sd_card_v2` and `tmc_driver`.

Build/run:

```sh
pio run -e EMULATOR_LINUX
.pio/build/EMULATOR_LINUX/program        # prints SIM PTY: /dev/pts/N
# server:
CNC_TRANSPORT=serial CNC_SERIAL_PATH=/tmp/ucnc-sim-tty ./node_modules/.bin/vite-node ./app.ts
```

## Validation

- `pio run -e EMULATOR_LINUX` — SUCCESS.
- `pio run -e LPC176X-SKR-v1_4-TURBO` — SUCCESS (RAM 28.3%, flash 24.3%).
  The branch keeps the local platform path `../pio-nxplpc-arduino-lpc176x`; the
  build here used the published `p3p/pio-nxplpc-arduino-lpc176x` 0.1.3 archive
  only for validation.
- SimpleCNC against the PTY with `CNC_TRANSPORT=serial`:
  `e2e-cadence`, `e2e-job`, `e2e-pause`, `e2e-stutter`, `e2e-bridge`,
  `e2e-vfd-safety` — all PASS. This exercises `SerialTransport` +
  `ReadlineParser`, not the old test-only `TcpTransport`.

## Known gaps

- No hardware validation yet (no USB/serial on `arch-volans.local` at the time).
- The Python helper tests (`sim-vel-test.py`, `sim-ln-test.py`, `sim-persist-test.py`,
  `sim-rt-test.py`, `sim-heal-test.py`, `sim-console.py`) still speak TCP and
  were left on `my_cnc-linux-sim`; the serial e2e suite is the current gate.
- The emulator needs PlatformIO (`pio`) rather than the old plain-gcc
  `sim-linux/Makefile`.
- The machine build needs the local LPC176x platform checkout unless the
  platform line is changed to the published archive.
- The merge is large (676 upstream commits); the machine branch should get a
  hardware smoke test before `my_cnc` is fast-forwarded.

## Next steps

1. Review the staging branch; run the SKR build on the machine host.
2. Fast-forward `my_cnc`:
   ```sh
   git checkout -B my_cnc my_cnc-upstream-merge
   git push origin my_cnc
   ```
3. After a hardware smoke test, retire `my_cnc-linux-sim`/its TCP platform port
   and port the remaining helper tests to the PTY.
4. Optionally upstream the PTY + control-channel support as a small addition to
   `EMULATOR_LINUX` (separate from the machine-specific merge).
