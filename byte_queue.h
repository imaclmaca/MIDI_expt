/* A "class" for a basic circular queue holding a fixed byte array.
 * To be used for "printf" and "MIDI message" passing. */

// circular buffer for key-codes, pending sending...
#define KC_SZ 128
#define KC_MSK (KC_SZ - 1)


/* A structure for the queue "class" */
struct _x_byte_queue_
{
    uint32_t kc_in;
    uint32_t kc_out;
    char kc_buf [KC_SZ];
};

typedef struct _x_byte_queue_ Byte_Q;

extern void kc_put (Byte_Q *px, char c_add);
extern void kc_put_str (Byte_Q *px, char *pc_add, uint32_t len);
extern char kc_get (Byte_Q *px);

extern uint32_t kc_used (Byte_Q *px);
extern uint32_t kc_free (Byte_Q *px);

/* End of File */
