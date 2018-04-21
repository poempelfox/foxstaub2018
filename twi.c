/* $Id: twi.c $
 * Functions for handling the I2C-compatible TWI bus.
 * We have multiple devices connected there, so this handles the common stuff.
 */

#include <avr/io.h>
#include <util/delay.h>
#include "twi.h"
#include "lufa/console.h"

void twi_init(void)
{
  /* Set bitrate to 100kbps
   * SCL frequency = CPUFREQ / (16 + (2 * TWBR * TWPS))
   * with TWBR = bitrate  TWPS = prescaler */
  TWBR = 32;
  /* The two TWPS bits are hidden in the status register */
  TWSR = 0; /* prescaler = 1 (that's the poweron default anyways) */
  /* Do not enable pullups on the I2C pins, they are there externally. */
  /* PORTD |= _BV(PD0) | _BV(PD1); */
}

static void waitforcompl(void)
{
  uint8_t abortctr = 0;
  while ((TWCR & _BV(TWINT)) == 0) {
    abortctr++; /* This is a needed workaround, else we'll just get stuck */
    if (abortctr > 200) { /* whenever a slave doesn't feel like ACKing. */
      console_printchar('!');
      console_printchar('W');
      break;
    }
  }
}

void twi_open(uint8_t addr)
{
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* send start condition */
  waitforcompl();
  TWDR = addr;
  TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
  waitforcompl();
}

void twi_close(void)
{
  /* send stop condition */
  TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
  /* no waitforcompl(); after stop! TWINT is not set after STOP has been sent! */
  /* Instead wait for the transmission of the STOP */
  uint8_t abortctr = 0;
  while ((TWCR & _BV(TWSTO))) {
    abortctr++;
    if (abortctr > 200) {
      console_printchar('!');
      console_printchar('C');
      break;
    }
  }
  TWCR = _BV(TWINT); /* Disable TWI completely, it will be reenabled next open */
}

void twi_write(uint8_t what)
{
  TWDR = what;
  TWCR = _BV(TWINT) | _BV(TWEN); /* clear interrupt to start transmission */
  waitforcompl();
}

uint8_t twi_read(uint8_t ack)
{
  /* clear interrupt flag to start receiving. */
  TWCR = _BV(TWINT) | _BV(TWEN) | ((ack) ? _BV(TWEA) : 0x00);
  waitforcompl();
  return TWDR;
}
