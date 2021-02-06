#!/bin/sh
#
# No support in VS Code for OTA upload with password.
# https://github.com/microsoft/vscode-arduino/issues/1172
python $HOME/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/espota.py --ip=192.168.178.82 -a admin123 -f ../../build/FastLedTest.ino.bin
