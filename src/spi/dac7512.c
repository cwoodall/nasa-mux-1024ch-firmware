/*
 * dac7512.c
 *
 *  Created on: Jun 21, 2013
 *      Author: Christopher Woodall
 */
#include <stdint.h>
#include "spi/spi.h"
#include "spi/dac7512.h"
#include <msp430.h>

#define DAC7512_ADDRL_OUT  (P2OUT)
#define DAC7512_ADDRH_OUT  (P1OUT)
#define DAC7512_ADDRL_MASK (0xff)
#define DAC7512_ADDRH_MASK (0x18)
#define DAC7512_ADDRL_SET(addr) (DAC7512_ADDRL_OUT = (DAC7512_ADDRL_OUT & ~ DAC7512_ADDRL_MASK) | ((addr)&0xff))
#define DAC7512_ADDRH_SET(addr) (DAC7512_ADDRH_OUT = (DAC7512_ADDRH_OUT & ~ DAC7512_ADDRH_MASK) | ((addr)&0x300)>>5)

__inline void DAC7512_set_addr(uint16_t addr) {
  DAC7512_ADDRL_SET(addr);
  DAC7512_ADDRH_SET(addr);
}

__inline void DAC7512_unset_addr() {
  DAC7512_ADDRL_OUT &= ~DAC7512_ADDRL_MASK;
  DAC7512_ADDRH_OUT &= ~DAC7512_ADDRH_MASK;
}
void DAC7512_addr_init() {
  P2DIR |= DAC7512_ADDRL_MASK;
  P2SEL = 0;
  P1DIR |= DAC7512_ADDRH_MASK;
}
void DAC7512_init(spi_device_t *dev) {
  DAC7512_addr_init();
  // SETUP SPI DEVICE
  dev->bits = 16;
  dev->spi_mode = SPI_MODE_2;
  dev->delay_cycles = 10;
  dev->chip_select  = *DAC7512_chip_select;
  dev->chip_release = *DAC7512_chip_release;
  DAC7512_chip_release(0);
}

void DAC7512_send(spi_device_t *dev, uint16_t data, uint16_t addr) {
  // Row is bits 16 downto 8 (where each bit represents a row)
  // Col is bits 7 downto 0 (where each bit represents a column)
  SPI_send(dev, data, addr);
}

int16_t DAC7512_chip_select(uint16_t cs_arg) {
  // ROW[15:8] | COL[7:0]
  DAC7512_set_addr(0x3ff);
  DAC7512_set_addr(cs_arg);
  return 0;
}

int16_t DAC7512_chip_release(uint16_t cs_arg) {
  DAC7512_set_addr(0x3ff);
  DAC7512_unset_addr();
  return 0;
}
