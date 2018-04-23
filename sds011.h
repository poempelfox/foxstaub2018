/* $Id: sds011.h $
 * Functions for talking to the Nova Fitness SDS011 fine particle sensor
 */

#ifndef _SDS011_H_
#define _SDS011_H_

/* Initialize the sensor */
void sds011_init(void);

/* Turn measurements on or off. */
void sds011_setmeasurements(uint8_t ooo);

/* Request measurement results from sensor */
void sds011_requestresult(void);

/* Fetch the data last received from the sensor */
uint16_t sds011_getlastpm2_5(void);
uint16_t sds011_getlastpm10(void);

#endif /* _SDS011_H_ */
