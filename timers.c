/* $Id: timers.c $
 * Functions for timekeeping / getting timestamps
 *
 * This internally uses TIMER1.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "timers.h"

volatile uint16_t ticks = 0;

ISR(TIMER1_OVF_vect)
{
  ticks++;
}

uint16_t timers_getticks(void)
{
  uint16_t res;
  cli();
  res = ticks;
  sei();
  return res;
}

uint16_t timers_getticks_noirq(void)
{
  uint16_t res;
  res = ticks;
  return res;
}

void timers_init(void)
{
  /* Normal operation counting from 0 to overflow, nothing special
   * (this is the default anyways) */
  TCCR1A = 0x00;
  /* Select prescaler /256 clock, this results in one overflow every 2.1 seconds */
  TCCR1B = _BV(CS12);
  TIMSK1 |= _BV(TOIE1); /* Enable interrupt on overflow */
  TIFR1 |= _BV(TOV1); /* clear the interrupt flag to be safe none is pending */
}
