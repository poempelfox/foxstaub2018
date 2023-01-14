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

#include "adc.h"
#include "eeprom.h"
#include "lps25hb.h"
#include "lufa/console.h"
#include "rfm69.h"
#include "sds011.h"
#include "sht3x.h"
#include "timers.h"

/* The values last measured */
/* How often did we send a packet? */
uint32_t pktssent = 0;
/* Pressure, in (Pascal * 256) */
uint32_t pressure = 0;
/* Temperature, in (degC * 100) */
int32_t temperature = 0;
/* Rel. Humidity, in (% * 512) */
uint16_t humidity = 0;
/* Particulate matter measurements. in (PMn * 10) ug/m^3 */
/* 0xffff marks it as invalid, this value can never be measured as it's far
 * outside the sensors range */
uint16_t particulatematter2_5u = 0xffff;
uint16_t particulatematter10u = 0xffff;
uint8_t batvolt = 0;

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
 * Byte  2: Number of data bytes that follow (13)
 * Byte  3: Sensortype (=0xf5 for FoxStaub)
 * Byte  4: Pressure, MSB    (raw value from LPS25HB)
 * Byte  5: Pressure
 * Byte  6: Pressure, LSB
 * Byte  7: Temperature, MSB   (raw value from SHT31)
 * Byte  8: Temperature, LSB
 * Byte  9: rel.Humidity, MSB  (raw value from SHT31)
 * Byte 10: rel.Humidity, LSB
 * Byte 11: PM2.5, MSB       particulate matter 2.5 is in 1/10th ug/m^3
 * Byte 12: PM2.5, LSB
 * Byte 13: PM10, MSB
 * Byte 14: PM10, LSB
 * Byte 15: CRC
 */
void prepareframe(void)
{
  frametosend[ 0] = 0xCC;
  frametosend[ 1] = sensorid;
  frametosend[ 2] = 13; /* 13 bytes of data follow (CRC not counted) */
  frametosend[ 3] = 0xf5; /* Sensor type: FoxStaub */
  frametosend[ 4] = (pressure >> 16) & 0xff;
  frametosend[ 5] = (pressure >>  8) & 0xff;
  frametosend[ 6] = (pressure >>  0) & 0xff;
  frametosend[ 7] = (temperature >> 8) & 0xff;
  frametosend[ 8] = (temperature >> 0) & 0xff;
  frametosend[ 9] = (humidity >> 8) & 0xff;
  frametosend[10] = (humidity >> 0) & 0xff;
  frametosend[11] = (particulatematter2_5u >> 8) & 0xff;
  frametosend[12] = (particulatematter2_5u >> 0) & 0xff;
  frametosend[13] = (particulatematter10u >> 8) & 0xff;
  frametosend[14] = (particulatematter10u >> 0) & 0xff;
  frametosend[15] = batvolt;
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
  
  adc_init();
  timers_init();
  console_init();
  rfm69_initport();
  /* The RFM69 needs some time to start up (5 ms according to data sheet, we wait 10 to be sure) */
  _delay_ms(10);
  rfm69_initchip();
  rfm69_setsleep(1);
  sht3x_init();
  lps25hb_init();
  
  /* Enable watchdog timer with a timeout of 8 seconds */
  wdt_enable(WDTO_8S); /* Longest possible on ATmega328P */
  
  /* Disable unused chip parts and ports */
  /* PE6 is the IRQ line from the RFM69. We don't use it. Make sure that pin
   * is tristated on our side (it won't float, the RFM69 pulls it) */
  PORTE &= (uint8_t)~_BV(PE6);
  DDRE &= (uint8_t)~_BV(PE6);
  /* Turn off unused stuff on the AVR via PRR registers */
  /* We don't use Timer0 */
  PRR0 |= _BV(PRTIM0);
  /* We don't use Timer3+4. There seems to be a bug in
   * avr-libc on Ubuntu 16.04, it doesn't define PRTIM4 but instead
   * PRTIM2 for a nonexistant Timer2. Therefore we cannot use the
   * macro and hardcode the correct value. */
  PRR1 |= _BV(/* PRTIM4 */ 4) | _BV(PRTIM3);
  DDRC |= (uint8_t)_BV(PC7); /* PC7 is the LED pin, drive it */
  PORTC &= (uint8_t)~_BV(PC7); /* turn it off. */
  /* Disable digital input registers for ADC pins. We don't use any of
   * the ADC pins for digital input and only use ADC4 for ADC. */
  DIDR0 |= _BV(ADC7D) | _BV(ADC6D) | _BV(ADC5D) | _BV(ADC4D) | _BV(ADC1D) | _BV(ADC0D);
  DIDR2 |= _BV(ADC13D) | _BV(ADC12D) | _BV(ADC11D) | _BV(ADC10D) | _BV(ADC9D) | _BV(ADC8D);

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
      struct sht3xdata temphum;
      struct lps25hbdata lps25press;
      /* Time to update values and send */
      adc_power(1);
      adc_start();
      sht3x_read(&temphum);
      if (temphum.valid) {
        temperature = temphum.temp;
        humidity = temphum.hum;
      } else {
        /* FIXME best values for marking as invalid? */
        temperature = 0xffff;
        humidity = 0xffff;
      }
      /* FIXME add LPS25HB here */
      lps25hb_read(&lps25press);
      if (lps25press.valid) {
        pressure = ((uint32_t)lps25press.pressure[2] << 16)
                 | ((uint32_t)lps25press.pressure[1] <<  8)
                 | lps25press.pressure[0];
      } else {
        pressure = 0xffffff;
      }
      particulatematter2_5u = sds011_getlastpm2_5();
      particulatematter10u = sds011_getlastpm10();
      sht3x_startmeas(); /* Start the next measurement */
      lps25hb_startmeas();
      batvolt = adc_read() >> 2;
      adc_power(0);
      /* SEND */
      rfm69_setsleep(0);  /* This mainly turns on the oscillator again */
      prepareframe();
      console_printpgm_P(PSTR(" TX "));
      rfm69_sendarray(frametosend, 18);
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
