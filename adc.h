/* $Id: adc.h $
 * Functions for the analog digital converter - used to measure supply voltage
 */

#ifndef _ADC_H_
#define _ADC_H_

/* General initialization */
void adc_init(void);

/* Turn ADC on or off */
void adc_power(uint8_t p);

/* Start ADC conversion */
void adc_start(void);

/* Read ADC. Will wait for completion of ADC conversion if necessary. */
uint16_t adc_read(void);

#endif /* _ADC_H_ */
