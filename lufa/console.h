/* This originally was the "VirtualSerial" example from the LUFA library.
 * It has been turned into a proper serial console for foxgeig2018.
 * What follows is the original copyright notice, although probably not enough
 * of the old example remains to warrant that.
 */
/*
             LUFA Library
     Copyright (C) Dean Camera, 2017.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2017  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <avr/pgmspace.h>

/* USB functions (not to be called by application) */
void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);

/* Init needs to be called with IRQs still disabled! */
void console_init(void);
/* Needs to be called regulary to handle pending USB work */
void console_work(void);
/* Check if we're connected to a PC. */
uint8_t console_isusbconfigured(void);

/* These need to be called with IRQs disabled! They are usually NOT what
 * you want. */
void console_printchar_noirq(uint8_t c);
void console_printtext_noirq(const uint8_t * what);
void console_printpgm_noirq_P(PGM_P what);
void console_printhex8_noirq(uint8_t what);
void console_printdec_noirq(uint8_t what);
void console_printbin8_noirq(uint8_t what);

/* These can be called with interrupts enabled (and will reenable them!) */
void console_printchar(uint8_t c);
void console_printtext(const uint8_t * what);
void console_printpgm_P(PGM_P what);
void console_printhex8(uint8_t what);
void console_printdec(uint8_t what);

#endif
