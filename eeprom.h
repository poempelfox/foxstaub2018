/* $Id: eeprom.h
 * This defines the contents of the EEPROM, both the locations that the
 * firmware will read, and the values that will be written to the EEPROM if you
 * do 'make uploadeeprom'.
 */

#ifndef _EEPROM_H_
#define _EEPROM_H_

extern EEMEM uint8_t ee_sensorid;
extern EEMEM uint8_t ee_invsensorid; /* This is used as a sort of "CRC" */

#endif /* _EEPROM_H_ */
