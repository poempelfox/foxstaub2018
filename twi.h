/* $Id: twi.h $
 * Functions for handling the I2C-compatible TWI bus.
 * We have multiple devices connected there, so this handles the common stuff
 */

#ifndef _TWI_H_
#define _TWI_H_

/* I2C defines */
#define I2C_AUTOINCREGADDR 0x80
#define I2C_WRITE 0x00
#define I2C_READ  0x01

/* Initialize TWI */
void twi_init(void);

/* send start condition and select slave */
void twi_open(uint8_t addr);

/* send stop condition */
void twi_close(void);

/* write one byte to the TWI bus */
void twi_write(uint8_t what);
/* read one byte from the TWI bus.
 * ACK sets whether to ACK or NAK the transfer to the slave.
 * Only ACK if you intend to receive more bytes, the last byte MUST NOT be ACKd! */
uint8_t twi_read(uint8_t ack);

#endif /* _TWI_H_ */
