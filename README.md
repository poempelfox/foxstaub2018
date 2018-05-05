
# FoxStaub 2018

(FIXME: add picture)

## Introduction

This is a solar powered air quality (fine particles) sensor based on an SDS011
particulate matter (PM) sensor. It also features a BME280 sensor for
measuring temperature, humidity and barometric pressure. An
Adafruit 32U4 RFM69HCW with 868 MHz radio is used as the main microcontroller,
and transmits the measured data wirelessly. The radio-transmitted measurements
are then received by a [Jeelink V3](https://jeelabs.net/projects/hardware/wiki/JeeLink)
and fed into [FHEM](https://fhem.de/).

This project was of course inspired by [https://luftdaten.info](luftdaten.info),
a project for the crowd-sourcing of open sensor data originating in the city of
Stuttgart, which does due to its geography have a very high fine particle
pollution.

## Part list / BOM

* Adafruit Feather 32U4 RFM69HCW (868 MHz variant)
* Sparkfun BME280 breakout
* SDS011 air quality sensor
* JST XH 7pin plug to connect the SDS011.  
  I used Voelkner #D17687 (as the hull of the plug) and #D16379 (as the pins
  inside), and this is the proper plug to use, but alternatively, you should be
  just fine with using normal jumper wire connectors.
* Two "Marley HT" pipe bends are used as the case.
* "Goal Zero Guide 10 Plus Power Bank" for storing the power
* (5m) Micro USB power cable for getting the power from the power bank to the sensor
* FIXME solarzelle und schaltung

## Power

Sadly, the SDS011 uses a lot of power while measuring, the data sheet claims
up to 80 mA. That makes powering this with solar power a challenge, even if it
does not measure continuously (it measures for 30 seconds before going to sleep
for 120 seconds because that increases the sensor life).

## Wireless protocol

Data is sent on 868,300 MHz with FSK modulation and a bitrate of 17241 baud.

## Compile error

Compilation of console.c will fail when using an avr-gcc version between
4.8 and 6.4 (inclusive) due to a compiler bug. avr-gcc will die with the
error message `internal compiler error: in push_reload, at reload.c:1360`.
As a workaround, you can add `-fno-move-loop-invariants` to the compiler
flags. Using this just for console.c is enough, for the rest of the source gcc
does not need this workaround.
