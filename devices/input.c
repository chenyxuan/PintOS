#include "devices/input.h"
#include <debug.h>
#include "devices/intq.h"
#include "devices/serial.h"

/* Stores keys from the keyboard and serial port. */
static struct intq buffer;

/* Read a line from the input buffer */
void
readline(char *str, int max_length) {
  int len = 0;
  while (true) {
    char ch = input_getc();
    bool end = false;

    switch(ch) {
      case 0x03:
      case 0x04:
      case 0x0D:
        end = true;
        break;

      case 0x09:
      case 0x20:
        putchar(ch);
        str[len++] = ch;
        break;

      case 0x08:
        if(len > 0) {
          putbuf("\b \b", 3);
          str[--len] = '\0';
          break;
        }
      default:
        if(ch >= 0x21 && ch <= 0x7E) {
          putchar(ch);
          str[len++] = ch;
        }
    }
    if(end || len == max_length) {
      putchar('\n');
      break;
    }
  }
  if(len < max_length) str[len] = '\0';
}

/* Initializes the input buffer. */
void
input_init (void) 
{
  intq_init (&buffer);
}

/* Adds a key to the input buffer.
   Interrupts must be off and the buffer must not be full. */
void
input_putc (uint8_t key) 
{
  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (!intq_full (&buffer));

  intq_putc (&buffer, key);
  serial_notify ();
}

/* Retrieves a key from the input buffer.
   If the buffer is empty, waits for a key to be pressed. */
uint8_t
input_getc (void) 
{
  enum intr_level old_level;
  uint8_t key;

  old_level = intr_disable ();
  key = intq_getc (&buffer);
  serial_notify ();
  intr_set_level (old_level);
  
  return key;
}

/* Returns true if the input buffer is full,
   false otherwise.
   Interrupts must be off. */
bool
input_full (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);
  return intq_full (&buffer);
}
