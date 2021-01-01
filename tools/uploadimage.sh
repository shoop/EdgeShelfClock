#!/bin/sh
#
# "Reverse engineered" from https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/blob/master/src/ESP8266LittleFS.java
# Better would be support in VS Code for this but it's OK for now.
python ~/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/upload.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 write_flash 0x200000 ../build/image.littlefs.bin
