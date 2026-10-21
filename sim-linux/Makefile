# sim-linux/Makefile — build the my_cnc firmware as a Linux simulator.
#
# Uses plain gcc (no PlatformIO). Compiles the REAL uCNC core from this
# branch (parser, planner, interpolator, motion control, Grbl protocol)
# against a POSIX virtual MCU (UART2 <-> TCP socket, timers via pthreads).
#
#   make            build ./build/ucnc-sim
#   make run        build + listen on 127.0.0.1:34000
#   make run PORT=34001
#   make clean
#
# Only two shared firmware headers are touched for simulator support, both
# with backward-compatible guards (stock builds unaffected):
#   uCNC/src/hal/boards/boarddefs.h      SIM_SKIP_BOARDMAP_OVERRIDES
#   uCNC/src/cnc_hal_config_helper.h     SIM_HAL_OVERRIDES_FILE
#
# The simulated machine is 5-axis cartesian (X Y Z A B) like the SKR build.

CC      ?= gcc
BUILD   ?= build
TARGET  := $(BUILD)/ucnc-sim

UCSRC   := ../uCNC/src
UCINC   := ../uCNC

# -DMCU=99 is MCU_VIRTUAL_WIN (see uCNC/src/hal/mcus/mcus.h).
# -DMCU is defined so boarddefs.h skips the real boardmap entirely.
CFLAGS  := -std=gnu99 -O2 -g -Wall -Wno-unused-function -Wno-unused-variable \
	-D_GNU_SOURCE \
	-DF_CPU=1000000UL \
	-DMCU=99 \
	-DAXIS_COUNT=5 \
	-DSIM_SKIP_BOARDMAP_OVERRIDES \
	-DSIM_HAL_OVERRIDES_FILE='"sim_hal_overrides.h"' \
	-I. -I$(UCINC) -I$(UCSRC) \
	-pthread

LDFLAGS := -pthread -lm -rdynamic

SRCS := \
	$(UCSRC)/cnc.c \
	$(UCSRC)/utils.c \
	$(UCSRC)/modules/file_system.c \
	$(UCSRC)/core/interpolator.c \
	$(UCSRC)/core/io_control.c \
	$(UCSRC)/core/motion_control.c \
	$(UCSRC)/core/parser.c \
	$(UCSRC)/core/parser_expr.c \
	$(UCSRC)/core/planner.c \
	$(UCSRC)/hal/kinematics/kinematic.c \
	$(UCSRC)/hal/kinematics/kinematic_cartesian.c \
	$(UCSRC)/hal/kinematics/kinematic_corexy.c \
	$(UCSRC)/hal/kinematics/kinematic_delta.c \
	$(UCSRC)/hal/kinematics/kinematic_linear_delta.c \
	$(UCSRC)/hal/kinematics/kinematic_scara.c \
	$(UCSRC)/hal/mcus/mcu.c \
	$(UCSRC)/hal/mcus/virtual/mcu_virtual_linux.c \
	$(UCSRC)/hal/tools/tool.c \
	$(UCSRC)/hal/tools/tools/spindle_pwm.c \
	$(UCSRC)/interface/grbl_print.c \
	$(UCSRC)/interface/grbl_protocol.c \
	$(UCSRC)/interface/grbl_settings.c \
	$(UCSRC)/interface/grbl_stream.c \
	$(UCSRC)/module.c

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all run clean info

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "built $@"

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

PORT ?= 34000
run: $(TARGET)
	./$(TARGET) -p $(PORT)

info:
	@echo "SRCS=$(SRCS)"
