/*****************************************************************************/
/*                                                                           */
/*                               handymusic.h                                */
/*                                                                           */
/*        HandyMusic script-driven BGM + SFX engine for the Atari Lynx       */
/*                                                                           */
/*                                                                           */
/* Part of the Lynx Game Development SDK (lynxcc).                            */
/*                                                                           */
/* Derived work of Osman Celimli's HandyMusic 1.40cx+ sound driver.  Under   */
/* the author's grant, HandyMusic is completely free to use and modify in    */
/* your own projects.  This ca65 port and C surface retain that grant (they  */
/* are NOT under the SDK's MPL-2.0 fork).  See doc/licenses.html sec. 4.5.    */
/*                                                                           */
/*****************************************************************************/



#ifndef _LYNX_HANDYMUSIC_H
#define _LYNX_HANDYMUSIC_H

#if !defined(__LYNX__)
#  error This module may only be used when compiling for the Lynx game console!
#endif

#include <zeropage.h>           /* __zeropage: the SFX table pointers are ZP */

/* HandyMusic is an opt-in, macro-instrument BGM + SFX engine: instruments and
** sound effects are byte-code envelope scripts, music is four parallel track
** scripts, all compiled from SASS-format text by the host tool bin/hmcc (see
** doc/hmcc.html).  It is a heavier, mutually-exclusive alternative to the snd
** engine + sfx catalogue -- a game opts into one or the other, never both.
** Link it in by building against cfg/lynx-handymusic.cfg; the library and its
** lib/lynx-handymusic.lib are pulled in automatically.  See doc/handymusic.html
** and design/LYNX_HANDYMUSIC_DESIGN.md.
**
** Fixed-region data model (design sec. 3.2 / 4.3): hmcc bakes ABSOLUTE pointer
** tables into the .mus / .sfx blobs at a chosen base.  cfg/lynx-handymusic.cfg
** reserves two bases above the C stack -- __HMMUS_START__ ($B000) for the song
** and __HMSFX_START__ ($AE00) for the SFX blob.  The game copies each blob to
** its base at startup, then plays.
**
** The library owns its own 60 Hz tick: HandyMusic's per-frame update is
** installed on the VBL IRQ chain, so the game only calls init / play / stop /
** pause.  The VBL timer must be running -- call gfx_init() (or otherwise enable
** the VBL interrupt) before handymusic_init().
**
** PCM sample playback is a planned phase and is not in this build: the music
** "play sample" command is a no-op (design sec. 4.4).
*/

/* Initialize HandyMusic and the audio hardware (stereo on, all channels idle).
** Call once, after the VBL interrupt is running, before any play call.
*/
void handymusic_init (void);

/* Start playing the song resident in the reserved music region
** (__HMMUS_START__).  Stop the current song before loading another. */
void handymusic_play_music (void);

/* Stop the current song, freeing any channels it was using. */
void handymusic_stop_music (void);

/* Queue sound effect n (a SFX_* index from the hmcc .equ) to start next frame.
** A higher-priority effect wins a busy channel; see doc/hmcc.html. */
void __fastcall__ handymusic_play_sfx (unsigned char n);

/* Stop the first sound (effect OR note) whose priority equals prio. */
void __fastcall__ handymusic_stop_sfx (unsigned char prio);

/* Stop every music track and sound effect. */
void handymusic_stop_all (void);

/* Pause / resume all channels (both are double-call safe). */
void handymusic_pause (void);
void handymusic_unpause (void);

/* Pre-init SFX script-table pointers (zero page).  Before the first
** handymusic_play_sfx, point these at the three tables hmcc emitted for the SFX
** blob -- their absolute addresses are in the generated .equ as
** HandyMusic_SFX_ATableLo / _ATableHi / _PTable.  Use the helper below. */
extern const unsigned char *handymusic_sfx_addr_lo __zeropage; /* low-byte addr table  */
extern const unsigned char *handymusic_sfx_addr_hi __zeropage; /* high-byte addr table */
extern const unsigned char *handymusic_sfx_prio    __zeropage; /* priority table       */

/* Point the driver at a SFX blob's three tables.  For the standard hmcc layout
** the tables are contiguous at base, base+count and base+2*count. */
#define handymusic_set_sfx_tables(lo, hi, pri)                          \
    do {                                                                \
        handymusic_sfx_addr_lo = (const unsigned char *)(lo);           \
        handymusic_sfx_addr_hi = (const unsigned char *)(hi);           \
        handymusic_sfx_prio    = (const unsigned char *)(pri);          \
    } while (0)

/* End of handymusic.h */
#endif
