/* Test Code: Drive UART1 at 31250 Baud and send MIDI messages */

#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "hardware/uart.h"

#include "byte_queue.h"

static Byte_Q Q_midi;
static Byte_Q Q_debug;

 // leave uart0 free for stdio
#define MIDI_UART uart1  // MIDI on UART1, to leave UART0 free for debug support
#define BAUD_RATE 31250  // 31250 for MIDI
#define UART_1_TX_PIN 8  // NOTE: GP8, which is pin-11. Could also be GP4 (pin-6)
#define UART_1_RX_PIN 9  // NOTE: GP9, which is pin-12. Could also be GP5 (pin-7)

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define ACTIVE_SENSE 0xFE   // Code for Active Sensing message
#define AS_PERIOD_us 270000 // Typical active sense repeat period, 270ms (in us ticks)

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

        if (ch == 'V') // Change channel volume
        {
            global_vol++;
            if (global_vol > 127) global_vol = 127;
        }
        else if (ch == 'v')
        {
            global_vol--;
            if (global_vol < 0) global_vol = 0;
        }

        if (ch == 'P') // Change channel program
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
} // v_uart_Rx_isr

// MIDI RX interrupt handler /////////////////////////////////////
#ifdef WITH_MIDI_RX
void v_midi_Rx_isr()
{
    while (uart_is_readable(MIDI_UART))
    {
        uint8_t ch = uart_getc(MIDI_UART);
        (void)ch;
    }
} // v_midi_Rx_isr
#endif // WITH_MIDI_RX

//#undef SER_DBG_ON
#define SER_DBG_ON
/* The "main" task on the second core.
 * This manages the UART outputs. */
#define THREAD_READY 0xA5A5
void core1_main (void)
{
    static uint32_t last_midi = 0; // When we last sent a MIDI packet
    uint32_t time_now = 0;

    // signal to the primary thread that this worker thread is ready
    multicore_fifo_push_blocking (THREAD_READY);

    while (true) // loop forever
    {
        while ((kc_used (&Q_midi)) && (uart_is_writable(MIDI_UART)))
        {
            char c1 = kc_get (&Q_midi);
            uart_putc_raw(MIDI_UART, c1);
            last_midi = time_now;
        }

        while ((kc_used (&Q_debug)) && (uart_is_writable(uart0)))
        {
            char c1 = kc_get (&Q_debug);
            uart_putc_raw(uart0, c1);
        }

        time_now = time_us_32();
        if ((time_now - last_midi) > AS_PERIOD_us)
        {
            // Send an Active Sensing packet to keep the link alive.
            if (uart_is_writable(MIDI_UART))
            {
                uart_putc_raw(MIDI_UART, ACTIVE_SENSE);
                last_midi = time_now;
//                {
//                    // Print test
//                    char message [32];
//                    int chars = snprintf (message, 31, "\r\n\nActive Sense\n\n\r");
//                    kc_put_str (&Q_debug, message, chars);
//                }
            }
        }
    } // forever loop

} // core1_main

// main - setup and run loop //////////////////////////////
int main (void)
{
    stdio_init_all(); // Enable UARTS - This will set UART0 for debug
#ifdef SER_DBG_ON
    printf ("\n\n\n\n\n-- MIDI test starting --\n");
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

#ifdef WITH_MIDI_RX
    // Set up the MIDI RX interrupt
    // Select correct interrupt for the UART we are using
    int MIDI_IRQ = UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(MIDI_IRQ, v_midi_Rx_isr);
    irq_set_enabled(MIDI_IRQ, true);

    // Now enable the UART to deliver interrupts - RX only
    uart_set_irq_enables(MIDI_UART, true, false);
#endif // WITH_MIDI_RX

    // Set up UART0 for the debug i/o
    irq_set_exclusive_handler(UART0_IRQ, v_uart_Rx_isr);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);

    // Start the keyboard scanner thread on core-1
    multicore_launch_core1 (core1_main);

    // Wait until things start
    sleep_ms (300);

    // Wait for scan_thread() to start up
    uint32_t core1_rdy = multicore_fifo_pop_blocking();
    // cursory check that core-1 started OK
    if (core1_rdy == THREAD_READY)
    {
#ifdef SER_DBG_ON
        printf ("Core-1 started OK\n");
#endif // SER_DBG_ON
    }
    else
    {
#ifdef SER_DBG_ON
        printf ("Core-1 not started correctly\n");
#endif // SER_DBG_ON
    }

    // "Clear" the screen
    for (int idx = 0; idx < 15; ++idx)
    {
        uart_putc_raw(uart0, 10);
    }

    // Send all notes off
    char chan_mode = 0xB0; // Channel Mode
    char all_off = 123;

    kc_put (&Q_midi, chan_mode);
    kc_put (&Q_midi, all_off); //  All Notes Off
    kc_put (&Q_midi, 0);

    // Enable Active Sensing
    kc_put (&Q_midi, ACTIVE_SENSE);

    // Set voice back to default for channel
    kc_put (&Q_midi, 0xC0); // Program change for channel 0
    kc_put (&Q_midi, 0); // Voice 0

    char note_on = 0x90;  // Note On is 0x9x
    char note_pitch = 60; // 60 decimal == middle-C
    char note_vel = 64;   // Middle volume

    char chan_voice = 0;
    char chan_vol = 70;

    int led_set = 1;

    while (1) // forever loop
    {
        // Note on:
        kc_put (&Q_midi, note_on);
        kc_put (&Q_midi, note_pitch);
        kc_put (&Q_midi, note_vel);

        gpio_put(LED_PIN, led_set);

        // sleep for a bit while note sounds
        sleep_ms (250);

        // Note OFF: running status, note-on with velocity 0
        kc_put (&Q_midi, note_pitch);
        kc_put (&Q_midi, 0);

        // Tweak the channel volume
        if (last_vol != global_vol)
        {
            chan_vol = global_vol;
            last_vol = global_vol;
            kc_put (&Q_midi, chan_mode);
            kc_put (&Q_midi, 0x07); //  Channel volume
            kc_put (&Q_midi, chan_vol);
        }

        // Select the next voice
        if (last_voice != global_voice)
        {
            last_voice = global_voice;
            chan_voice = last_voice;
            kc_put (&Q_midi, 0xC0); // Program change for channel 0
            kc_put (&Q_midi, chan_voice);
        }

        // Select the next note
        note_pitch++;
        if (note_pitch > 72)
        {
            note_pitch = 60;
        }

        led_set = (led_set + 1) & 1;

        {
            // Print test
            char message [32];
            int chars = snprintf (message, 31, "\rVoice: %3d Vol: %2d ", (chan_voice + 1), chan_vol);
            kc_put_str (&Q_debug, message, chars);
        }
    } // forever loop

    return 0;
} // main

/* End of File */
