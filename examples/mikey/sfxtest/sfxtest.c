/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** snd engine SFX player: a small "sound test" front-end for the compiled event
** streams in sfx.s (a hand-translation of an old rsound .mac source -- see the
** comments there).
**
** Instead of binding one effect to each button, the effects are listed in a
** scrolling menu.  Move the highlight with the d-pad and trigger the selected
** effect with A; B stops everything.  This scales to any number of effects:
** add a row to sfx_list[] and it appears in the menu automatically.
**
** Controls:
**   Up / Down   move the selection (the list scrolls if it overflows)
**   A           play the selected effect (on channel 0)
**   B           stop all channels
**
** Effects play on channel 0; snd_play replaces whatever was sounding there, so
** picking a new effect cuts the previous one.  The UFO drone loops until you
** press B (or pick something else).  snd_active() drives the "playing" readout.
**
** Build:  cl65 -Ors -o sfxtest.lnx sfxtest.c sfx.s
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

/* Event streams defined in sfx.s. */
extern const unsigned char sfx_shot[];
extern const unsigned char sfx_alien_expl[];
extern const unsigned char sfx_alien_move[];
extern const unsigned char sfx_nothit_expl[];
extern const unsigned char sfx_ship_expl[];
extern const unsigned char sfx_ufo[];

/* One menu row per effect.  Keeping the table here (rather than a switch on the
** button) means new effects show up in the menu just by adding an entry. */
typedef struct {
    const char          *name;
    const unsigned char *stream;
} SfxEntry;

static const SfxEntry sfx_list[] = {
    { "Player shot",      sfx_shot       },
    { "Alien explosion",  sfx_alien_expl },
    { "Alien march step", sfx_alien_move },
    { "Shot missed",      sfx_nothit_expl},
    { "Ship explosion",   sfx_ship_expl  },
    { "UFO drone (loop)", sfx_ufo        },
};
#define SFX_COUNT   (sizeof (sfx_list) / sizeof (sfx_list[0]))

#define SFX_CHANNEL 0           /* effects play here */

#define LIST_X      14          /* name column                       */
#define CURSOR_X    4           /* '>' marker column                 */
#define LIST_Y      22          /* first row                         */
#define ROW_H       11          /* row pitch (compact font is 6px)   */
#define VISIBLE     6           /* rows shown at once before scroll   */

void main (void)
{
    unsigned char joy, prev = 0, pressed;
    unsigned char sel = 0;      /* highlighted effect                 */
    unsigned char top = 0;      /* first visible row (scroll window)  */
    int           cur = -1;     /* last-triggered effect, -1 = none   */
    unsigned char i, last;
    int           y;

    gfx_init ();
    gfx_setdefpalette ();
    CLI ();                     /* let the VBL + sound-timer IRQs run */
    while (gfx_busy ()) {}
    snd_init ();

    for (;;) {
        joy     = (unsigned char)joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        /* --- input ----------------------------------------------------- */
        if (pressed & JOY_UP_MASK)
            sel = (sel == 0) ? (unsigned char)(SFX_COUNT - 1) : (sel - 1);
        if (pressed & JOY_DOWN_MASK)
            sel = (sel + 1 >= SFX_COUNT) ? 0 : (sel + 1);

        if (pressed & JOY_BTN_A_MASK) {
            snd_play (SFX_CHANNEL, sfx_list[sel].stream);
            cur = sel;
        }
        if (pressed & JOY_BTN_B_MASK) {
            snd_stop ();
            cur = -1;
        }

        /* Keep the selection inside the scroll window. */
        if (sel < top)
            top = sel;
        else if (sel >= top + VISIBLE)
            top = (unsigned char)(sel - VISIBLE + 1);

        /* --- draw ------------------------------------------------------ */
        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();
        gfx_setfont (GFX_FONT_COMPACT);

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (CURSOR_X, 4, "lynxcc sfx player");

        last = (unsigned char)(top + VISIBLE);
        if (last > SFX_COUNT)
            last = (unsigned char)SFX_COUNT;

        for (i = top; i < last; ++i) {
            y = LIST_Y + (int)(i - top) * ROW_H;

            if (i == sel) {
                gfx_setcolor (COLOR_GREEN);
                gfx_outtextxy (CURSOR_X, y, ">");
            }
            /* A still-sounding triggered effect is shown in yellow. */
            if ((int)i == cur && snd_active ())
                gfx_setcolor (COLOR_YELLOW);
            else if (i == sel)
                gfx_setcolor (COLOR_GREEN);
            else
                gfx_setcolor (COLOR_GREY);

            gfx_outtextxy (LIST_X, y, sfx_list[i].name);
        }

        /* --- status + hints ------------------------------------------- */
        gfx_setcolor (COLOR_WHITE);
        if (cur >= 0 && snd_active ())
            gfx_outtextxy (CURSOR_X, 86, sfx_list[cur].name);
        else
            gfx_outtextxy (CURSOR_X, 86, "stopped");

        gfx_setcolor (COLOR_GREY);
        gfx_outtextxy (CURSOR_X, 96, "Up/Dn A:play B:stop");

        gfx_updatedisplay ();
    }
}
