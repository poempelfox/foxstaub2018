/* $Id: rfm69.c $
 * Functions for communication with RF12 module
 *
 * This code was heavily copy+pasted from RFMxx.cpp in the FHEM LaCrosseIT+
 * Jeenode Firmware Code.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>
#include "rfm69.h"
#include "lufa/console.h"

/* Pin mappings:
 *  SS     PB4
 *  MOSI   PB2
 *  MISO   PB3
 *  SCK    PB1
 *  RESET  PD4
 *  OUROWNSS PB0 (not used!)
 */
#define RFMDDR   DDRB
#define RFMPIN   PINB
#define RFMPORT  PORTB

#define RFMPIN_SS    PB4
#define RFMPIN_MOSI  PB2
#define RFMPIN_MISO  PB3
#define RFMPIN_SCK   PB1
#define RFMPIN_OURSS PB0

#define RFM_FREQUENCY 868300UL
#define RFM_DATARATE 17241.0

#define PAYLOADSIZE 64

ISR(INT6_vect)
{
  /* Nothing to do here. */
}

ISR(SPI_STC_vect)
{
  /* Nothing to do here. */
}

/* Note: Internal use only. Does not set the SS pin, the calling function
 * has to do that! */
static uint8_t rfm69_spi8(uint8_t value) {
  SPDR = value;
  /* busy-wait for transmission. this loop spins 4 usec with a 2 MHz SPI clock */
  while (!(SPSR & _BV(SPIF))) { }

  return SPDR;
}

uint16_t rfm69_spi16(uint16_t value) {
  _delay_us(1);
  RFMPORT &= (uint8_t)~_BV(RFMPIN_SS);
  _delay_us(1);
  uint16_t reply = rfm69_spi8(value >> 8) << 8;
  reply |= rfm69_spi8(value & 0xff);
  _delay_us(1);
  RFMPORT |= _BV(RFMPIN_SS);
  _delay_us(1);
  
  return reply;
}

uint8_t rfm69_readreg(uint8_t reg) {
  return rfm69_spi16(((uint16_t)(reg & 0x7f) << 8) | 0x00) & 0xff;
}

static void rfm69_writereg(uint8_t reg, uint8_t val) {
  rfm69_spi16(((uint16_t)(reg | 0x80) << 8) | val);
}

void rfm69_clearfifo(void) {
  /* There is no need for reading / ORing the register here because all
   * bits except the FiFoOverrun-bit we set to clear the FIFO are read-only */
  rfm69_writereg(0x28, (1 << 4)); /* RegIrqFlags2 */
}

void rfm69_settransmitter(uint8_t e) {
  if (e) {
    /* RegOpMode => TRANSMIT */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x0C);
  } else {
    /* RegOpMode => STANDBY */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x04);
  }
}

void rfm69_setsleep(uint8_t s) {
  if (s) {
    /* RegOpMode => SLEEP */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x00);
  } else {
    /* RegOpMode => STANDBY */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x04);
    while (!(rfm69_readreg(0x27) & 0x80)) { /* Wait until ready */ }
  }
}

void rfm69_sendarray(uint8_t * data, uint8_t length) {
  /* Set the length of our payload */
  rfm69_writereg(0x38, length);
  rfm69_clearfifo(); /* Clear the FIFO */
  /* Now fill the FIFO. We manually set SS and use spi8 because this
   * is the only "register" that is larger than 8 bits. */
  _delay_us(1);
  RFMPORT &= (uint8_t)~_BV(RFMPIN_SS);
  _delay_us(1);
  rfm69_spi8(0x80); /* Select RegFifo (0x00) for writing (|0x80) */
  for (int i = 0; i < length; i++) {
    rfm69_spi8(data[i]);
  }
  _delay_us(1);
  RFMPORT |= _BV(RFMPIN_SS);
  _delay_us(1);
  /* FIFO has been filled. Tell the RFM69 to send by just turning on the transmitter. */
  rfm69_settransmitter(1);
  /* Wait for transmission to finish, visible in RegIrqFlags2. */
  uint8_t reg28 = 0x00;
  uint16_t maxreps = 10000;
  while (!(reg28 & 0x08)) {
    reg28 = rfm69_readreg(0x28);
    maxreps--;
    if (maxreps == 0) {
      console_printpgm_P(PSTR("![TX TIMED OUT]!"));
      break;
    }
  }
  rfm69_settransmitter(0);
}

void rfm69_initport(void) {
  /* Configure Pins for output / input */
  /* on the feather, the RESET pin of the RFM is connected to PD4. Trigger a
   * reset by pulling the RESET pin HIGH for at least 100us. */
  DDRD |= _BV(PD4);
  PORTD |= _BV(PD4);
  /* Now the rest of the I/O pins */
  RFMDDR |= _BV(RFMPIN_MOSI);
  RFMDDR &= (uint8_t)~_BV(RFMPIN_MISO);
  RFMDDR |= _BV(RFMPIN_SCK);
  RFMDDR |= _BV(RFMPIN_SS);
  /* We do not really use the SS pin of the AVR, but if it were pulled low,
   * the AVR would drop out of master mode! To avoid this, we configure it as
   * output pin. */
  RFMDDR |= _BV(RFMPIN_OURSS);

  RFMPORT |= _BV(RFMPIN_SS);
  
  /* Enable hardware SPI, no need to manually do it.
   * set master mode with rate clk/4 = 2 MHz (maximum of RFM69 is unknown) */
  SPCR = _BV(SPE) | _BV(MSTR);
  SPSR = 0x00; /* To set SPI2X to 0, rest is read-only anyways */
  
  _delay_us(200); /* 100us minimum time the RESET pin needs to be pulled high on the RFM */
  PORTD &= (uint8_t)~_BV(PD4);
}

void rfm69_initchip(void) {
  /* RegOpMode -> standby. */
  rfm69_writereg(0x01, 0x00 | 0x04);
  /* RegDataModul -> PacketMode, FSK, Shaping 0 */
  rfm69_writereg(0x02, 0x00);
  /* RegFDevMsb / RegFDevLsb -> 0x05C3 (90 kHz). */
  rfm69_writereg(0x05, 0x05);
  rfm69_writereg(0x06, 0xC3);
  /* RegPaLevel -> Pa0=0 Pa1=1 Pa2=0 Outputpower=31 -> 13 dbM */
  rfm69_writereg(0x11, 0x5F);
  /* RegPaRamp -> 0x0c = 20us   default = 0x09 = 40us */
  /* rfm69_writereg(0x12, 0x0c); */
  /* RegOcp -> defaults (jeelink-sketch sets 0 but that seems wrong) */
  rfm69_writereg(0x13, 0x1a);
  /* RegRxBw -> DccFreq 010   Mant 16   Exp 2 - this is a receiver-register,
   * we do not really care about it */
  rfm69_writereg(0x19, 0x42);
  /* RegDioMapping2 -> disable clkout (but thats the default anyways) */
  rfm69_writereg(0x26, 0x07);
  /* RegIrqFlags2 (0x28): some status flags, writing a 1 to FIFOOVERRUN bit
   * clears the FIFO. This is what clearfifo() does. */
  rfm69_clearfifo();
  /* RegRssiThresh -> 220 */
  rfm69_writereg(0x29, 220);
  /* RegPreambleMsb / Lsb - we want 3 bytes of preamble (0xAA) */
  rfm69_writereg(0x2C, 0x00);
  rfm69_writereg(0x2D, 0x03);
  /* RegSyncConfig -> SyncOn FiFoFillAuto SyncSize=2 SyncTol=0 */
  rfm69_writereg(0x2E, 0x88);
  /* RegSyncValue1/2 (3-8 exist too but we only use 2 so do not need to set them) */
  rfm69_writereg(0x2F, 0x2D);
  rfm69_writereg(0x30, 0xD4);
  /* RegPacketConfig1 -> FixedPacketLength CrcOn=0 */
  rfm69_writereg(0x37, 0x00);
  /* RegPayloadLength -> 0
   * This selects between two different modes: "0" means "Unlimited length
   * packet format", any other value "Fixed Length Packet Format" (with that
   * length). We set 0 for now, but actually fill the register before sending.
   */
  rfm69_writereg(0x38, 0x0c);
  /* RegFifoThreshold -> TxStartCond=1 value=0x0f */
  rfm69_writereg(0x3C, 0x8F);
  /* RegPacketConfig2 -> AesOn=0 and AutoRxRestart=1 even if we do not care about RX */
  rfm69_writereg(0x3D, 0x12);
  /* RegTestDagc -> improvedlowbeta0 - I haven't got the faintest... */
  /* rfm69_writereg(0x6F, 0x30); */
  /* Set Frequency */
  /* The datasheet is horrible to read at that point, never stating a clear
   * formula ready for use. */
  /* F(Step) = F(XOSC) / (2 ** 19)      2 ** 19 = 524288
   * F(forreg) = FREQUENCY_IN_HZ / F(Step) */
  uint32_t freq = round((1000.0 * RFM_FREQUENCY) / (32000000.0 / 524288.0));
  /* (Alternative calculation from JeeNode library:)
   * Frequency steps are in units of (32,000,000 >> 19) = 61.03515625 Hz
   * use multiples of 64 to avoid multi-precision arithmetic, i.e. 3906.25 Hz
   * due to this, the lower 6 bits of the calculated factor will always be 0
   * this is still 4 ppm, i.e. well below the radio's 32 MHz crystal accuracy */
  /* uint32_t freq = (((RFM_FREQUENCY * 1000) << 2) / (32000000UL >> 11)) << 6; */
  rfm69_writereg(0x07, (freq >> 16) & 0xff);
  rfm69_writereg(0x08, (freq >>  8) & 0xff);
  rfm69_writereg(0x09, (freq >>  0) & 0xff);
  /* Set Datarate - the whole floating point mess seems to be optimized out 
   * by gcc at compile time, thank god. */
  uint16_t dr = (uint16_t)round(32000000.0 / RFM_DATARATE);
  rfm69_writereg(0x03, (dr >> 8));
  rfm69_writereg(0x04, (dr & 0xff));

  rfm69_clearfifo();
}
