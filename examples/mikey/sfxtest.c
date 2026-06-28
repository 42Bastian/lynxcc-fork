/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** snd engine SFX demo: play one of six Space-Invaders-style sound effects on
** each button press.  The effects live in sfx.s as compiled event streams (a
** hand-translation of an old rsound .mac source -- see the comments there).
**
** Each press hands a stream to snd_play().  We use channel 0 for the one-shot
** effects and channel 1 for the long UFO drone, so the drone can keep playing
** while you trigger other effects.  snd_play picks a free channel if the one
** you ask for is busy, so rapid presses still sound.
**
** Button map:
**   A          player shot          B          alien explosion
**   Up         alien march step     Down       shot-missed explosion
**   Option 1   ship explosion       Option 2   UFO drone
**
** Build:  cl65 -Ors -o sfxtest.lnx sfxtest.c sfx.s
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>

/* event streams defined in sfx.s */
extern const unsigned char sfx_shot[];
extern const unsigned char sfx_alien_expl[];
extern const unsigned char sfx_alien_move[];
extern const unsigned char sfx_nothit_expl[];
extern const unsigned char sfx_ship_expl[];
extern const unsigned char sfx_ufo[];

static void draw_ui(void)
{
    gfx_init();
    gfx_setbgcolor(0);
    gfx_setcolor(2);
    gfx_clear();

    gfx_setfont(GFX_FONT_COMPACT);      /* 5x5 / 6px pitch: fits the 160x102 screen */

    gfx_setcolor(11);
    gfx_outtextxy(8, 6, "lynxcc sound fx");
    gfx_setcolor(7);
    gfx_outtextxy(8, 26, "A    : shot");
    gfx_outtextxy(8, 38, "B    : alien boom");
    gfx_outtextxy(8, 50, "Up   : alien step");
    gfx_outtextxy(8, 62, "Down : miss boom");
    gfx_outtextxy(8, 74, "Opt1 : ship boom");
    gfx_outtextxy(8, 86, "Opt2 : ufo drone");
    gfx_updatedisplay();
}

void main(void)
{
    unsigned now;
    unsigned prev = 0;
    unsigned pressed;

    draw_ui();
    snd_init();

    for (;;) {
        now = joy_read();
        pressed = now & ~prev;      /* rising edges only */
        prev = now;

        if (pressed & JOY_BTN_A_MASK)   snd_play(0, sfx_shot);
        if (pressed & JOY_BTN_B_MASK)   snd_play(0, sfx_alien_expl);
        if (pressed & JOY_UP_MASK)      snd_play(0, sfx_alien_move);
        if (pressed & JOY_DOWN_MASK)    snd_play(0, sfx_nothit_expl);
        if (pressed & JOY_OPT1_MASK)    snd_play(0, sfx_ship_expl);
        if (pressed & JOY_OPT2_MASK)    snd_play(1, sfx_ufo);
    }
}
