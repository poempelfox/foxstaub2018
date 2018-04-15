/* $Id: eeprom.h
 * This defines the contents of the EEPROM, both the locations that the
 * firmware will read, and the values that will be written to the EEPROM if you
 * do 'make uploadeeprom'.
 */

#include <avr/eeprom.h>
#include "eeprom.h"

/* The SensorID */
#define THESENSORID 21
/* Do not set these directly, set the define above */
EEMEM uint8_t ee_sensorid = THESENSORID;
EEMEM uint8_t ee_invsensorid = THESENSORID ^ 0xff;

