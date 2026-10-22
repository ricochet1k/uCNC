#!/bin/sh
# Build and run the my_cnc config on upstream's EMULATOR_LINUX.
set -e
cd "$(dirname "$0")/.."
"${PIO:-pio}" run -e EMULATOR_LINUX
exec .pio/build/EMULATOR_LINUX/program "$@"
