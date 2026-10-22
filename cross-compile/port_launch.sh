#!/bin/sh
# Onion OS "Ports" entry: the game lives in the hidden folder .BroforceLite next to this script.
cd "$(dirname "$0")/.BroforceLite"
# Only stderr goes to log.txt (rewritten at each launch): rumble/perf/pixel-format diagnostics land there.
./broforcelite 2>log.txt
# Safety net: make sure the rumble motor is off whatever happened (GPIO 48 is active low).
echo 1 > /sys/class/gpio/gpio48/value 2>/dev/null
