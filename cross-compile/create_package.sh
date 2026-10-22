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
APP=$STAGE/App/BroforceLite
rm -rf "${STAGE:?}" && mkdir -p "$APP"
${CROSS_COMPILE}strip -o $APP/broforcelite build/broyoo
cp cross-compile/launch.sh $APP/
cp build/icon.png $APP/icon.png
cp -r assets/sounds $APP/sounds
[ -d assets/music ] && cp -r assets/music $APP/music || echo "WARNING: no assets/music - synth music fallback"
touch $APP/debug.txt.off   # rename to debug.txt to enable the debug mode
sed "s/VERSION/${VERSION}/" cross-compile/config.json > $APP/config.json
chmod +x $APP/launch.sh $APP/broforcelite
FILE="broforcelite_onion_v${VERSION}.zip"
(cd $STAGE && zip -r $FILE .)
cp $STAGE/$FILE build/
echo "Created build/$FILE"
