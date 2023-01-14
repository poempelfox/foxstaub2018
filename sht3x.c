/* $Id: sht3x.c $
 * Functions for reading the SHT 31 temperature / humidity sensor
 * (should also work unmodified with SHT30 or SHT35 as they use they are 100%
 * software compatible)
 */

#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>
#include "sht3x.h"
#include "twi.h"

/* The I2C address of the sensor.
 * If the addr pin of the sensor is pulled high, it's 0x45 instead. */
#define SHT3X_I2C_ADDR (0x44 << 1)

/* Commands for the sensor. The sensor supports a bunch
 * of measuring modes: oneshot or periodic, with clock
 * stretching (pulling SCL low until measurement is
 * completed) or with out, ... Only the ones we are likely
 * to use are listed here, look up the rest in the data sheet. */
/* MSB */
#define SHT3X_ONESHOT_NOCS      0x24
#define SHT3X_ONESHOT_CS        0x2c
#define SHT3X_READSTATUSREG_MSB 0xf3
/* LSB */
#define SHT3X_ONESHOT_NOCS_HIGREP  0x00
#define SHT3X_ONESHOT_NOCS_MEDREP  0x0b
#define SHT3X_ONESHOT_NOCS_LOWREP  0x16
#define SHT3X_ONESHOT_CS_HIGREP    0x06
#define SHT3X_ONESHOT_CS_MEDREP    0x0d
#define SHT3X_ONESHOT_CS_LOWREP    0x10
#define SHT3X_READSTATUSREG_LSB    0x2d

void sht3x_init(void)
{
  /* There is nothing to initialize at the SHT31 really. */
}

void sht3x_startmeas(void)
{
  twi_open(SHT3X_I2C_ADDR | I2C_WRITE);
  /* single shot, high repeatability, no 'clock stretch' */
  twi_write(SHT3X_ONESHOT_NOCS);
  twi_write(SHT3X_ONESHOT_NOCS_HIGREP);
  twi_close();
}

/* This function is based on Sensirons example code and datasheet */
uint8_t sht3x_crc(uint8_t b1, uint8_t b2)
{
  uint8_t crc = 0xff; /* Start value */
  uint8_t b;
  crc ^= b1;
  for (b = 0; b < 8; b++) {
    if (crc & 0x80) {
      crc = (crc << 1) ^ 0x131;
    } else {
      crc = crc << 1;
    }
  }
  crc ^= b2;
  for (b = 0; b < 8; b++) {
    if (crc & 0x80) {
      crc = (crc << 1) ^ 0x131;
    } else {
      crc = crc << 1;
    }
  }
  return crc;
}

void sht3x_read(struct sht3xdata * d)
{
  d->valid = 0;
  /* There is no "command", just addressing the device while indicating a */
  /* read. The device will reply with NAK if it has not finished yet. */
  twi_open(SHT3X_I2C_ADDR | I2C_READ);
  uint8_t b1 = twi_read(1);  /* Temp MSB */
  uint8_t b2 = twi_read(1);  /* Temp LSB */
  uint8_t b3 = twi_read(1);  /* Temp CRC */
  uint8_t b4 = twi_read(1);  /* Humi MSB */
  uint8_t b5 = twi_read(1);  /* Humi LSB */
  uint8_t b6 = twi_read(0);  /* Humi CRC */
  if ((sht3x_crc(b1, b2) == b3) && (sht3x_crc(b4, b5) == b6)) {
    d->valid = 1;
  }
  d->temp = (b1 << 8) | b2;
  d->hum = (b4 << 8) | b5;
  twi_close();
}
