/* $Id: lps25hb.c $
 * Functions for reading the LPS 25 HB pressure (and temperature) sensor
 */

#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>
#include "lps25hb.h"
#include "twi.h"

/* The I2C address of the sensor. */
#define LPS25HB_I2C_ADDR (0x5C << 1)

/* Commands for the sensor. The sensor supports a bunch
 * of measuring modes: oneshot or periodic... Only the ones we are likely
 * to use are listed here, look up the rest in the data sheet. */
#define LPS25HB_CTRL_REG1   0x20
#define LPS25HB_CTRL_REG2   0x21
#define LPS25HB_RES_CONF    0x10
#define LPS25HB_PRESS_OUT_XL   0x28
#define LPS25HB_PRESS_OUT_L    0x29
#define LPS25HB_PRESS_OUT_H    0x2a
#define LPS25HB_TEMP_OUT_L     0x2b
#define LPS25HB_TEMP_OUT_H     0x2c
#define LPS25HB_STATUS_REG     0x27

void lps25hb_init(void)
{
  /* Turn on. */
  twi_open(LPS25HB_I2C_ADDR | I2C_WRITE);
  twi_write(LPS25HB_CTRL_REG1);
  twi_write(0x80);
  twi_close();
  /* Set resolution for pressure to maximum, for temperature to minimum. */
  twi_open(LPS25HB_I2C_ADDR | I2C_WRITE);
  twi_write(LPS25HB_RES_CONF);
  twi_write(0x03);
  twi_close();
}

void lps25hb_startmeas(void)
{
  twi_open(LPS25HB_I2C_ADDR | I2C_WRITE);
  twi_write(LPS25HB_CTRL_REG2);
  /* start single shot single shot */
  twi_write(0x01);
  twi_close();
}

void lps25hb_read(struct lps25hbdata * d)
{
  uint8_t tmp;
  d->valid = 0;
  twi_open(LPS25HB_I2C_ADDR | I2C_WRITE);
  twi_write(LPS25HB_STATUS_REG | I2C_AUTOINCREGADDR);
  twi_close();
  twi_open(LPS25HB_I2C_ADDR | I2C_READ);
  tmp = twi_read(1);
  if (tmp & 0x02) { d->valid = 1; }
  /* Next register after LPS25HB_STATUS_REG (0x27) ist LPS25HB_PRESS_OUT_XL (0x28),
   * how convenient */
  d->pressure[0] = twi_read(1);
  d->pressure[1] = twi_read(1);
  d->pressure[2] = twi_read(0);
  twi_close();
}
