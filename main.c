//***************************************************************************************
//  MSP430G2553 Based Controller board for the 1024 Channel Setup.
//  For use with the Controller setup designed by Jeff Kittredge and
//  4 256 channel ADC boards designed by David Freedman.
//
//  C. Woodall < cwoodall at bu.edu >
//  Boston University, NASA MUX
//  November 2013
//  Built with Code Composer Studio v5
//***************************************************************************************
#define __DEBUG__

#include <msp430.h>
#include <stdint.h>

#include "types/vector/vector_uint8.h"
#include "uart/uart.h"
#include "uart/mux_proto.h"
#include "spi/spi.h"
#include "spi/dac7512.h"
#include "crc16/crc16.h"
#include "types/vector/vector_uint8.h"

#define TTL_TOGGLE_OUTPUT BIT0
#define TTL_TOGGLE_DIR    P1DIR
#define TTL_TOGGLE_POUT   P1OUT

// Initialize the state and message
static uint16_t settings_reg = 0x0001;
static uint8_t ignore_crc = 0;
static uart_dev_t uart_dev;
static spi_device_t dac7512;
//static uint16_t dac7512_value_reg[1024];

/**
static uint16_t dac7512_step_reg[16] = {
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0
};

static uint16_t dac7512_counter_reg[16] = {
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0
};
**/

void main(void)
{
  /** Disable Watchdog Timer and Setup Clocks **/
  WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer
  BCSCTL1 = CALBC1_16MHZ;		// Run at 16 MHz
  DCOCTL = CALDCO_16MHZ;		// Run at 16 MHz

  /** Initialize BIT0 as the TTL output for toggling. **/
  TTL_TOGGLE_DIR |= TTL_TOGGLE_OUTPUT;

  /** Initialize SPI peripheral and DAC7512 devices **/
  SPI_init();
  DAC7512_init(&dac7512);

  /* commented to disable TimerA
  TA1CCR0 = 100; // Set timer to 100 time length intervals.
  //Set TimerA to use auxiliary clock in UP mode
  TA1CTL = TASSEL_2 | ID_2 | MC_1; // Select the SM_CLK which is running at 16MHz/4
  //Enable the interrupt for TACCR0 match
  TA1CCTL0 = CCIE;
  */
  uint16_t i;
  /** Initialize all DAC7512's to initial values. **/
  for (i = 0; i < 1024; i++) {
    DAC7512_send(&dac7512, 0, i);
  }

  /** Initialize UART **/
  uart_dev_init(&uart_dev);
  setupUART(9600, UART_RXIE); // N = f_clk/Baudrate, Baudrate == 9600

  /** Interrupts **/
  __bis_SR_register(GIE); // interrupts enabled

  while ( 1 ) { };
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
	uint8_t rx_read;
	// Handle a UART Rx Interrupt
	if (IFG2 & UCA0RXIFG) {
		// Read in from UART peripheral and echo (for debug... will be removed)
		rx_read = UCA0RXBUF;

		switch ( uart_dev.state ) {
		case IDLE:
			if (rx_read == UART_MAGIC_FRAME_START) {
				uart_dev.state = MESSAGE;
				vector_uint8_clear(&uart_dev.rxbuf);
				crc_init(&(uart_dev.rxcrc));
			}
			break;
		case MESSAGE:
			if (rx_read == UART_MAGIC_ESCAPE) {
				uart_dev.state = ESCAPE;
				crc_add_byte( &(uart_dev.rxcrc), rx_read);
			} else if (rx_read == UART_MAGIC_FRAME_END) {
				if ( crc_check(uart_dev.rxcrc) || ignore_crc ) {
					// Remove CRC
				  uart_dev.rxbuf.end -= 2;
					// Find command
					switch (vector_uint8_get(&uart_dev.rxbuf, 0)) {
					case UART_COMMANDS_WR_REG: // Write Register command
						// rxbuf should be 5 long (command + addr + data)
						if (uart_dev.rxbuf.end == 5) {
						  uint16_t addr = (uint16_t)((uint16_t) vector_uint8_get(&uart_dev.rxbuf, 1)<<8) | vector_uint8_get(&uart_dev.rxbuf, 2);
						  uint16_t value = (uint16_t)(((uint16_t)vector_uint8_get(&uart_dev.rxbuf, 3) << 8) | vector_uint8_get(&uart_dev.rxbuf, 4));
							if (addr == 0x0000) {
								settings_reg = value;
								uart_dev_send_ack(&uart_dev);
							} else if ((addr & 0xf000) == 0x1000) {
							  // DAC7512
                DAC7512_send(&dac7512, value, addr & 0x03ff);
                uart_dev_send_ack(&uart_dev);
							} else {
								uart_dev_send_err(&uart_dev, UART_ERRORS_BAD_ADDRESS);
							}
						} else {
							uart_dev_send_err(&uart_dev, UART_ERRORS_BAD_PACKET);
						}
						break;
					case UART_COMMANDS_READ_REG:
						// rxbuf should be 2 long (command + address)
						if (uart_dev.rxbuf.end == 2) {
              uint8_t addr = vector_uint8_get(&uart_dev.rxbuf, 1);

							if (addr == 0x00) {
								uart_dev_send_read_ack(&uart_dev, settings_reg);
             // } else if (addr & 0x20) {
     //           uart_dev_send_read_ack(&uart_dev, dac7512_value_reg[addr & 0x0f]);
							} else {
								uart_dev_send_err(&uart_dev, UART_ERRORS_BAD_ADDRESS);
							}
						} else {
							uart_dev_send_err(&uart_dev, UART_ERRORS_BAD_PACKET);
						}
						break;
					case UART_COMMANDS_ACK: // ACK back to ACKs... basically a cheap ping.
						uart_dev_send_ack(&uart_dev);
						break;
					case UART_COMMANDS_ERR: // DO NOT REACT TO ERRORS
						break;
					case UART_COMMANDS_DISABLE_CRC: // IGNORE CRC
            uart_dev_send_read_ack(&uart_dev, 0xDEAD);
            ignore_crc = 1;
            break;
          case UART_COMMANDS_ENABLE_CRC: // IGNORE CRC
            uart_dev_send_read_ack(&uart_dev, 0xBEEF);
            ignore_crc = 0;
            break;
					default:
						uart_dev_send_err(&uart_dev, UART_ERRORS_GENERAL);
						break;
					}
				} else {
					uart_dev_send_err(&uart_dev, UART_ERRORS_CRC);
				}
				uart_dev.state = IDLE;
			} else if (rx_read == UART_MAGIC_FRAME_START) {
				// If we have a spurious frame start, we have a FRAME error. Send a frame error then resync.
        vector_uint8_clear(&uart_dev.rxbuf);
        crc_init(&(uart_dev.rxcrc));
			  uart_dev_send_err(&uart_dev, UART_ERRORS_FRAME);
				uart_dev.state = MESSAGE;
			} else {
				vector_uint8_push_back(&(uart_dev.rxbuf), rx_read);
				crc_add_byte( &(uart_dev.rxcrc), rx_read);
			}
			break;
		case ESCAPE:
			vector_uint8_push_back(&(uart_dev.rxbuf), rx_read);
			crc_add_byte( &(uart_dev.rxcrc), rx_read);
			uart_dev.state = MESSAGE;
			break;
		}
	}
}

/* comment out toggle code
 * // FIXME: Rework math to be more exact on how to calculate the oscillation frequency
 * #pragma vector=TIMER1_A0_VECTOR
 * __interrupt void Timer1_A0(void) {
 *   static uint16_t counter = 0;
 *   static uint8_t is_stepped = 0;
 *
 *   if (dac7512_counter_reg[0] < 2) { // NOTE: Having a counter value of 1 is removes your ability to maintain a good UART connection.
 *     // Hold the counter at 0 if 0x40 (dac_7512_counter_reg[0]) is not set.
 *     counter = 0;
 *     P1OUT &= ~BIT0; // Hold the LED at 0
 *
 *   } else if (counter >= dac7512_counter_reg[0]) {
 *     // If we are stepped we turn on the digital output and send the message + the step.
 *     if (is_stepped) {
 *       DAC7512_send(&dac7512, dac7512_value_reg[0]+dac7512_step_reg[0], 0, 0);
 *       P1OUT |= BIT0;
 *       is_stepped = 0;
 *     } else {
 *       // Otherwise turn the DAC back down to its non stepped voltage and turn off the TTL output.
 *       DAC7512_send(&dac7512, dac7512_value_reg[0], 0, 0);
 *       P1OUT &= ~BIT0;
 *       is_stepped = 1;
 *     }
 *     counter = 0; // reset counter
 *   } else {
 *     counter++; // increment counter.
 *   }
 * }
 */
