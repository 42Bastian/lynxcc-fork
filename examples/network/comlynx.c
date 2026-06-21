/*****************************************************************************/
/*                                                                           */
/* ComLynx loopback self-test (design/LYNX_JOY_SER_DESIGN.md section 8.2)           */
/*                                                                           */
/* The ComLynx wire is open collector with Tx and Rx tied together, so       */
/* every transmitted byte is also received by the sender. That makes a      */
/* full serial test possible without a cable: send a known pattern, expect  */
/* every byte back, then close the port and confirm reception stops.        */
/*                                                                           */
/* Also demonstrates the static joy API: all nine inputs from one          */
/* joy_read() call, with the one-line edge detection idiom.                 */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <lynx/lynx.h>
#include <lynx/tgi.h>
#include <6502.h>
#include <lynx/serial.h>
#include <lynx/joystick.h>

static char buf[32];

static const struct ser_params params = {
    SER_BAUD_62500,             /* Lynx-native speed */
    SER_BITS_8,
    SER_STOP_1,
    SER_PAR_MARK,               /* One of the two sane Lynx formats */
    SER_HS_NONE
};

/* Wait for a received byte with a crude timeout. Returns 1 on data. */
static unsigned char get_with_timeout (char* b)
{
    unsigned timeout;
    for (timeout = 0; timeout < 10000U; ++timeout) {
        if (ser_get (b) == SER_ERR_OK) {
            return 1;
        }
    }
    return 0;
}

void main (void)
{
    unsigned i;
    unsigned ok, bad, lost;
    unsigned char res, status;
    unsigned now, prev = 0, pressed;
    char in;

    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());

    for (;;) {

        tgi_setcolor (COLOR_BLACK);     /* tgi_clear fills in draw color */
        tgi_clear ();
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 2, "COMLYNX LOOPBACK");
        tgi_updatedisplay ();
        while (tgi_busy ()) {}

        ok = bad = lost = 0;

        res = ser_open (&params);
        if (res != SER_ERR_OK) {
            sprintf (buf, "OPEN FAILED: %u", res);
            tgi_setcolor (COLOR_RED);
            tgi_outtextxy (4, 16, buf);
        } else {
            /* Send 0..255; the open collector bus echoes each byte back. */
            for (i = 0; i < 256U; ++i) {
                if (ser_put ((char) i) != SER_ERR_OK) {
                    ++bad;
                    continue;
                }
                if (!get_with_timeout (&in)) {
                    ++lost;
                } else if ((unsigned char) in != (unsigned char) i) {
                    ++bad;
                } else {
                    ++ok;
                }
            }

            ser_status (&status);
            ser_close ();

            /* After close no further bytes may arrive. */
            in = 0;
            res = get_with_timeout (&in);

            tgi_setcolor (COLOR_GREEN);
            sprintf (buf, "OK   %3u", ok);
            tgi_outtextxy (4, 16, buf);
            tgi_setcolor ((ok == 256U) ? COLOR_GREEN : COLOR_RED);
            sprintf (buf, "BAD  %3u  LOST %3u", bad, lost);
            tgi_outtextxy (4, 26, buf);
            tgi_setcolor (COLOR_WHITE);
            sprintf (buf, "STATUS %02X", status);
            tgi_outtextxy (4, 36, buf);
            sprintf (buf, "CLOSED: %s", res ? "RX!? FAIL" : "QUIET OK");
            tgi_outtextxy (4, 46, buf);
        }

        tgi_setcolor (COLOR_YELLOW);
        tgi_outtextxy (4, 70, "A=AGAIN OPT1+PAUSE=?");
        tgi_updatedisplay ();
        while (tgi_busy ()) {}

        /* The §2.2 edge-detect idiom on the unified joy_read. */
        for (;;) {
            now = joy_read (JOY_1);
            pressed = now & ~prev;
            prev = now;
            if (pressed & JOY_BTN_A_MASK) {
                break;                          /* run again */
            }
            if (JOY_PAUSE (now) && JOY_OPT1 (now)) {
                /* Restart convention is the game's job now: demo it. */
                tgi_setcolor (COLOR_RED);
                tgi_outtextxy (4, 84, "RESTART CHORD!");
                tgi_updatedisplay ();
                while (tgi_busy ()) {}
            }
        }
    }
}
