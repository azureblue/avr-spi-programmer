# rpi-atmega8-programmer
### Simple ATmega8 flash programmer for Raspberry Pi.

It uses **SPI** (3 wires directly connected to the ATmega8) and **GPIO** (pin 22, connected to the ATmega8 RESET pin). In this configuration the ATmega8 can be powered from the 3.3v RPi line. Do not use 5v as it would probably damage the RPi.
