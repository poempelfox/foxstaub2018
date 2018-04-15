/* $Id: rfm69.h $
 * Functions for communication with RF69 module
 */

#ifndef _RFM69_H_
#define _RFM69_H_

/* This configures the pins on the AVR for the right modes (i.e. INPUT/OUTPUT/SPI)
 * and it also resets the RFM! */
void rfm69_initport(void);
void rfm69_initchip(void);
void rfm69_clearfifo(void);
void rfm69_settransmitter(uint8_t e);
void rfm69_sendarray(uint8_t * data, uint8_t length);
void rfm69_setsleep(uint8_t s);
uint8_t rfm69_readreg(uint8_t reg);

#endif /* _RFM69_H_ */
