/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in the examples directory.
*/

/*
** HandyMusic engine demo: plays Osman Celimli's HandyMusic demo song and lets
** you trigger its sound effects.  HandyMusic is the opt-in, script-driven BGM +
** SFX engine (<lynx/handymusic.h>, doc/handymusic.html, doc/hmcc.html); it is a
** heavier, mutually-exclusive alternative to the snd engine used by the other
** mikey/ examples, so this demo links against cfg/lynx-handymusic.cfg.
**
** Fixed-region data model (design/LYNX_HANDYMUSIC_DESIGN.md sec. 3.2 / 4.3):
** the music and SFX scripts were compiled by bin/hmcc to the absolute bases
** the cfg reserves above the C stack -- $B000 for the song, $AE00 for the SFX
** blob.  They ride in the cart as ordinary RODATA (hmdemo.s) and are copied to
** those bases once at startup, after which every pointer hmcc baked in resolves.
**
** The library owns its own 60 Hz tick: HandyMusic's per-frame update is on the
** VBL IRQ chain, so once the VBL interrupt is running (gfx_init + CLI) the game
** just calls play / stop.
**
** PCM (design/LYNX_HANDYMUSIC_DESIGN.md sec. 4.4): two short 8 kHz blips
** (pcmsamples.h) are registered as samples 0 and 1, so the demo song's own
** "play sample" commands stream them out channel 0.  Opt1 also fires one
** directly, and the status line shows when a sample is playing.
**
** Controls:
**   A     play the selected sound effect
**   B     stop the music (press again to restart)
**   Opt1  play a PCM blip directly (borrows channel 0 briefly)
**   Up/Dn choose which effect A fires
**
** Build:  make   (see the Makefile; it drives hmcc, then cl65 -C
**                 lynx-handymusic.cfg).
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <lynx/handymusic.h>
#include <6502.h>
#include <string.h>

#include "sfx_equ.h"             /* generated from sfx.equ by the Makefile:    */
                                /* HandyMusic_NumSFX, HandyMusic_SFX_ATable*, */
                                /* and the SFX_* effect indices.              */
#include "pcmsamples.h"          /* pcm_laser / pcm_blip 8 kHz sample arrays.  */

/* Resident hmcc blobs (hmdemo.s) and their reserved run-time bases. */
extern const unsigned char hm_music_data[];
extern const unsigned char hm_music_len;         /* &hm_music_len == length */
extern const unsigned char hm_sfx_data[];
extern const unsigned char hm_sfx_len;

#define HM_MUSIC_BASE   ((unsigned char *)0xB000u) /* __HMMUS_START__ */
#define HM_SFX_BASE     ((unsigned char *)0xAE00u) /* __HMSFX_START__ */

/* A browsable list of the demo's effects (names + hmcc SFX_* indices). */
typedef struct {
    const char   *name;
    unsigned char index;
} SfxEntry;

static const SfxEntry sfx_list[] = {
    { "Rapid shot",   SFX_Zaku_RapidShot  },
    { "Back blaster", SFX_Zaku_BackBlaster },
    { "Charge up",    SFX_Zaku_ChargeUp   },
    { "Charge shot",  SFX_Zaku_ChargeShot },
    { "VWF buzzy",    SFX_VWFBuzzy        },
    { "VWF square",   SFX_VWFSquare       },
};

#define SFX_COUNT   (sizeof (sfx_list) / sizeof (sfx_list[0]))

void main (void)
{
    unsigned char joy, prev = 0, pressed;
    unsigned char sel = 0;
    unsigned char i;
    unsigned char playing = 1;
    int           y;

    gfx_init ();
    gfx_setdefpalette ();
    CLI ();                     /* let the VBL (+ HandyMusic) IRQ run */
    while (gfx_busy ()) {}

    /* Move the compiled blobs into the reserved regions hmcc targeted. */
    memcpy (HM_MUSIC_BASE, hm_music_data, (unsigned)&hm_music_len);
    memcpy (HM_SFX_BASE,   hm_sfx_data,   (unsigned)&hm_sfx_len);

    /* Point the driver at the SFX blob's three tables (base, base+N, base+2N,
    ** all inside the copied $AE00 region -- addresses from sfx.equ). */
    handymusic_set_sfx_tables (HandyMusic_SFX_ATableLo,
                               HandyMusic_SFX_ATableHi,
                               HandyMusic_SFX_PTable);

    handymusic_init ();

    /* Register the two blips so the song's "sample 0" / "sample 1" commands
    ** stream them (design sec. 4.4). */
    handymusic_register_pcm (0, pcm_laser, pcm_laser_len);
    handymusic_register_pcm (1, pcm_blip,  pcm_blip_len);

    handymusic_play_music ();

    for (;;) {
        joy     = (unsigned char)joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_UP_MASK)
            sel = (sel == 0) ? (unsigned char)(SFX_COUNT - 1) : (sel - 1);
        if (pressed & JOY_DOWN_MASK)
            sel = (sel + 1 >= SFX_COUNT) ? 0 : (sel + 1);

        if (pressed & JOY_BTN_A_MASK)
            handymusic_play_sfx (sfx_list[sel].index);

        if (pressed & JOY_BTN_B_MASK) {
            if (playing)
                handymusic_stop_music ();
            else
                handymusic_play_music ();
            playing ^= 1;
        }

        if (pressed & JOY_OPT1_MASK)
            handymusic_play_pcm (pcm_laser, pcm_laser_len);

        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();
        gfx_setfont (GFX_FONT_COMPACT);

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 4, "HandyMusic engine");
        gfx_setcolor (COLOR_GREY);
        gfx_outtextxy (4, 16, playing ? "Music: playing" : "Music: stopped");
        if (handymusic_pcm_playing ()) {
            gfx_setcolor (COLOR_LIGHTBLUE);
            gfx_outtextxy (120, 16, "PCM");
        }

        for (i = 0; i < SFX_COUNT; ++i) {
            y = 34 + (int)i * 11;
            if (i == sel) {
                gfx_setcolor (COLOR_GREEN);
                gfx_outtextxy (4, y, ">");
                gfx_setcolor (COLOR_GREEN);
            } else {
                gfx_setcolor (COLOR_GREY);
            }
            gfx_outtextxy (14, y, sfx_list[i].name);
        }

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 108, "A:sfx B:music O1:pcm");

        gfx_updatedisplay ();
    }
}
