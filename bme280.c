/* $Id: bme280.c $
 * Functions for talking to the
 * Bosch/Sensortec BME280 temperature/humidity/pressure-sensor
 */

#include <avr/io.h>
#include <util/delay.h>
#include "bme280.h"
#include "twi.h"

#define BME280_I2C_ADDR (0x77 << 1)

static uint32_t bme280_lastpress = 0;
static  int32_t bme280_lasttemp  = 0;
static uint16_t bme280_lasthum   = 0;

/* Three 16 bit calibration values for the temperature. */
static uint16_t bme280_dig_T1;
static  int16_t bme280_dig_T2;
static  int16_t bme280_dig_T3;

/* Nine 16 bit calibration values for the pressure. */
static uint16_t bme280_dig_P1;
static  int16_t bme280_dig_P2;
static  int16_t bme280_dig_P3;
static  int16_t bme280_dig_P4;
static  int16_t bme280_dig_P5;
static  int16_t bme280_dig_P6;
static  int16_t bme280_dig_P7;
static  int16_t bme280_dig_P8;
static  int16_t bme280_dig_P9;

/* Six 16 bit calibration values for the humidity. */
static  uint8_t bme280_dig_H1;
static  int16_t bme280_dig_H2;
static  uint8_t bme280_dig_H3;
static  int16_t bme280_dig_H4;
static  int16_t bme280_dig_H5;
static   int8_t bme280_dig_H6;


void bme280_init(void)
{
  uint8_t tmpval;
  /* The BME280 does not autoincrement register addresses on I2C writes.
   * Instead it expects pairs of Register-address and register-value to be
   * transmitted.
   * Note that the same is not true for READS, there it DOES autoincrement. */
  twi_open(BME280_I2C_ADDR | I2C_WRITE);
  
  /* Select Register 0xF2 "ctrl_hum" */
  twi_write(0xF2);
  /* 0xF2 = ctrl_hum: 00000101 = maximum oversampling for humidity */
  twi_write(0x05);
  
  /* Select Register 0xF4 "ctrl_meas". Has to be written AFTER ctrl_hum! */
  twi_write(0xF4);
  /* 0xF4 = ctrl_meas: 10110100 = max.oversampling press + temp, sleep mode */
  twi_write(0xB4);
  
  /* Select Register 0xF5 "config". */
  twi_write(0xF5);
  /* 0xF5 = config: 00001000 = filter coefficient 4, no 3pin SPI */
  twi_write(0x08);
  
  twi_close();
  
  /* The BME280 does not provide useful data directly, it just returns raw ADC
   * converter values. To turn these into a temperature / pressure / humidity,
   * you have to use an extremely complicated formula using the data from
   * no less than 33 (!) calibration registers stored on the chip in NVRAM.
   * These then need to be put into 18 helper variables for the formula, using
   * a weird mix of signed and unsigned 8 and 16 bit data types.
   * Seriously - WTF were they smoking?
   * Luckily for us, even though the behaviour of "|" on signed datatypes is
   * "implementation defined", gcc does the right thing here (and it's
   * documented that it does so, so it's not likely to change).
   * We need to read all of the registers and store them for later use. */
  twi_open(BME280_I2C_ADDR | I2C_WRITE);
  /* The madness starts at 0x88 / 89 = dig_T1. Oh, and as an added bonus:
   * in contrast to all other values that are Little Endian, the calibration
   * values are stored Big Endian. They really seem to have put A LOT of
   * effort in to make this as hard to use as possible. */
  twi_write(0x88);
  twi_close();
  twi_open(BME280_I2C_ADDR | I2C_READ);
  /* 0x88 / 0x89 = dig_T1 */
  bme280_dig_T1 = twi_read(1);
  bme280_dig_T1 |= (twi_read(1) << 8);
  /* 0x8a / 0x8b = dig_T2 */
  bme280_dig_T2 = twi_read(1);
  bme280_dig_T2 |= (twi_read(1) << 8);
  /* 0x8c / 0x8d = dig_T3 */
  bme280_dig_T3 = twi_read(1);
  bme280_dig_T3 |= (twi_read(1) << 8);
  /* 0x8e / 0x8f = dig_P1 */
  bme280_dig_P1 = twi_read(1);
  bme280_dig_P1 |= (twi_read(1) << 8);
  /* 0x90 / 0x91 = dig_P2 */
  bme280_dig_P2 = twi_read(1);
  bme280_dig_P2 |= (twi_read(1) << 8);
  /* 0x92 / 0x93 = dig_P3 */
  bme280_dig_P3 = twi_read(1);
  bme280_dig_P3 |= (twi_read(1) << 8);
  /* 0x94 / 0x95 = dig_P4 */
  bme280_dig_P4 = twi_read(1);
  bme280_dig_P4 |= (twi_read(1) << 8);
  /* 0x96 / 0x97 = dig_P5 */
  bme280_dig_P5 = twi_read(1);
  bme280_dig_P5 |= (twi_read(1) << 8);
  /* 0x98 / 0x99 = dig_P6 */
  bme280_dig_P6 = twi_read(1);
  bme280_dig_P6 |= (twi_read(1) << 8);
  /* 0x9a / 0x9b = dig_P7 */
  bme280_dig_P7 = twi_read(1);
  bme280_dig_P7 |= (twi_read(1) << 8);
  /* 0x9c / 0x9d = dig_P8 */
  bme280_dig_P8 = twi_read(1);
  bme280_dig_P8 |= (twi_read(1) << 8);
  /* 0x9e / 0x9f = dig_P9 */
  bme280_dig_P9 = twi_read(1);
  bme280_dig_P9 |= (twi_read(1) << 8);
  /* 0xa0 is not used, just read and throw it away. */
  twi_read(1);
  /* 0xa1 = dig_H1 */
  bme280_dig_H1 = twi_read(0);
  twi_close();
  /* The rest of the registers are in an entirely different place. */
  twi_open(BME280_I2C_ADDR | I2C_WRITE);
  twi_write(0xe1);
  twi_close();
  twi_open(BME280_I2C_ADDR | I2C_READ);
  /* 0xe1 / 0xe2 = dig_H2 */
  bme280_dig_H2 = twi_read(1);
  bme280_dig_H2 |= (twi_read(1) << 8);
  /* 0xe3 = dig_H3 */
  bme280_dig_H3 = twi_read(1);
  /* 0xe4 / 0xe5 / 0xe6. This is a real beauty, because this mess of 3x 8 bit
   * contains 2 16-bit registers, one in big, and one in little endian, and
   * both somehow shitfted by 4. */
  bme280_dig_H4 = twi_read(1) << 4;
  tmpval = twi_read(1);
  bme280_dig_H5 = twi_read(1) << 4;
  bme280_dig_H4 |= (tmpval & 0x0f);
  bme280_dig_H5 |= (tmpval >> 4);
  /* 0xe7 = dig_H6 */
  bme280_dig_H6 = (int8_t)twi_read(0);
  /* Finally - that's the end of this mess. */
  twi_close();
}

/* This will start one full measurement cycle ("forced mode").
 * The sensor will return to sleep mode automatically when finished */
void bme280_startonemeasurement(void)
{
  twi_open(BME280_I2C_ADDR | I2C_WRITE);
  /* Select Register 0xF4 "ctrl_meas". */
  twi_write(0xF4);
  /* 0xF4 = ctrl_meas: 10110101 = max.oversampling press + temp, forced mode */
  twi_write(0xB5);
  twi_close();
}

/* This will read the last measured values from the sensor. They can then
 * be queried with the corresponding functions below. */
void bme280_readmeasuredvalues(void)
{
  uint8_t tmpv1, tmpv2, tmpv3;
  int32_t t_fine; int32_t adc_P; int32_t adc_T; int32_t adc_H;
  int32_t var1; int32_t var2; int64_t var3; int64_t var4; int64_t ptmp;
  twi_open(BME280_I2C_ADDR | I2C_WRITE);
  /* Select Register 0xF7 "press_x". */
  twi_write(0xF7);
  twi_close();
  twi_open(BME280_I2C_ADDR | I2C_READ);
  /* Register 0xF7: press_msb */
  tmpv1 = twi_read(1);
  /* Register 0xF8: press_lsb */
  tmpv2 = twi_read(1);
  /* Register 0xF9: press_xlsb (yes, that's a weird naming) */
  tmpv3 = twi_read(1);
  /* The pressure is 20 bits left aligned, so we need to SHR the whole thing by 4 */
  adc_P = ((uint32_t)tmpv1 << 12) | ((uint32_t)tmpv2 << 4) | ((uint32_t)tmpv3 >> 4);
  /* Register 0xFA: temp_msb */
  tmpv1 = twi_read(1);
  /* Register 0xFB: temp_lsb */
  tmpv2 = twi_read(1);
  /* Register 0xFC: temp_xlsb (yes, that's a weird naming) */
  tmpv3 = twi_read(1);
  /* The temperature is 20 bits left aligned, so we need to SHR the whole thing by 4 */
  adc_T = ((uint32_t)tmpv1 << 12) | ((uint32_t)tmpv2 << 4) | ((uint32_t)tmpv3 >> 4);
  /* Register 0xFD: hum_msb */
  tmpv1 = twi_read(1);
  /* Register 0xFE: hum_lsb */
  tmpv2 = twi_read(0);
  /* The humidity is simply 16 bits wide, no weird shifting necessary */
  adc_H = ((uint16_t)tmpv1 << 8) | ((uint16_t)tmpv2);
  twi_close();
  /* Now comes the lengthy calculation. This mess comes from the datasheet
   * with only minimal modifications. I simply gave up trying to understand
   * WTF they are doing there. They never give a human readable formula, only
   * code. */
  /* Temperature */
  var1 = ((((adc_T >> 3) - ((int32_t)bme280_dig_T1 << 1))) * ((int32_t)bme280_dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)bme280_dig_T1)) *
            ((adc_T >> 4) - ((int32_t)bme280_dig_T1))) >> 12) *
          ((int32_t)bme280_dig_T3)) >> 14;
  t_fine = var1 + var2; /* This is also used for the pressure and humidity calculation later! */
  bme280_lasttemp = (t_fine * 5 + 128) >> 8;
  /* Pressure */
  var3 = ((int64_t)t_fine) - 128000;
  var4 = var3 * var3 * (int64_t)bme280_dig_P6
       + ((var3 * (int64_t)bme280_dig_P5) << 17)
       + (((int64_t)bme280_dig_P4) << 35);
  var3 = ((var3 * var3 * (int64_t)bme280_dig_P3) >> 8)
       + ((var3 * (int64_t)bme280_dig_P2) << 12);
  var3 = ((((((int64_t)1) << 47) + var3)) * ((int64_t)bme280_dig_P1)) >> 33;
  ptmp = 1048576 - adc_P;
  ptmp = (((ptmp << 31) - var4) * 3125) / var3;
  var3 = (((int64_t)bme280_dig_P9) * (ptmp >> 13) * (ptmp >> 13)) >> 25;
  var4 = (((int64_t)bme280_dig_P8) * ptmp) >> 19;
  bme280_lastpress = ((ptmp + var3 + var4) >> 8) + (((int64_t)bme280_dig_P7) << 4);
  var1 = (t_fine - ((int32_t)76800));
  var1 = (((((adc_H << 14)
             - (((int32_t)bme280_dig_H4) << 20)
             - (((int32_t)bme280_dig_H5) * var1))
            + ((int32_t)16384)) >> 15) *
          (((((((var1 * ((int32_t)bme280_dig_H6)) >> 10) *
               (((var1 * ((int32_t)bme280_dig_H3)) >> 11) + ((int32_t)32768))) >> 10)
             + ((int32_t)2097152)) *
            ((int32_t)bme280_dig_H2) + 8192) >> 14));
  var1 = (var1 - (((((var1 >> 15) * (var1 >> 15)) >> 7) * ((int32_t)bme280_dig_H1)) >> 4));
  if (var1 < 0) { var1 = 0; }
  if (var1 > 419430400) { var1 = 419430400; }
  bme280_lasthum = ((uint32_t)var1 >> 12);
}

uint32_t bme280_getpressure(void)
{
  return bme280_lastpress;
}

int32_t bme280_gettemperature(void)
{
  return bme280_lasttemp;
}

uint16_t bme280_gethumidity(void)
{
  return bme280_lasthum;
}
