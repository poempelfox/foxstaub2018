/* $Id: lps25hb.h $
 * Functions for reading the LPS 25 HB pressure sensor
 */

#ifndef _LPS25HB_H_
#define _LPS25HB_H_

struct lps25hbdata {
  uint8_t pressure[3];
  uint8_t valid;
};

/* Initialize LPS 25 HB */
void lps25hb_init(void);

/* Start (oneshot) measurement */
void lps25hb_startmeas(void);

/* Read result of measurement. Needs to be called no earlier than ?? ms
 * after starting. */
void lps25hb_read(struct lps25hbdata * d);

#endif /* _LPS25HB_H_ */
