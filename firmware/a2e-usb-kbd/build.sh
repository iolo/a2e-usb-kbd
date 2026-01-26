#!/bin/bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
arduino-cli config init
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli board listall
arduino-cli lib install "USB Host Shield Library 2.0"
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old .
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano:cpu=atmega328old .
