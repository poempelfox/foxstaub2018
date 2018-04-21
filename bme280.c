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
static uint32_t bme280_lasttemp  = 0;
static uint16_t bme280_lasthum   = 0;

void bme280_init(void)
{
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
  /* 0xF5 = config: 00010000 = filter coefficient 16, no 3pin SPI */
  twi_write(0x10);
  
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
  bme280_lastpress = ((uint32_t)tmpv1 << 12) | ((uint32_t)tmpv2 << 4) | ((uint32_t)tmpv3 >> 4);
  /* Register 0xFA: temp_msb */
  tmpv1 = twi_read(1);
  /* Register 0xFB: temp_lsb */
  tmpv2 = twi_read(1);
  /* Register 0xFC: temp_xlsb (yes, that's a weird naming) */
  tmpv3 = twi_read(1);
  /* The temperature is 20 bits left aligned, so we need to SHR the whole thing by 4 */
  bme280_lasttemp = ((uint32_t)tmpv1 << 12) | ((uint32_t)tmpv2 << 4) | ((uint32_t)tmpv3 >> 4);
  /* Register 0xFD: hum_msb */
  tmpv1 = twi_read(1);
  /* Register 0xFE: hum_lsb */
  tmpv2 = twi_read(0);
  /* The humidity is simply 16 bits wide, no weird shifting necessary */
  bme280_lasthum = ((uint16_t)tmpv1 << 8) | ((uint16_t)tmpv2);
  twi_close();
}

uint32_t bme280_getpressure(void)
{
  return bme280_lastpress;
}

uint32_t bme280_gettemparature(void)
{
  return bme280_lasttemp;
}

uint16_t bme280_gethumidity(void)
{
  return bme280_lasthum;
}
