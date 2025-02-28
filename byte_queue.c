/* A "class" for a basic circular queue holding a fixed byte array.
 * To be used for "printf" and "MIDI message" passing. */

#include <stdint.h>
#include "byte_queue.h"

// Put a single byte into queue
void kc_put (Byte_Q *px, char c_add)
{
    uint32_t next = (px->kc_in + 1) & KC_MSK;
    if (next == px->kc_out)
    {
        // queue full, skip this character
        return;
    }
    px->kc_buf [px->kc_in] = c_add;
    px->kc_in = next;
} // kc_put

// Put a byte string into queue
void kc_put_str (Byte_Q *px, char *pc_add, uint32_t len)
{
    uint32_t idx;
    uint32_t free = kc_free(px);
    if (len >= free)
    {
        // too long to fit - skip
        return;
    }

    uint32_t next = px->kc_in & KC_MSK;
    for (idx = 0; idx < len; ++idx)
    {
        px->kc_buf [next] = pc_add[idx];
        next = (next + 1) & KC_MSK;
    }
    px->kc_in = next;
} // kc_put

// Read a single byte from queue
char kc_get (Byte_Q *px)
{
    if (px->kc_in == px->kc_out) // queue is empty
    {
        return 0;
    }
    char uv = px->kc_buf [px->kc_out];
    px->kc_out = (px->kc_out + 1) & KC_MSK;
    return uv;
} // kc_get

// Query queue fill level
uint32_t kc_used (Byte_Q *px)
{
    uint32_t used = (px->kc_in - px->kc_out) & KC_MSK;
    return used;
} // kc_used

// Query space in queue
uint32_t kc_free (Byte_Q *px)
{
    uint32_t used = kc_used(px);
    uint32_t free = KC_MSK - used;
    return free;
} // kc_free

/* End of File */
