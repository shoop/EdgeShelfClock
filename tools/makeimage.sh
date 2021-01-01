#!/bin/sh
#
# "Reverse engineered" from https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/blob/master/src/ESP8266LittleFS.java
# Better would be support in VS Code for this but it's OK for now.
$HOME/.arduino15/packages/esp8266/tools/mklittlefs/2.5.0-4-fe5bb56/mklittlefs -c data -p 256 -b 8192 -s $((2024 * 1024)) ../build/image.littlefs.bin
