/* Test Code: Drive UART1 at 31250 Baud and send MIDI messages */

#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/uart.h"

 // leave uart0 free for stdio
#define MIDI_UART uart1  // MIDI on UART1, to leave UART0 free for debug support
#define BAUD_RATE 31250  // 31250 for MIDI
#define UART_1_TX_PIN 8  // NOTE: GP8, which is pin-11. Could also be GP4 (pin-6)
#define UART_1_RX_PIN 9  // NOTE: GP9, which is pin-12. Could also be GP5 (pin-7)

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

static int chars_rxed = 0;

// RX interrupt handler
void on_uart_rx()
{
    while (uart_is_readable(MIDI_UART))
    {
        uint8_t ch = uart_getc(MIDI_UART);
        // Can we send it back?
        if (uart_is_writable(MIDI_UART))
        {
            // Change it slightly first!
            ch++;
            uart_putc(MIDI_UART, ch);
        }
        chars_rxed++;
    }
}

// main - setup and run loop
int main (void)
{
//#ifdef SER_DBG_ON
    stdio_init_all(); // Enable UARTS - This will set UART0 for debug i/o on USB serial by default
    printf ("\n-- MIDI test starting --\n");
//#endif // SER_DBG_ON

    // Set up our MIDI UART with a basic baud rate.
    uart_init(MIDI_UART, 2400);

    // Turn off CRLF translation
    uart_set_translate_crlf(MIDI_UART, false);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_1_RX_PIN, GPIO_FUNC_UART);

    // Actually, we want a different speed:
    // This will return the actual baud rate selected, which will be as close as possible to requested
    int actual = uart_set_baudrate(MIDI_UART, BAUD_RATE);

    if (BAUD_RATE != actual)
    {
        printf ("MIDI Baud rate mismatch: got %d\n", actual);
    }
    else
    {
        printf ("MIDI Baud rate OK\n");
    }

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(MIDI_UART, false, false);

    // Set our data format: 8N1 here
    uart_set_format(MIDI_UART, DATA_BITS, STOP_BITS, PARITY);

    uart_set_fifo_enabled(MIDI_UART, true); // enable the UART FIFO

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = MIDI_UART == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(MIDI_UART, true, false);

    while (1) // forever loop
    {
        char c = 'U';
        uart_putc_raw(MIDI_UART, c);
    }

    return 0;
} // main

/* End of File */
