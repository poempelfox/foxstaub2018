
# FoxStaub 2018 - 2022 edition

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
* Adafruit SHT31 (temperature/humidity sensor) breakout
* ClosedCube LPS25HB (pressure sensor) breakout
* Note that the original version of this, built in 2018, used a BME280 instead of the SHT31 and the LPS25HB. After two of these died within just 5 years (after reporting utter nonsense in the weeks or months before), I have decided these are not suitable sensors, and replaced them.
* SDS011 air quality sensor
* JST XH 7pin plug to connect the SDS011.  
  I used Voelkner #D17687 (as the hull of the plug) and #D16379 (as the pins
  inside), and this is the proper plug to use, but alternatively, you should be
  just fine with using normal jumper wire connectors.
* Two "Marley HT" pipe bends are used as the case, as in the luftdaten.info project.  
  However, the DN75 size they recommend wasn't available, so I used DN90 instead.
* 12V AGM battery for storing the power. I originally used a "Offgridtec AGM Solar
  Batterie extrem zyklenfest 12 Ah 12V" off Amazon, but this proved to be too small
  for the winter, always running out of power around December. This has been upgraded
  to a 32 Ah battery in January 2023, we'll see how that one works out.
* (5m) Micro USB power cable for getting the power from the battery to the sensor
* solar charge controller with USB output (I used a "LS0512EU 5A" off Amazon)
* a solar panel big enough to charge the battery even when it's not sunny (I  
  used a "Suaoki Solar Autobatterie Panel Ladegeraet 18W 18V" off Amazon. This  
  is a thin, bendable module that I mounted onto a wooden board for stability.)
* (5m) cable for connecting the solar panel to the charge controller
* fuse to prevent short circuits from making things go up in smoke


## Power

Sadly, the SDS011 uses a lot of power while measuring, the data sheet claims
up to 80 mA. That makes powering this with solar power a challenge, even if it
does not measure continuously (it measures for 30 seconds before going to sleep
for 120 seconds because that increases the sensor life) and actual consumption
seems to be less than what the datasheet claims.

The microcontroller-board actually would have a LiPo charger circuit onboard,
so you could just connect a somewhat large LiPo battery and be done with it
(you'd need to generate the 5V for the SDS011 through a boost converter from
the microcontroller-boards 3.3V output though).
However, I did not like the idea of putting a temperamental LiPo battery into a
tiny case that can heat to 50+ degrees when hit by the sun, and cool below -20
in winter. Instead, the plan was to use a "Goal Zero Guide 10 Plus" power bank
with four NiMH batteries for storing the power. However, it turned out to be
impossible to find any 6V solar panel that would be big enough to charge that
power bank and rigid enough to be mounted on my balcony. It seems solar panels
only come in two flavours: 6V and utter crap (mechanically), or 12+V.

So sadly, instead of the nice and compact power bank,
I had to use a 12V AGM battery and matching solar charge controller with
USB output. These parts are not right where the sensor and the solar panel are,
but instead they're connected by 5 meters of cable and stored safely in the
closet adjacent to my balcony where they are protected from the weather.


## Wireless protocol

Data is sent on 868,300 MHz with FSK modulation and a bitrate of 17241 baud.
On a higher level, the protocol we use is that of a "CustomSensor" from the
FHEM LaCrosseItPlusReader sketch for the Jeelink. However, support for that
if not compiled in by default, so you will have to enable it in the source
code, then recompile the sketch and flash the result onto your Jeelink.

The file `36_Foxstaub2018viaJeelink.pm` in this repository contains the
FHEM module for receiving the sensor. To use it, you will need to put this
file into /opt/fhem/FHEM/ and then modify the file 36_JeeLink.pm: to the
string `clientsJeeLink` append `:Foxstaub2018viaJeelink`.

The format of the packets we send is this:

| Byte | Content description |
| ---: | --------------------|
| (-)  | Preamble: 3 times 0xAA (you usually don't see these bytes anywhere) |
| (-)  | Sync-Bytes (2): 0x2D 0xD4 |
|   0  | Startbyte (=0xCC) |
|   1  | Sensor-ID (in the range 0 - 255/0xff) |
|   2  | Number of data bytes that follow (13) |
|   3  | Sensortype (=0xf5 for FoxStaub) |
|   4  | Pressure, MSB. Raw value from LPS25HB. |
|   5  | Pressure cont. |
|   6  | Pressure, LSB |
|   7  | Temperature, MSB.  Raw value from SHT31. Formula for converting to degree celsius (value / 65535) * 175 - 45 |
|   8  | Temperature, LSB |
|   9  | rel.Humidity, MSB.  Raw value from SHT31. 0 == 0%, 65535 == 100% |
|  10  | rel.Humidity, LSB |
|  11  | PM2.5, MSB.  Particulate Matter 2.5u is in 1/10th ug/m^3 |
|  12  | PM2.5, LSB |
|  13  | PM10, MSB.  Particulate Matter 10u is in 1/10th ug/m^3 |
|  14  | PM10, LSB |
|  15  | Battery voltage. This is measured through a voltage divider, with 1 MOhm towards GND, and 10 MOhm towards '+'. The ADC runs with a reference voltage of 2.56 volts, meaning 255 would be 2.56 volts, thus the formula for converting this value into volts is: value * 0.11 |
|  16  | CRC |


## Compile error

Compilation of console.c will fail when using an avr-gcc version between
4.8 and 6.4 (inclusive) due to a compiler bug. avr-gcc will die with the
error message `internal compiler error: in push_reload, at reload.c:1360`.
As a workaround, you can add `-fno-move-loop-invariants` to the compiler
flags. Using this just for console.c is enough, for the rest of the source gcc
does not need this workaround.

