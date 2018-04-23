/* $Id: main.c $
 * main for foxstaub2018 (Foxis Feinstaubsensor)
 * (C) Michael "Fox" Meier 2018
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "bme280.h"
#include "eeprom.h"
#include "lufa/console.h"
#include "rfm69.h"
#include "sds011.h"
#include "timers.h"

/* The values last measured */
/* How often did we send a packet? */
uint32_t pktssent = 0;
/* Pressure, in (Pascal * 256) */
uint32_t pressure = 0;
/* Temperature, in (degC * 100) */
int32_t temperature = 0;
/* Rel. Humidity, in (% * 1024) */
uint16_t humidity = 0;
/* Particulate matter measurements. in (PMn * 10) ug/m^3 */
/* 0xffff marks it as invalid, this value can never be measured as it's far
 * outside the sensors range */
uint16_t particulatematter2_5u = 0xffff;
uint16_t particulatematter10u = 0xffff;

/* This is just a fallback value, in case we cannot read this from EEPROM
 * on Boot */
uint8_t sensorid = 3; // 0 - 255 / 0xff

/* The frame we're preparing to send. */
static uint8_t frametosend[17];

/* Length of one SDS011 measurement cycle, in ticks. */
#define SDS011CYCLELENGTH 72 /* 151 seconds */
/* How long do we turn the sensor on at the beginning of the cycle? */
#define SDS011CYCLEONTIME 15 /* 31 seconds */

/* We need to disable the watchdog very early, because it stays active
 * after a reset with a timeout of only 15 ms. */
void dwdtonreset(void) __attribute__((naked)) __attribute__((section(".init3")));
void dwdtonreset(void) {
  MCUSR = 0;
  wdt_disable();
}

static uint8_t calculatecrc(uint8_t * data, uint8_t len)
{
  uint8_t i, j;
  uint8_t res = 0;
  for (j = 0; j < len; j++) {
    uint8_t val = data[j];
    for (i = 0; i < 8; i++) {
      uint8_t tmp = (uint8_t)((res ^ val) & 0x80);
      res <<= 1;
      if (0 != tmp) {
        res ^= 0x31;
      }
      val <<= 1;
    }
  }
  return res;
}

/* Fill the frame to send with our collected data and a CRC.
 * The protocol we use is that of a "CustomSensor" from the
 * FHEM LaCrosseItPlusReader sketch for the Jeelink.
 * So you'll just have to enable the support for CustomSensor in that sketch
 * and flash it onto a JeeNode and voila, you have your receiver.
 *
 * Byte  0: Startbyte (=0xCC)
 * Byte  1: Sensor-ID (0 - 255/0xff)
 * Byte  2: Number of data bytes that follow (6)
 * Byte  3: Sensortype (=0xf5 for FoxStaub)
 * Byte  4: Pressure, MSB
 * Byte  5: Pressure       Pressure is in 1/256th Pascal.
 * Byte  6: Pressure
 * Byte  7: Pressure, LSB
 * Byte  8: Temperature, MSB   The temperature is in 1/100th degrees C with an
 * Byte  9: Temperature, LSB   offset of +100.00, so e.g. 12155 = 21.55 degC
 * Byte 10: rel.Humidity, MSB     humidity is in 1/1024th percent.
 * Byte 11: rel.Humidity, LSB
 * Byte 12: PM2.5, MSB       particulate matter 2.5 is in 1/10th ug/m^3
 * Byte 13: PM2.5, LSB
 * Byte 14: PM10, MSB
 * Byte 15: PM10, LSB
 * Byte 16: CRC
 */
void prepareframe(void)
{
  uint16_t temptmp;
  temptmp = (uint16_t)(temperature + 10000);
  if (temperature < -10000) { temptmp = 0; }
  frametosend[ 0] = 0xCC;
  frametosend[ 1] = sensorid;
  frametosend[ 2] = 13; /* 13 bytes of data follow (CRC not counted) */
  frametosend[ 3] = 0xf5; /* Sensor type: FoxStaub */
  frametosend[ 4] = (pressure >> 24) & 0xff;
  frametosend[ 5] = (pressure >> 16) & 0xff;
  frametosend[ 6] = (pressure >>  8) & 0xff;
  frametosend[ 7] = (pressure >>  0) & 0xff;
  frametosend[ 8] = (temptmp >> 8) & 0xff;
  frametosend[ 9] = (temptmp >> 0) & 0xff;
  frametosend[10] = (humidity >> 8) & 0xff;
  frametosend[11] = (humidity >> 0) & 0xff;
  frametosend[12] = (particulatematter2_5u >> 8) & 0xff;
  frametosend[13] = (particulatematter2_5u >> 0) & 0xff;
  frametosend[14] = (particulatematter10u >> 8) & 0xff;
  frametosend[15] = (particulatematter10u >> 0) & 0xff;
  frametosend[16] = calculatecrc(frametosend, 16);
}

void loadsettingsfromeeprom(void)
{
  uint8_t e1 = eeprom_read_byte(&ee_sensorid);
  uint8_t e2 = eeprom_read_byte(&ee_invsensorid);
  if ((e1 ^ 0xff) == e2) { /* OK, the 'checksum' matches. Use this as our ID */
    sensorid = e1;
  }
}

int main(void)
{
  uint16_t lasttxts = 0xf000; /* This forces an update immediately after start */
  uint16_t sds011cyclestart = 0xffff - (SDS011CYCLELENGTH / 2); /* places us at the end of a cycle */
  uint16_t lastloopts = 0;
  uint16_t curts;
  uint16_t tsdiff;
  uint8_t transmitinterval = 15; /* Transmitinterval in ticks of 2.1s, so 15 = 31s */
  
  /* Initialize stuff */
  
  loadsettingsfromeeprom();
  
  timers_init();
  console_init();
  rfm69_initport();
  /* The RFM69 needs some time to start up (5 ms according to data sheet, we wait 10 to be sure) */
  _delay_ms(10);
  rfm69_initchip();
  rfm69_setsleep(1);
  /* The BME280 needs 2 ms startup, so should be ready now as well. */
  bme280_init();
  
  /* Enable watchdog timer with a timeout of 8 seconds */
  wdt_enable(WDTO_8S); /* Longest possible on ATmega328P */
  
  /* Disable unused chip parts and ports */
  /* PE6 is the IRQ line from the RFM69. We don't use it. Make sure that pin
   * is tristated on our side (it won't float, the RFM69 pulls it) */
  PORTE &= (uint8_t)~_BV(PE6);
  DDRE &= (uint8_t)~_BV(PE6);
  /* Turn off unused stuff on the AVR via PRR registers */
  /* We don't use Timer0 and the ADC*/
  PRR0 |= _BV(PRTIM0) | _BV(PRADC);
  /* We don't use Timer3+4. There seems to be a bug in
   * avr-libc on Ubuntu 16.04, it doesn't define PRTIM4 but instead
   * PRTIM2 for a nonexistant Timer2. Therefore we cannot use the
   * macro and hardcode the correct value. */
  PRR1 |= _BV(/* PRTIM4 */ 4) | _BV(PRTIM3);
  DDRC |= (uint8_t)_BV(PC7); /* PC7 is the LED pin, drive it */
  PORTC &= (uint8_t)~_BV(PC7); /* turn it off. */

  /* The SDS011 needs a LOT longer to start. So lets sleep for some more
   * time and then try to init it. */
  _delay_ms(2000);
  sds011_init();

  /* Prepare sleep mode */
  /* SLEEP_MODE_IDLE is the only sleepmode we can safely use. */
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  /* All set up, enable interrupts and go. */
  sei();

  while (1) {
    wdt_reset();
    curts = timers_getticks();
    tsdiff = curts - lasttxts;
    if (tsdiff >= transmitinterval) {
      /* Time to update values and send */
      bme280_readmeasuredvalues();
      pressure = bme280_getpressure();
      temperature = bme280_gettemperature();
      humidity = bme280_gethumidity();
      particulatematter2_5u = sds011_getlastpm2_5();
      particulatematter10u = sds011_getlastpm10();
      bme280_startonemeasurement(); /* Start the next measurement */
      /* SEND */
      rfm69_setsleep(0);  /* This mainly turns on the oscillator again */
      prepareframe();
      console_printpgm_P(PSTR(" TX "));
      rfm69_sendarray(frametosend, 17);
      rfm69_setsleep(1);
      pktssent++;
      lasttxts = curts; /* Remember when we last sent a packet */
      /* We use the lowest two bits of pressure as random noise */
      uint8_t rnd = pressure & 0x00000003;
      if (rnd == 3) {
        transmitinterval = 17;
      } else if (rnd == 0) {
        transmitinterval = 15;
      } else { /* 1 or 2 */
        transmitinterval = 16;
      }
    }
    tsdiff = curts - lastloopts;
    if (tsdiff > 0) { /* OK, we had one tick. (we need to make sure we execute only once per tick!) */
      lastloopts = curts;
      tsdiff = curts - sds011cyclestart;
      if (tsdiff >= SDS011CYCLELENGTH) { /* Cycle over - start again */
        sds011cyclestart = curts;
        sds011_setmeasurements(1);
      } else if (tsdiff == SDS011CYCLEONTIME) {
        sds011_requestresult(); /* Request latest result */
        sds011_setmeasurements(0); /* Then turn off */
      }
    }
    console_work();
    if (!console_isusbconfigured()) {
      /* Don't go to sleep when USB is configured. Because then there is no
       * lack of power, and more importantly, we want the console to feel
       * "snappy" and we can't get that if we sleep for 2 second. */
      wdt_reset(); /* Buy us 8 seconds time because the next IRQ might only arrive in 2 seconds */
      sleep_cpu(); /* Go to sleep until the next IRQ arrives */
    }
  }
}
