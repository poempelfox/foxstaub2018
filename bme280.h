/* $Id: bme280.h $
 * Functions for talking to the
 * Bosch/Sensortec BME280 temperature/humidity/pressure-sensor
 */

#ifndef _BME280_H_
#define _BME280_H_

/* Initialize the sensors registers */
void bme280_init(void);

/* This will start one full measurement cycle ("forced mode").
 * The sensor will return to sleep mode automatically when finished */
void bme280_startonemeasurement(void);

/* This will read the last measured values from the sensor. They can then
 * be queried with the corresponding functions below. */
void bme280_readmeasuredvalues(void);

/* Returns the values read on the last call of bme280_readmeasuredvalues */
uint32_t bme280_getpressure(void);
uint32_t bme280_gettemperature(void);
uint16_t bme280_gethumidity(void);

#endif /* _BME280_H_ */
