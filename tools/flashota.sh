#!/bin/sh
#
# No support in VS Code for OTA upload with password.
# https://github.com/microsoft/vscode-arduino/issues/1172
BUILDFOLDER=$(readlink -f $(dirname $(readlink -f "$0"))/../build)
BINFILE=${BUILDFOLDER}/EdgeShelfClock.ino.bin
if [ ! -f ${BINFILE} ]; then
    echo "Image file to upload ${BINFILE} not found."
    exit 1
fi
echo "Uploading image ${BINFILE}..."
python $HOME/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/espota.py --ip=192.168.178.82 -a admin123 -f ${BINFILE}
