#!/bin/sh
# Run inside the miyoomini-toolchain container, from the repo root, after `source /root/setup-env.sh`.
set -e
if [ "${UNION_PLATFORM}" != "miyoomini" ]; then echo "Must be run from the cross-compile shell"; exit 1; fi
cd "$(dirname "$0")/.."
VERSION=${1:?version required, e.g. 1.0}
make clean && make headless && make
mkdir -p build out
./build/headless --icon build/icon.png
STAGE=/tmp/staging_onion
PORTS=$STAGE/Roms/PORTS
APP=$PORTS/.BroforceLite   # hidden folder (leading dot): only the launcher shows up in the Ports list
rm -rf "${STAGE:?}" && mkdir -p "$APP" "$PORTS/Imgs"
${CROSS_COMPILE}strip -o $APP/broforcelite build/broyoo
cp cross-compile/port_launch.sh "$PORTS/Broforce Lite.sh"
cp build/icon.png "$PORTS/Imgs/Broforce Lite.png"
cp build/icon.png $APP/icon.png
cp -r assets/sounds $APP/sounds
[ -d assets/music ] && cp -r assets/music $APP/music || echo "WARNING: no assets/music - synth music fallback"
touch $APP/debug.txt.off   # rename to debug.txt to enable the debug mode
chmod +x "$PORTS/Broforce Lite.sh" $APP/broforcelite
FILE="broforcelite_onion_v${VERSION}.zip"
(cd $STAGE && zip -r $FILE .)
cp $STAGE/$FILE build/
echo "Created build/$FILE"
