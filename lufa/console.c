/* This originally was the "VirtualSerial" example from the LUFA library.
 * It has been turned into a proper serial console for foxstaub2018.
 * What follows is the original copyright notice, although probably not enough
 * of the old example remains to warrant that.
 */

#if (defined(SERIALCONSOLE))

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



#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <string.h>

#include "console.h"
#include "Descriptors.h"
#include <LUFA/Drivers/USB/USB.h>
#include "../rfm69.h"


#define INPUTBUFSIZE 30
static uint8_t inputbuf[INPUTBUFSIZE];
static uint8_t inputpos = 0;
#define OUTPUTBUFSIZE 800
static uint8_t outputbuf[OUTPUTBUFSIZE];
static uint16_t outputhead = 0; /* WARNING cannot be modified atomically */
static uint16_t outputtail = 0;
static uint8_t escstatus = 0;
static const uint8_t CRLF[] PROGMEM = "\r\n";
static const uint8_t WELCOMEMSG[] PROGMEM = "\r\n"\
                                   "\r\n ****************"\
                                   "\r\n * foxstaub2018 *"\
                                   "\r\n ****************"\
                                   "\r\n"\
                                   "\r\nAVR-libc: " __AVR_LIBC_VERSION_STRING__ " (" __AVR_LIBC_DATE_STRING__ ")"\
                                   "\r\nSoftware Version 0.1, Compiled " __DATE__ " " __TIME__;
static const uint8_t PROMPT[] PROGMEM = "\r\n# ";

/* external variables */
/* these are defined in main.c and contain our last measured data for output
 * in the status command.
 * Note: Since we execute from interrupt context, we might occasionally
 * get corrupted values in the uint16_t and uint32_ts.
 */
extern uint32_t pktssent;
extern uint32_t pressure;
extern int32_t temperature;
extern uint16_t humidity;
extern uint16_t particulatematter2_5u;
extern uint16_t particulatematter10u;

/* Contains the current baud rate and other settings of the virtual serial port. While this demo does not use
 *  the physical USART and thus does not use these settings, they must still be retained and returned to the host
 *  upon request or the host will assume the device is non-functional.
 *
 *  These values are set by the host via a class-specific request, however they are not required to be used accurately.
 *  It is possible to completely ignore these value or use other settings as the host is completely unaware of the physical
 *  serial link characteristics and instead sends and receives data in endpoint streams.
 */
static CDC_LineEncoding_t LineEncoding = { .BaudRateBPS = 0,
                                           .CharFormat  = CDC_LINEENCODING_OneStopBit,
                                           .ParityType  = CDC_PARITY_None,
                                           .DataBits    = 8                            };


/* Event handler for the USB_Connect event. This indicates that the device is enumerating
 */
void EVENT_USB_Device_Connect(void)
{
  /* Nothing to do? */
}

/* Event handler for the USB_Disconnect event. This indicates that the device is
 * no longer connected to a host
 */
void EVENT_USB_Device_Disconnect(void)
{
  /* Throw away all our buffers. */
  inputpos = 0;
  outputhead = 0;
  outputtail = 0;
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the CDC management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup CDC Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPADDR, EP_TYPE_INTERRUPT, CDC_NOTIFICATION_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_TX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_RX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1);

	/* Reset line encoding baud rate so that the host knows to send new values */
	LineEncoding.BaudRateBPS = 0;
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Process CDC specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case CDC_REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Write the line coding data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&LineEncoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearOUT();
			}

			break;
		case CDC_REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Read the line coding data in from the host into the global struct */
				Endpoint_Read_Control_Stream_LE(&LineEncoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearIN();
			}

			break;
		case CDC_REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* NOTE: Here you can read in the line state mask from the host, to get the current state of the output handshake
				         lines. The mask is read in from the wValue parameter in USB_ControlRequest, and can be masked against the
					 CONTROL_LINE_OUT_* masks to determine the RTS and DTR line states using the following code:
				*/
			}

			break;
	}
}

#if defined __GNUC__
static void appendchar(uint8_t what) __attribute__((noinline));
#endif /* __GNUC__ */
/* This can only be called safely with interrupts disabled - remember that! */
static void appendchar(uint8_t what) {
  uint16_t newpos;
  newpos = (outputtail + 1);
  if (newpos >= OUTPUTBUFSIZE) {
    newpos = 0;
  }
  if (newpos != outputhead) {
    outputbuf[outputtail] = what;
    outputtail = newpos;
  }
}

/* We do all query processing here.
 * This must be called with IRQs disabled.
 */
static void console_inputchar(uint8_t inpb) {
  if (escstatus == 1) {
    if (inpb == '[') {
      escstatus = 2;
    } else {
      escstatus = 0;
      appendchar(7); /* Bell */
    }
    return;
  }
  if (escstatus == 2) {
    switch (inpb) {
    case 'A': /* Up */
            /* Try to restore the last comnmand */
            for (; inputpos > 0; inputpos--) {
              appendchar(8);
            }
            while (inputbuf[inputpos]) {
              appendchar(inputbuf[inputpos++]);
            }
            break;
    case 'B': /* Down */
            /* New empty command line */
            for (; inputpos > 0; inputpos--) {
              appendchar(8);
              appendchar(' ');
              appendchar(8);
            }
            break;
    case 'C': /* Left */
    case 'D': /* Right */
    default:
            appendchar(7); /* Bell */
            break;
    };
    escstatus = 0;
    return;
  }
  /* escstatus == 0, i.e. not in an escape */
  switch (inpb) {
  case 0 ... 7: /* Nonprinting characters. Ignore. */
  case 11 ... 12: /* Nonprinting characters. Ignore. */
  case 14 ... 26: /* Nonprinting characters. Ignore. */
  case 28 ... 31: /* Nonprinting characters. Ignore. */
  case 0x80 ... 0xff: /* Nonprinting characters. Ignore. */
          console_printhex8_noirq(inpb);
          appendchar(7); /* Bell */
          break;
  case 9: /* tab. Should be implemented some day? */
          appendchar(7); /* Bell */
          break;
  case 27: /* Escape */
          escstatus = 1;
          break;
  case 0x7f: /* Weird backspace variant */
  case 8: /* Backspace */
          if (inputpos > 0) {
            inputpos--;
            appendchar(8);
            appendchar(' ');
            appendchar(8);
          }
          break;
  case '\r': /* 13 */
  case '\n': /* 10 */
          if (inputpos == 0) {
            console_printpgm_noirq_P(PROMPT);
            break;
          }
          inputbuf[inputpos] = 0; /* Null-terminate the string */
          console_printpgm_noirq_P(CRLF);
          /* now lets see what it is */
          if        (strcmp_P(inputbuf, PSTR("help")) == 0) {
            console_printpgm_noirq_P(PSTR("Available commands:"));
            console_printpgm_noirq_P(PSTR("\r\n motd             repeat welcome message"));
            console_printpgm_noirq_P(PSTR("\r\n showpins [x]     shows the avrs inputpins"));
            console_printpgm_noirq_P(PSTR("\r\n status           show status / counters"));
          } else if (strcmp_P(inputbuf, PSTR("motd")) == 0) {
            console_printpgm_noirq_P(WELCOMEMSG);
          } else if (strncmp_P(inputbuf, PSTR("showpins"), 8) == 0) {
            uint8_t which = 0;
            uint8_t stal = 2; uint8_t endl = 6;
            if (inputpos == 10) {
                    switch (inputbuf[9]) {
                    case 'a':
                    case 'A':
                            which = 1; break;
                    case 'b':
                    case 'B':
                            which = 2; break;
                    case 'c':
                    case 'C':
                            which = 3; break;
                    case 'd':
                    case 'D':
                            which = 4; break;
                    case 'e':
                    case 'E':
                            which = 5; break;
                    case 'f':
                    case 'F':
                            which = 6; break;
                    };
            }
            if (which) {
                    stal = which;
                    endl = which;
            }
            for (which = stal; which <= endl; which++) {
              uint8_t pstatus;
              switch (which) {
              case 2: pstatus = PINB;
                      console_printpgm_noirq_P(PSTR("PINB: 0x"));
                      break;
              case 3: pstatus = PINC;
                      console_printpgm_noirq_P(PSTR("PINC: 0x"));
                      break;
              case 4: pstatus = PIND;
                      console_printpgm_noirq_P(PSTR("PIND: 0x"));
                      break;
              case 5: pstatus = PINE;
                      console_printpgm_noirq_P(PSTR("PINE: 0x"));
                      break;
              case 6: pstatus = PINF;
                      console_printpgm_noirq_P(PSTR("PINF: 0x"));
                      break;
              default:
                      pstatus = 0;
                      console_printpgm_noirq_P(PSTR("NOPIN: 0x"));
                      break;
              };
              console_printhex8_noirq(pstatus);
              appendchar(' ');
              console_printbin8_noirq(pstatus);
              if (which < endl) {
                appendchar('\r'); appendchar('\n');
              }
            }
          } else if (strcmp_P(inputbuf, PSTR("status")) == 0) {
            uint8_t tmpbuf[20];
            console_printpgm_noirq_P(PSTR("Status / last measured values:\r\n"));
            console_printpgm_noirq_P(PSTR("Packets sent: "));
            sprintf_P(tmpbuf, PSTR("%10lu"), pktssent);
            console_printtext_noirq(tmpbuf);
            console_printpgm_noirq_P(PSTR("\r\n"));
            console_printpgm_noirq_P(PSTR("Pressure: "));
            sprintf_P(tmpbuf, PSTR("%.3f"), (float)pressure / 25600.0);
            console_printtext_noirq(tmpbuf);
            console_printpgm_noirq_P(PSTR(" hPa\r\n"));
            console_printpgm_noirq_P(PSTR("PressRAW: 0x"));
            console_printhex8_noirq((pressure >> 24) & 0xff);
            console_printhex8_noirq((pressure >> 16) & 0xff);
            console_printhex8_noirq((pressure >>  8) & 0xff);
            console_printhex8_noirq((pressure >>  0) & 0xff);
            console_printpgm_noirq_P(PSTR("\r\n"));
            console_printpgm_noirq_P(PSTR("Temperature: "));
            sprintf_P(tmpbuf, PSTR("%.2f"), (float)temperature / 100.0);
            console_printtext_noirq(tmpbuf);
            console_printpgm_noirq_P(PSTR(" degC\r\n"));
            console_printpgm_noirq_P(PSTR("TempRAW: 0x"));
            console_printhex8_noirq((temperature >> 24) & 0xff);
            console_printhex8_noirq((temperature >> 16) & 0xff);
            console_printhex8_noirq((temperature >>  8) & 0xff);
            console_printhex8_noirq((temperature >>  0) & 0xff);
            console_printpgm_noirq_P(PSTR("\r\n"));
            console_printpgm_noirq_P(PSTR("rel. Humidity: "));
            sprintf_P(tmpbuf, PSTR("%3.2f"), (float)humidity / 1024.0);
            console_printtext_noirq(tmpbuf);
            console_printpgm_noirq_P(PSTR("%\r\n"));
            console_printpgm_noirq_P(PSTR("HumRAW: 0x"));
            console_printhex8_noirq((humidity >> 8) & 0xff);
            console_printhex8_noirq((humidity >> 0) & 0xff);
            console_printpgm_noirq_P(PSTR("\r\n"));
            console_printpgm_noirq_P(PSTR("PM2.5: "));
            sprintf_P(tmpbuf, PSTR("%5.1f"), ((float)particulatematter2_5u) / 10.0);
            console_printtext_noirq(tmpbuf);
            console_printpgm_noirq_P(PSTR(" ug/m^3\r\n"));
            console_printpgm_noirq_P(PSTR("PM10:  "));
            sprintf_P(tmpbuf, PSTR("%5.1f"), (float)particulatematter10u / 10.0);
            console_printtext_noirq(tmpbuf);
            console_printpgm_noirq_P(PSTR(" ug/m^3"));
          } else if (strncmp_P(inputbuf, PSTR("rfm69reg"), 8) == 0) {
            uint8_t star = 0x01;
            uint8_t endr = 0x4f;  /* Show all relevant ones by default */
            int regtoshow = -1;
            if (inputpos >= 10) {
              sscanf(&inputbuf[9], "%d", &regtoshow);
              regtoshow = regtoshow & 0x7f;
            }
            if (regtoshow > 0) {
              star = regtoshow;
              endr = regtoshow;
            }
            for (regtoshow = star; regtoshow <= endr; regtoshow++) {
              if (star != endr) { /* if we show a range, skip reserved registers */
                if (regtoshow == 0x0c) { continue; }
                if ((regtoshow >= 0x14) && (regtoshow <= 0x17)) { continue; }
              }
              appendchar('0'); appendchar('x');
              console_printhex8_noirq(regtoshow);
              appendchar(':'); appendchar(' ');
              console_printhex8_noirq(rfm69_readreg(regtoshow));
              appendchar('\r'); appendchar('\n');
            }
          } else {
            console_printpgm_noirq_P(PSTR("Unknown command: "));
            console_printtext_noirq(inputbuf);
          }
          /* show PROMPT and go back to start. */
          console_printpgm_noirq_P(PROMPT);
          inputpos = 0;
          break;
  default:
          if (inputpos < (INPUTBUFSIZE - 1)) { /* -1 for terminating \0 */
                  inputbuf[inputpos++] = inpb;
                  /* Echo the character */
                  appendchar(inpb);
          } else {
                  appendchar(7); /* Bell */
          }
  };
}

/* Function to manage CDC data transmission and reception to and from the host. */
/* call with interrupts disabled! */
void CDC_Task(void)
{
  /* Device must be connected and configured for the task to run */
  if (USB_DeviceState != DEVICE_STATE_Configured)
    return;

  /* Select the Serial Rx Endpoint */
  Endpoint_SelectEndpoint(CDC_RX_EPADDR);

  if (Endpoint_IsOUTReceived()) { /* Do we have output to read? Then read it. */
    uint8_t errorcode;
    uint8_t inp[2];
    uint8_t i;
    uint8_t bytestoread = Endpoint_BytesInEndpoint();
    for (i = 0; i < bytestoread; i++) {
      errorcode = Endpoint_Read_Stream_LE(inp, 1, NULL);
      if (errorcode != ENDPOINT_RWSTREAM_NoError) {
        break;
      }
      console_inputchar(inp[0]);
    }
    Endpoint_ClearOUT();
  }

  /* Select the Serial Tx Endpoint */
  Endpoint_SelectEndpoint(CDC_TX_EPADDR);
  /* Do we have anything to send, and can we send? */
  if ((outputhead != outputtail) && (Endpoint_IsINReady())) {
    uint8_t whattosend[CDC_TXRX_EPSIZE];
    uint8_t bytestosend = 0;
    /* Note: We send one byte less than the maximum on purpose, because if the
     * host would receive a maximum sized packet, it would wait for more to
     * follow. */
    while ((bytestosend < (CDC_TXRX_EPSIZE - 1)) && (outputhead != outputtail)) {
      whattosend[bytestosend] = outputbuf[outputhead];
      outputhead++;
      if (outputhead >= OUTPUTBUFSIZE) {
        outputhead = 0;
      }
      bytestosend++;
    }
    Endpoint_Write_Stream_LE(whattosend, bytestosend, NULL);
    Endpoint_ClearIN();
  }
}

void console_printchar_noirq(uint8_t what) {
  appendchar(what);
}

/* This can only be called safely with interrupts disabled - remember that! */
void console_printhex8_noirq(uint8_t what) {
  uint8_t buf;
  uint8_t i;
  for (i=0; i<2; i++) {
    buf = (uint8_t)(what & (uint8_t)0xF0) >> 4;
    if (buf <= 9) {
      buf += '0';
    } else {
      buf += 'A' - 10;
    }
    appendchar(buf);
    what <<= 4;
  }
}

/* This can only be called safely with interrupts disabled - remember that! */
void console_printdec_noirq(uint8_t what) {
  uint8_t buf;
  buf = what / 100;
  appendchar(buf + '0');
  what %= 100;
  buf = what / 10;
  appendchar(buf + '0');
  buf = what % 10;
  appendchar(buf + '0');
}

/* This can only be called safely with interrupts disabled - remember that! */
/* This is the same as printec, but only prints 2 digits (e.g. for times/dates) */
void console_printdec2_noirq(uint8_t what) {
  if (what > 99) { what = 99; }
  appendchar((what / 10) + '0');
  appendchar((what % 10) + '0');
}

/* This can only be called safely with interrupts disabled - remember that! */
void console_printbin8_noirq(uint8_t what) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    if (what & 0x80) {
      appendchar('1');
    } else {
      appendchar('0');
    }
    what <<= 1;
  }
}

/* This can only be called safely with interrupts disabled - remember that! */
void console_printtext_noirq(const uint8_t * what) {
  while (*what) {
    appendchar(*what);
    what++;
  }
}

/* This can only be called safely with interrupts disabled - remember that! */
void console_printpgm_noirq_P(PGM_P what) {
  uint8_t t;
  while ((t = pgm_read_byte(what++))) {
    appendchar(t);
  }
}

/* These are wrappers for our internal functions, disabling IRQs before
 * calling them. */
void console_printchar(uint8_t what) {
  cli();
  console_printchar_noirq(what);
  sei();
}

void console_printtext(const uint8_t * what) {
  cli();
  console_printtext_noirq(what);
  sei();
}

void console_printpgm_P(PGM_P what) {
  cli();
  console_printpgm_noirq_P(what);
  sei();
}

void console_printhex8(uint8_t what) {
  cli();
  console_printhex8_noirq(what);
  sei();
}

void console_printdec(uint8_t what) {
  cli();
  console_printdec_noirq(what);
  sei();
}

/* Initialize ourselves. Must be called with interrupts still disabled! */
void console_init(void)
{
  USB_Init();
  console_printpgm_noirq_P(WELCOMEMSG);
  console_printpgm_noirq_P(PROMPT);
}

void console_work(void)
{
  cli();
  CDC_Task();
  sei();
}

uint8_t console_isusbconfigured(void) {
  return (USB_DeviceState == DEVICE_STATE_Configured);
}

#else /* SERIALCONSOLE */

void console_init(void) { }
void console_work(void) { }
uint8_t console_isusbconfigured(void) { return 0; }
void console_printchar_noirq(uint8_t c) { }
void console_printchar(uint8_t c) { sei(); }
void console_printtext(const uint8_t * what) { sei(); }
void console_printpgm_P(PGM_P what) { sei(); }
void console_printhex8(uint8_t what) { sei(); }
void console_printdec(uint8_t what) { sei(); }

#endif /* SERIALCONSOLE */
