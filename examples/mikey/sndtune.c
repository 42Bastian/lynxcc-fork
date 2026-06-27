/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** snd engine demo: play a compiled ABC tune, plus a direct-register beep.
**
** The looping melody in theme_music[] is the compiled event stream produced
** offline by the abccc tune compiler from theme.abc:
**
**     T36 V100 R8 H12 K4
**     |: CDEF GABc :|
**     z4
**
** Rebuild the stream with (abccc lives under host-tools/):
**
**     abccc -f s -l theme_music -o theme.s theme.abc
**
** theme.s is committed beside this file so the example links without the host
** tool present; see doc/sound.html and design/LYNX_SND_ENGINE_DESIGN.md.
**
** On boot the program draws a title, fires a one-shot beep on channel 3 with
** the mikey_snd_* direct helpers, then starts the tune on channel 0. Playback
** advances on its own under the sound-timer interrupt while main() idles.
**
** Build:  cl65 -Ors -o sndtune.lnx sndtune.c theme.s
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>

/* abccc output (theme.s) */
extern const unsigned char theme_music[];

static void beep(void)
{
    /* A short rising blip on channel 3 using only the direct helpers -- no
    ** stream involved.  taps() picks a feedback that makes the poly counter
    ** oscillate (a near-square tone); octave() both selects the clock band and
    ** enables the channel timer.  Without taps the channel only emits a DC
    ** level, i.e. no audible sound. */
    unsigned char p;
    mikey_snd_taps(3, 1);
    mikey_snd_octave(3, 4);
    mikey_snd_volume(3, 70);
    for (p = 60; p < 130; ++p) {
        unsigned int d;
        mikey_snd_pitch(3, p);
        for (d = 0; d < 1200; ++d) { }   /* crude on-CPU delay */
    }
    mikey_snd_volume(3, 0);
}

void main(void)
{
    gfx_init();
    gfx_setbgcolor(0);
    gfx_setcolor(2);
    gfx_clear();
    gfx_setcolor(11);
    gfx_outtextxy(28, 40, "lynxcc sound");
    gfx_setcolor(7);
    gfx_outtextxy(20, 56, "abccc tune + sfx");
    gfx_updatedisplay();

    snd_init();
    beep();                     /* direct-register SFX (channel 3) */
    snd_play(0, theme_music);   /* compiled tune (channel 0) */

    for (;;) { }                /* the IRQ advances the stream */
}
