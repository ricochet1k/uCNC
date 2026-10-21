// sim_hal_overrides.h
// Minimal HAL overrides for the Linux simulator build (see sim-linux/).
//
// The real machine's uCNC/cnc_hal_overrides.h pulls in TMC2209 drivers and
// the SD-card module, which need real hardware (software UART/SPI pins).
// The simulator runs with no loadable modules: the core motion control,
// parser, planner and Grbl protocol are 100% the my_cnc code, only the
// MCU vim (pins/timers/serial/EEPROM) is emulated in
// uCNC/src/hal/mcus/virtual/mcu_virtual_linux.c
//
// Consequences for tests: M906/M350/M913/M914 (TMC) and SD commands will
// answer with `error:20` (unsupported) instead of TMC data. That is
// intentional — the SimpleCNC client must tolerate per-command errors
// (see server/cnc-protocol tests), and real TMC answers are validated
// against the physical machine.

#ifndef SIM_HAL_OVERRIDES_H
#define SIM_HAL_OVERRIDES_H

#ifdef __cplusplus
extern "C"
{
#endif

// No modules: LOAD_MODULES_OVERRIDE intentionally left undefined so
// module.c compiles to an empty module list.

// Match the real my_cnc build (uCNC/cnc_hal_overrides.h): S-curve accel code
// path compiled and the runtime profile exposed as $14. Without this the sim
// runs a different planner path than the machine.
#undef S_CURVE_ACCELERATION_LEVEL
#define S_CURVE_ACCELERATION_LEVEL -1

#ifdef __cplusplus
}
#endif

#endif
