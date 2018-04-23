/* $Id: sds011.h $
 * Functions for talking to the Nova Fitness SDS011 fine particle sensor.
 * The sensor is connected to our serial port.
 * This reuses some code for the serial console of previous projects.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "sds011.h"
#include "console.h"

/* Buffers for input and output */
#define INPUTBUFSIZE 11  /* The SDS011 uses fixed size packets of 19 or 10 Bytes */
static uint8_t inputbuf[INPUTBUFSIZE];
static uint8_t inputpos = 0;
#define OUTPUTBUFSIZE 60 /* Enough to buffer 3 commands, that should be plenty. */
static uint8_t outputbuf[OUTPUTBUFSIZE];
static uint8_t outputhead = 0;
static uint8_t outputtail = 0;
static uint8_t opinprog = 0;

/* Formula for calculating the value of UBRR from baudrate and cpufreq */
#define BAUDRATE 9600UL
#define UBRRCALC ((CPUFREQ / (16UL * BAUDRATE)) - 1)

/* Some commands for the SDS011. Only the relevant part, the first 4 bytes -
 * the rest is the same for all commands anyways (except the CRC which we calc) */
/* Set data reporting to "only when queried" instead of the default "every second" */
static const uint8_t PROGMEM cmd_setdatareporting[] = { 0xB4, 0x02, 0x01, 0x01 };
/* Request the last measured data */
static const uint8_t PROGMEM cmd_requestdata[] = { 0xB4, 0x04, 0x00, 0x00 };
/* Turn measurements on */
static const uint8_t PROGMEM cmd_sensoron[] = { 0xB4, 0x06, 0x01, 0x01 };
/* Turn measurements off */
static const uint8_t PROGMEM cmd_sensoroff[] = { 0xB4, 0x06, 0x01, 0x00 };

/* where we store the values received from the sensor */
static uint16_t pm2_5 = 0xffff; /* 0xffff = "invalid" */
static uint16_t pm10 = 0xffff;

/* Calculate the CRC of a packet.
 * You need to give this the address of the first byte that will be used
 * in the CRC, which is byte 2 (header, tail, and command ID do not go into
 * the CRC). */
static uint8_t calcsds011crc(uint8_t * packet, uint8_t len)
{
  uint8_t res = 0;
  for (uint8_t i = 0; i < len; i++) {
    res += packet[i];
  }
  return res;
}

/* This can only be called safely with interrupts disabled! */
static void appendchar(uint8_t what)
{
  uint8_t newpos;
  newpos = (outputtail + 1);
  if (newpos >= OUTPUTBUFSIZE) {
    newpos = 0;
  }
  if (newpos != outputhead) {
    outputbuf[outputtail] = what;
    outputtail = newpos;
  }
  if (!opinprog) {
    /* Start output, send the byte */
    UDR1 = what;
    outputhead++;
    if (outputhead >= OUTPUTBUFSIZE) {
      outputhead = 0;
    }
    opinprog = 1;
  }
}

static void sendsds011cmd(PGM_P what)
{
  uint8_t crc = 0; uint8_t c;
  appendchar(0xAA);
  for (uint8_t i = 0; i < 4; i++) {
    c = pgm_read_byte(what);
    appendchar(c);
    what++;
    if (i != 0) { /* the 0xB4 is not added to the CRC! */
      crc += c;
    }
  }
  for (uint8_t i = 0; i < 10; i++) {
    appendchar(0x00);
    /* The 0 doesn't add anything to the CRC */
  }
  appendchar(0xff);
  crc += 0xff;
  appendchar(0xff);
  crc += 0xff;
  appendchar(crc);
  appendchar(0xAB);
}

static void processsdsdata(void)
{
  if (inputbuf[1] == 0xC0) { /* Sensor data */
    pm2_5 = ((uint16_t)inputbuf[3] << 8) | inputbuf[2];
    pm10 = ((uint16_t)inputbuf[5] << 8) | inputbuf[4];
  }
}

/* Handler for TXC (TX Complete) IRQ */
ISR(USART1_TX_vect)
{
  if (outputhead == outputtail) { /* Nothing more to send! */
    opinprog = 0;
  } else {
    UDR1 = outputbuf[outputhead];
    outputhead++;
    if (outputhead >= OUTPUTBUFSIZE) {
      outputhead = 0;
    }
  }
}

/* Handler for RXC (RX Complete) IRQ. */
ISR(USART1_RX_vect)
{
  uint8_t inpb;

  inpb = UDR1;
  /* console_printpgm_noirq_P(PSTR(" R"));
  console_printhex8_noirq(inpb); */
  if (inputpos > 0) { /* Has a packet already started? */
    inputbuf[inputpos] = inpb;
    inputpos++;
    if (inputpos == 10) { /* This should be the end of the packet */
      if (inpb == 0xAB) { /* Looking good */
        uint8_t crc = calcsds011crc(&inputbuf[2], 6);
        /* check "CRC" */
        if (crc == inputbuf[8]) {
          processsdsdata();
        } else {
          /* invalid packet */
          console_printpgm_noirq_P(PSTR("!SDSCRC!"));
        }
      } else {
        console_printpgm_noirq_P(PSTR("!SDSOVFL!"));
      }
      inputpos = 0; /* Start over */
    }
  } else { /* Check if a new packet started */
    if (inpb == 0xAA) { /* This marks the start of a packet */
      inputbuf[inputpos] = inpb;
      inputpos++;
    }
  }
}

void sds011_requestresult(void)
{
  cli();
  sendsds011cmd(cmd_requestdata);
  sei();
}

void sds011_setmeasurements(uint8_t ooo)
{
  cli();
  if (ooo) {
    sendsds011cmd(cmd_sensoron);
  } else {
    sendsds011cmd(cmd_sensoroff);
  }
  sei();
}

uint16_t sds011_getlastpm2_5(void)
{
  uint16_t res;
  cli();
  res = pm2_5;
  sei();
  return res;
}

uint16_t sds011_getlastpm10(void)
{
  uint16_t res;
  cli();
  res = pm10;
  sei();
  return res;
}

void sds011_init(void)
{
  /* Enable pullup on our RX pin, really weird sh*t can happen if that is
   * floating (receiving thousands of "0" bytes) */
  PORTD |= _BV(PD2);
  /* Set Baud Rate */
  UBRR1H = (uint8_t)((UBRRCALC >> 8) & 0xff);
  UBRR1L = (uint8_t)((UBRRCALC >> 0) & 0xff);
  /* clear any possible transmit complete flag */
  UCSR1A = _BV(TXC1);
  /* Set 8 Bit mode, no parity, 1 stop bit, asynchronous */
  UCSR1C = _BV(UCSZ10) | _BV(UCSZ11);
  /* Enable Send and Receive and IRQs */
  UCSR1B = _BV(TXEN1) | _BV(RXEN1) | _BV(TXCIE1) | _BV(RXCIE1);
  /* No CTS / RTS (although this is the poweron default anyways) */
  UCSR1D = 0x00;
  sendsds011cmd(cmd_setdatareporting);
  sendsds011cmd(cmd_sensoroff);
}
