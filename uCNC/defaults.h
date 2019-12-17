#ifndef DEFAULTS_H
#define DEFAULTS_H

//default step per mm
#define DEFAULT_X_STEP_PER_MM 200
#define DEFAULT_Y_STEP_PER_MM 200
#define DEFAULT_Z_STEP_PER_MM 200
#define DEFAULT_A_STEP_PER_MM 200
#define DEFAULT_B_STEP_PER_MM 200
#define DEFAULT_C_STEP_PER_MM 200

//default feed in mm/s
#define DEFAULT_X_MAX_FEED 100
#define DEFAULT_Y_MAX_FEED 100
#define DEFAULT_Z_MAX_FEED 100
#define DEFAULT_A_MAX_FEED 100
#define DEFAULT_B_MAX_FEED 100
#define DEFAULT_C_MAX_FEED 100

#define DEFAULT_HOMING_DIR_INV_MASK 0
#define DEFAULT_HOMING_SLOW 50
#define DEFAULT_HOMING_FAST 10
#define DEFAULT_HOMING_OFFSET 2

//default acceleration in mm/s^2
#define DEFAULT_X_ACCEL 10
#define DEFAULT_Y_ACCEL 10
#define DEFAULT_Z_ACCEL 10
#define DEFAULT_A_ACCEL 10
#define DEFAULT_B_ACCEL 10
#define DEFAULT_C_ACCEL 10

//default max distance traveled by each axis in mm
#define DEFAULT_X_MAX_DIST 200
#define DEFAULT_Y_MAX_DIST 200
#define DEFAULT_Z_MAX_DIST 10

#define DEFAULT_STEP_INV_MASK 0
#define DEFAULT_STEP_ENA_INV 0
#define DEFAULT_DIR_INV_MASK 0

#define DEFAULT_STATUS_MASK 1
#define DEFAULT_CONTROL_INV_MASK 1
#define DEFAULT_LIMIT_INV_MASK 1

#define DEFAULT_INPUT_MASK0 0
#define DEFAULT_INPUT_MASK1 0

#define DEFAULT_ARC_TOLERANCE 0.002

#define DEFAULT_TOOL_COUNT 1

#define DEFAULT_MAX_STEP_RATE 20000

#endif
