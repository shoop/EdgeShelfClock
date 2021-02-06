#!/bin/sh
#
# "Reverse engineered" from https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/blob/master/src/ESP8266LittleFS.java
# Better would be support in VS Code for this but it's OK for now.
BUILDFOLDER=$(readlink -f $(dirname $(readlink -f "$0"))/../build)
mkdir -p $BUILDFOLDER
$HOME/.arduino15/packages/esp8266/tools/mklittlefs/2.5.0-4-fe5bb56/mklittlefs -c data -p 256 -b 8192 -s $((2024 * 1024)) ${WORKSPACEFOLDER}/build/image.littlefs.bin
