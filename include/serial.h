/*****************************************************************************/
/*                                                                           */
/*                                 serial.h                                  */
/*                                                                           */
/*                  ComLynx serial communication (static)                    */
/*                                                                           */
/*                                                                           */
/* Static, driver-less implementation for the Lynx-only cc65 tree           */
/* (LYNX_JOY_SER_DESIGN.md). Interrupt driven with 256-byte ring buffers    */
/* in each direction.                                                        */
/*                                                                           */
/* ComLynx facts worth knowing:                                              */
/*  - The wire is open collector: every byte you transmit is also received  */
/*    by yourself (handy as a loopback self-test).                          */
/*  - The Lynx includes the parity bit itself in its EVEN/ODD parity        */
/*    calculation, which makes those modes incompatible with the rest of    */
/*    the world. The two formats actually used are parity MARK and SPACE.   */
/*  - SER_PAR_NONE is rejected: the hardware always sends a ninth bit.      */
/*  - Only 8 data bits, 1 stop bit, no handshake.                           */
/*  - A received break drops all buffered data.                             */
/*                                                                           */
/* Behavior notes (changes from the old driver API):                        */
/*  - SER_ERR_* values are renumbered. Compare against the macros, never    */
/*    against bare integers.                                                 */
/*  - ser_close() now really closes: serial interrupts are disabled, the    */
/*    baud timer (timer 4) is stopped, and buffered data is dropped.        */
/*                                                                           */
/*****************************************************************************/



#ifndef _SERIAL_H
#define _SERIAL_H



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* Baud rates implemented by the ComLynx hardware. 62500 and 31250 are the
** Lynx-native speeds frequently used by games.
*/
#define SER_BAUD_75             0x02
#define SER_BAUD_110            0x03
#define SER_BAUD_134_5          0x04
#define SER_BAUD_150            0x05
#define SER_BAUD_300            0x06
#define SER_BAUD_600            0x07
#define SER_BAUD_1200           0x08
#define SER_BAUD_1800           0x09
#define SER_BAUD_2400           0x0A
#define SER_BAUD_3600           0x0B
#define SER_BAUD_4800           0x0C
#define SER_BAUD_7200           0x0D
#define SER_BAUD_9600           0x0E
#define SER_BAUD_31250          0x14
#define SER_BAUD_62500          0x15

/* Data bit settings (only 8 is accepted) */
#define SER_BITS_8              0x03

/* Stop bit settings (only 1 is accepted) */
#define SER_STOP_1              0x00

/* Parity settings. NONE is rejected by ser_open (see header comment). */
#define SER_PAR_NONE            0x00
#define SER_PAR_ODD             0x01
#define SER_PAR_EVEN            0x02
#define SER_PAR_MARK            0x03
#define SER_PAR_SPACE           0x04

/* Handshake settings (only NONE is accepted) */
#define SER_HS_NONE             0x00

/* Bits in the byte returned by ser_status(): the SERCTL error bits
** accumulated by the interrupt handler since the last open, plus bit 7
** for a software receive-buffer overflow.
*/
#define SER_STATUS_BREAK        0x02    /* Break received */
#define SER_STATUS_FE           0x04    /* Framing error */
#define SER_STATUS_OE           0x08    /* Hardware overrun error */
#define SER_STATUS_PE           0x10    /* Parity error */
#define SER_STATUS_RXOVERFLOW   0x80    /* Receive ring buffer overflowed */

/* Error codes. Renumbered for the static library: compare against the
** macros, not bare integers.
*/
#define SER_ERR_OK              0x00    /* Not an error - relax */
#define SER_ERR_BAUD_UNAVAIL    0x01    /* Baud rate not available */
#define SER_ERR_NO_DATA         0x02    /* Nothing to read */
#define SER_ERR_OVERFLOW        0x03    /* No room in send buffer */
#define SER_ERR_INIT_FAILED     0x04    /* Initialization failed */

/* Struct containing parameters for the serial port */
struct ser_params {
    unsigned char       baudrate;       /* Baudrate */
    unsigned char       databits;       /* Number of data bits */
    unsigned char       stopbits;       /* Number of stop bits */
    unsigned char       parity;         /* Parity setting */
    unsigned char       handshake;      /* Type of handshake to use */
};



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



unsigned char __fastcall__ ser_open (const struct ser_params* params);
/* Open the port: set the parameters and enable interrupts. */

unsigned char ser_close (void);
/* Close the port: disable serial interrupts, stop the baud timer, and
** drop buffered data.
*/

unsigned char __fastcall__ ser_get (char* b);
/* Get a character from the serial port. If no characters are available, the
** function will return SER_ERR_NO_DATA, so this is not a fatal error.
*/

unsigned char __fastcall__ ser_put (char b);
/* Send a character via the serial port. Transmission is interrupt driven
** through a ring buffer; the function returns SER_ERR_OVERFLOW if there is
** no space left in it.
*/

unsigned char __fastcall__ ser_status (unsigned char* status);
/* Return the accumulated serial status bits (SER_STATUS_*). */



/* End of serial.h */
#endif
