/* $Id: sds011.h $
 * Functions for talking to the Nova Fitness SDS011 fine particle sensor
 */

#ifndef _SDS011_H_
#define _SDS011_H_

/* Initialize the sensor */
void sds011_init(void);

/* Turn measurements on or off. */
void sds011_setmeasurements(uint8_t ooo);

#endif /* _SDS011_H_ */
