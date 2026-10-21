#!/bin/bash
# run-sim.sh PORT — start the simulator with debug env knobs.
# Env (all optional): SIM_TICK_DEBUG=1 SIM_ISR_DEBUG=1 SIM_PROFILE=1 UCNC_SIM_PORT
cd "$(dirname "$0")"
export SIM_TICK_DEBUG=${SIM_TICK_DEBUG:-1}
export SIM_PROFILE=${SIM_PROFILE:-1}
export SIM_ISR_DEBUG=${SIM_ISR_DEBUG:-1}
PORT="${1:-34000}"
exec ./build/ucnc-sim -p "$PORT"
