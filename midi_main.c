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

static int global_vol = 70;
static int last_vol = 70;

static int global_voice = 0;
static int last_voice = 0;

// UART0 RX interrupt handler /////////////////////////////////////
void v_uart_Rx_isr()
{
    while (uart_is_readable(uart0))
    {
        uint8_t ch = uart_getc(uart0);

        if (ch == 'V')
        {
            global_vol++;
            if (global_vol > 127) global_vol = 127;
        }
        else if (ch == 'v')
        {
            global_vol--;
            if (global_vol < 0) global_vol = 0;
        }

        if (ch == 'P')
        {
            global_voice++;
            if (global_voice > 127) global_voice = 127;
        }
        else if (ch == 'p')
        {
            global_voice--;
            if (global_voice < 0) global_voice = 0;
        }
    }
}

// MIDI RX interrupt handler /////////////////////////////////////
void v_midi_Rx_isr()
{
    while (uart_is_readable(MIDI_UART))
    {
        uint8_t ch = uart_getc(MIDI_UART);
        (void)ch;
        // Can we send it back?
//        if (uart_is_writable(MIDI_UART))
//        {
//            // Change it slightly first!
//            ch++;
//            uart_putc(MIDI_UART, ch);
//        }
//        chars_rxed++;
    }
}

#undef SER_DBG_ON
#ifdef SER_DBG_ON
#endif // SER_DBG_ON

// main - setup and run loop //////////////////////////////
int main (void)
{
    stdio_init_all(); // Enable UARTS - This will set UART0 for debug
#ifdef SER_DBG_ON
    printf ("\n-- MIDI test starting --\n");
#endif // SER_DBG_ON

    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Set up our MIDI UART with a basic baud rate.
    uart_init(MIDI_UART, 2400);

    // Turn off CRLF translation
    uart_set_translate_crlf(MIDI_UART, false);
    uart_set_translate_crlf(uart0, false);

    // Set the TX and RX pins by using the function select on the GPIO
    // See datasheet for more information on function select
    gpio_set_function(UART_1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_1_RX_PIN, GPIO_FUNC_UART);

    // Actually, we want a different speed:
    // This will return the actual baud rate selected, which will be as close as possible to requested
    int actual = uart_set_baudrate(MIDI_UART, BAUD_RATE);

    if (BAUD_RATE != actual)
    {
#ifdef SER_DBG_ON
        printf ("MIDI Baud rate mismatch: got %d\n", actual);
#endif // SER_DBG_ON
    }
    else
    {
#ifdef SER_DBG_ON
        printf ("MIDI Baud rate OK\n");
#endif // SER_DBG_ON
    }

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(MIDI_UART, false, false);

    // Set our data format: 8N1 here
    uart_set_format(MIDI_UART, DATA_BITS, STOP_BITS, PARITY);

    uart_set_fifo_enabled(MIDI_UART, true); // enable the UART FIFO (FIFO is 32 deep)

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int MIDI_IRQ = (MIDI_UART == uart0) ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(MIDI_IRQ, v_midi_Rx_isr);
    irq_set_enabled(MIDI_IRQ, true);

    irq_set_exclusive_handler(UART0_IRQ, v_uart_Rx_isr);
    irq_set_enabled(UART0_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(MIDI_UART, true, false);

    uart_set_irq_enables(uart0, true, false);

    // Wait until things start
    sleep_ms (500);

    {
        // Print test
        for (int idx = 0; idx < 15; ++idx)
        {
            uart_putc_raw(uart0, 10);
        }
    }

    // Send all notes off
    char chan_mode = 0xB0; // Channel Mode
    char all_off = 123;
    uart_putc_raw(MIDI_UART, chan_mode);
    uart_putc_raw(MIDI_UART, all_off); //  All Notes Off
    uart_putc_raw(MIDI_UART, 0);

    // Set voice back to default for channel
    uart_putc_raw(MIDI_UART, 0xC0); // Program change for channel 0
    uart_putc_raw(MIDI_UART, 0); // Voice 0

    char note_on = 0x90;  // Note On is 0x9x
    char note_pitch = 60; // 60 decimal == middle-C
    char note_vel = 64;   // Middle volume

    char chan_voice = 0;
    char chan_vol = 70;

    int led_set = 1;
    //int vol_dir = (-1);

    while (1) // forever loop
    {
        // Note on:
        uart_putc_raw(MIDI_UART, note_on);
        uart_putc_raw(MIDI_UART, note_pitch);
        uart_putc_raw(MIDI_UART, note_vel);

        gpio_put(LED_PIN, led_set);

        // sleep for a bit while note sounds
        sleep_ms (250);

        // Note OFF: running status, note-on with velocity 0
        uart_putc_raw(MIDI_UART, note_pitch);
        uart_putc_raw(MIDI_UART, 0);

        // Tweak the channel volume
        if (last_vol != global_vol)
        {
            chan_vol = global_vol;
            last_vol = global_vol;
            uart_putc_raw(MIDI_UART, chan_mode);
            uart_putc_raw(MIDI_UART, 0x07); //  Channel volume
            uart_putc_raw(MIDI_UART, chan_vol);
        }

//        uart_putc_raw(MIDI_UART, chan_mode);
//        uart_putc_raw(MIDI_UART, 0x07); //  Channel volume
//        chan_vol = chan_vol + vol_dir;
//        uart_putc_raw(MIDI_UART, chan_vol);
//        if (chan_vol < 25)
//        {
//            vol_dir = 1;
//        }
//        else if (chan_vol > 70)
//        {
//            vol_dir = (-1);
//        }

        // Select the next voice
        if (last_voice != global_voice)
        {
            last_voice = global_voice;
            chan_voice = last_voice;
            uart_putc_raw(MIDI_UART, 0xC0); // Program change for channel 0
            uart_putc_raw(MIDI_UART, chan_voice);
        }

        // Select the next note
        note_pitch++;
        if (note_pitch > 72)
        {
            note_pitch = 60;
            // Select the next voice
//            chan_voice = (chan_voice + 1) & 0x7F;
//            uart_putc_raw(MIDI_UART, 0xC0); // Program change for channel 0
//            uart_putc_raw(MIDI_UART, chan_voice);
        }

        led_set = (led_set + 1) & 1;

        {
            // Print test
            char message [32];
            int chars = snprintf (message, 31, "\rVoice: %3d Vol: %2d ", (chan_voice + 1), chan_vol);
            int idx;
            for (idx = 0; idx < chars; ++idx)
            {
                uart_putc_raw(uart0, message[idx]);
            }
        }
    }

    return 0;
} // main

/* End of File */
