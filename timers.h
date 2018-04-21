/* $Id: timers.h $
 * Functions for timekeeping / getting timestamps
 */

#ifndef _TIMERS_H_
#define _TIMERS_H_

/* General initialization */
void timers_init(void);

/* Gets the current (up-)time in ticks.
 * One tick equals 2.1 seconds of uptime. This overflows after about 38 hours. */
uint16_t timers_getticks(void);

/* The same, but for calling while interrupts are disabled. */
uint16_t timers_getticks_noirq(void);

#endif /* _TIMERS_H_ */
