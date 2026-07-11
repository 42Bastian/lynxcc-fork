/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** poweroff.c - Soft power-off with poweroff().
**
** The Lynx voltage regulator has a "soft" on/off input driven by SYSCTL1
** (0xFD87) bit 1: 1 = power on, 0 = power off. The bit powers up as 1 at
** reset, so clearing it disconnects the system from the battery - the same
** mechanism the hardware uses to auto-power-off an idle unit. poweroff()
** (from <lynx/lynx.h>) masks interrupts and clears that bit; on real hardware
** the call never returns because the machine switches off.
**
** This ROM draws a short prompt and waits for the A button. A small |/-\
** spinner turns in the corner as a running-activity indicator: for as long as
** it animates, power is on (SYSCTL1 bit 1 == 1). Pressing A calls poweroff():
** a real Lynx powers down - the spinner stops because the CPU stops; an
** emulator (which usually does not model the regulator soft-off) simply
** freezes on the "powering off..." frame instead.
**
** A real game would save state to the EEPROM (see the eeprom_* family) before
** powering off; this demo keeps it to the one call so the mechanism is clear.
**
** Build:  cl65 -Ors -o poweroff.lnx poweroff.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

/* Previous joypad state, for one-shot A-press edge detection. */
static unsigned prev;

/* The four spinner glyphs, cycled to show the program is still running (i.e.
** power is on). SYSCTL1 bit 1 stays 1 for as long as this animates. */
static const char spinner[4] = { '|', '/', '-', '\\' };

static void draw_prompt (unsigned char arming, unsigned char phase)
{
    char blip[2];

    while (gfx_busy ()) {}

    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();

    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (2, 2, "SOFT POWER-OFF");

    gfx_setcolor (COLOR_LIGHTBLUE);
    gfx_outtextxy (2, 18, "poweroff() clears");
    gfx_outtextxy (2, 26, "SYSCTL1 power bit.");

    if (arming) {
        gfx_setcolor (COLOR_RED);
        gfx_outtextxy (2, 52, "POWERING OFF...");
        gfx_setcolor (COLOR_GREY);
        gfx_outtextxy (2, 68, "HW turns off; an");
        gfx_outtextxy (2, 76, "emulator freezes.");
    } else {
        gfx_setcolor (COLOR_GREEN);
        gfx_outtextxy (2, 52, "PRESS A TO POWER OFF");

        /* Live spinner: while power is on the loop keeps turning it. */
        blip[0] = spinner[phase & 3];
        blip[1] = 0;
        gfx_setcolor (COLOR_YELLOW);
        gfx_outtextxy (2, 88, "POWER ON");
        gfx_outtextxy (66, 88, blip);
    }

    gfx_updatedisplay ();
}

void main (void)
{
    unsigned now;
    unsigned char tick = 0;         /* frame counter driving the spinner */

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setpalette (gfx_getdefpalette ());
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);
    gfx_setfont (GFX_FONT_COMPACT);

    prev = joy_read ();

    for (;;) {
        now = joy_read ();

        /* Rising edge on the A button: arm, show the notice, then power off. */
        if (JOY_BTN_A (now) && !JOY_BTN_A (prev)) {
            draw_prompt (1, 0);     /* last frame the hardware ever shows */
            poweroff ();            /* does not return on real hardware   */
        }
        prev = now;

        /* Advance the spinner every 8th frame (~7.5 Hz at 60 fps). */
        draw_prompt (0, (unsigned char)(tick >> 3));
        ++tick;
    }
}
