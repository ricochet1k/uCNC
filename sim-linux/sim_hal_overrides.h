// sim-linux/sim_hal_overrides.h
// Minimal HAL overrides for hosted simulator builds of the my_cnc config.
// Selected with -DSIM_HAL_OVERRIDES_FILE='"sim_hal_overrides.h"'; stock
// firmware builds are unaffected.
#ifndef SIM_HAL_OVERRIDES_H
#define SIM_HAL_OVERRIDES_H

// No machine modules in the simulator: LOAD_MODULES_OVERRIDE stays undefined.
// Match the machine's runtime-S-curve build.
#undef S_CURVE_ACCELERATION_LEVEL
#define S_CURVE_ACCELERATION_LEVEL -1

// Upstream renamed the cycles macro but mcumap_virtual.h still defines the
// old name; satisfy mcu.h.
#ifndef MCU_CYCLES_LOOP_OVERHEAD
#define MCU_CYCLES_LOOP_OVERHEAD 0
#endif

#endif
