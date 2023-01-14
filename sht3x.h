/* $Id: sht3x.h $
 * Functions for reading the SHT31 temperature / humidity sensor
 * (should also work unmodified with SHT30 or SHT35 as they use they are 100%
 * software compatible)
 */

#ifndef _SHT3X_H_
#define _SHT3X_H_

struct sht3xdata {
  uint16_t temp;
  uint16_t hum;
  uint8_t valid;
};

/* Initialize sht3x */
void sht3x_init(void);

/* Start measurement */
void sht3x_startmeas(void);

/* Read result of measurement. Needs to be called no earlier than 15 ms
 * after starting. */
void sht3x_read(struct sht3xdata * d);

#endif /* _SHT3X_H_ */
