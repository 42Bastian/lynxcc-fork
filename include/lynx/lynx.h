/*****************************************************************************/
/*                                                                           */
/*                                  lynx.h                                   */
/*                                                                           */
/*                     Lynx system-specific definitions                      */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2003      Shawn Jefferson                                             */
/*                                                                           */
/* Adapted with many changes Ullrich von Bassewitz, 2004-10-09               */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/



#ifndef _LYNX_H
#define _LYNX_H



/* Check for errors */
#if !defined(__LYNX__)
#  error This module may only be used when compiling for the Lynx game console!
#endif



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* Color defines */
#define COLOR_BLACK             0x00
#define COLOR_RED               0x01
#define COLOR_PINK              0x02
#define COLOR_LIGHTGREY         0x03
#define COLOR_GREY              0x04
#define COLOR_DARKGREY          0x05
#define COLOR_BROWN             0x06
#define COLOR_PEACH             0x07
#define COLOR_YELLOW            0x08
#define COLOR_LIGHTGREEN        0x09
#define COLOR_GREEN             0x0A
#define COLOR_DARKBROWN         0x0B
#define COLOR_VIOLET            0x0C
#define COLOR_BLUE              0x0D
#define COLOR_LIGHTBLUE         0x0E
#define COLOR_WHITE             0x0F

/* Lynx graphics color defines (default palette) */
#define GFX_COLOR_BLACK         COLOR_BLACK
#define GFX_COLOR_RED           COLOR_RED
#define GFX_COLOR_PINK          COLOR_PINK
#define GFX_COLOR_LIGHTGREY     COLOR_LIGHTGREY
#define GFX_COLOR_GREY          COLOR_GREY
#define GFX_COLOR_DARKGREY      COLOR_DARKGREY
#define GFX_COLOR_BROWN         COLOR_BROWN
#define GFX_COLOR_PEACH         COLOR_PEACH
#define GFX_COLOR_YELLOW        COLOR_YELLOW
#define GFX_COLOR_LIGHTGREEN    COLOR_LIGHTGREEN
#define GFX_COLOR_GREEN         COLOR_GREEN
#define GFX_COLOR_DARKBROWN     COLOR_DARKBROWN
#define GFX_COLOR_VIOLET        COLOR_VIOLET
#define GFX_COLOR_BLUE          COLOR_BLUE
#define GFX_COLOR_LIGHTBLUE     COLOR_LIGHTBLUE
#define GFX_COLOR_WHITE         COLOR_WHITE

/* DEPRECATED palette aliases: the TGI_COLOR_* names are retained for backward
** compatibility and map to the GFX_COLOR_* names above. Define
** LYNX_NO_TGI_COMPAT before including to drop them (see <lynx/gfx.h>).
*/
#ifndef LYNX_NO_TGI_COMPAT
#define TGI_COLOR_BLACK         GFX_COLOR_BLACK
#define TGI_COLOR_RED           GFX_COLOR_RED
#define TGI_COLOR_PINK          GFX_COLOR_PINK
#define TGI_COLOR_LIGHTGREY     GFX_COLOR_LIGHTGREY
#define TGI_COLOR_GREY          GFX_COLOR_GREY
#define TGI_COLOR_DARKGREY      GFX_COLOR_DARKGREY
#define TGI_COLOR_BROWN         GFX_COLOR_BROWN
#define TGI_COLOR_PEACH         GFX_COLOR_PEACH
#define TGI_COLOR_YELLOW        GFX_COLOR_YELLOW
#define TGI_COLOR_LIGHTGREEN    GFX_COLOR_LIGHTGREEN
#define TGI_COLOR_GREEN         GFX_COLOR_GREEN
#define TGI_COLOR_DARKBROWN     GFX_COLOR_DARKBROWN
#define TGI_COLOR_VIOLET        GFX_COLOR_VIOLET
#define TGI_COLOR_BLUE          GFX_COLOR_BLUE
#define TGI_COLOR_LIGHTBLUE     GFX_COLOR_LIGHTBLUE
#define TGI_COLOR_WHITE         GFX_COLOR_WHITE
#endif /* LYNX_NO_TGI_COMPAT */

/* Masks for joy_read. The low byte is the raw JOYSTICK register; Pause
** (a Mikey-side switch) is reported in bit 8 of the joy_read result.
*/
#define JOY_UP_MASK             0x80
#define JOY_DOWN_MASK           0x40
#define JOY_LEFT_MASK           0x20
#define JOY_RIGHT_MASK          0x10
#define JOY_OPT1_MASK           0x08
#define JOY_OPT2_MASK           0x04
#define JOY_BTN_1_MASK          0x01
#define JOY_BTN_2_MASK          0x02
#define JOY_PAUSE_MASK          0x100

#define JOY_BTN_A_MASK          JOY_BTN_1_MASK
#define JOY_BTN_B_MASK          JOY_BTN_2_MASK

#define JOY_BTN_A(v)            ((v) & JOY_BTN_A_MASK)
#define JOY_BTN_B(v)            ((v) & JOY_BTN_B_MASK)
#define JOY_OPT1(v)             ((v) & JOY_OPT1_MASK)
#define JOY_OPT2(v)             ((v) & JOY_OPT2_MASK)
#define JOY_PAUSE(v)            ((v) & JOY_PAUSE_MASK)



/*****************************************************************************/
/*                           Sound support                                   */
/*****************************************************************************/



/* Compiled-stream music engine.  Streams are produced offline by the abccc
** tune compiler; see design/LYNX_SND_ENGINE_DESIGN.md and doc/sound.html. */

void snd_init (void);
/* Initialize the sound engine: program the sound timer and install the
** sound-interrupt handler.  Call once before any other snd_* function. */

void snd_pause (void);
/* Suspend playback on all channels (e.g. game pause) */

void snd_continue (void);
/* Resume playback after snd_pause */

void __fastcall__ snd_play (unsigned char channel, const unsigned char *stream);
/* Start a compiled tune stream on a channel (0-3).  If that channel is still
** sounding, it is retriggered in place: the old stream is force-stopped (with
** a brief ~4-tick volume fade, not an instant cut -- see snd_stop_channel)
** before the new one starts. */

void snd_stop (void);
/* Stop sound on all channels.  Each channel's volume is faded out over a few
** ticks (~17ms at 240Hz) rather than cut instantly, to avoid an audible
** click; snd_active() reports the channel busy until the fade finishes. */

void __fastcall__ snd_stop_channel (unsigned char channel);
/* Stop sound on the given channel, fading it out the same way as snd_stop */

unsigned char snd_active (void);
/* Non-zero while any channel is still playing (bitmask of active channels) */

/* Direct Mikey channel-register helpers, independent of the stream engine --
** useful for sound effects or hand-built instruments (mikey-snd.s). */

void __fastcall__ mikey_snd_octave (unsigned char chan, unsigned char val);
/* Clock-divider band, CONTROL bits 0-2 (val 0..6); also enables the channel
** timer (a channel makes no sound until its timer runs). Set feedback taps too. */

void __fastcall__ mikey_snd_pitch (unsigned char chan, unsigned char val);
/* Timer reload / pitch within the octave, BACKUP (val 0..255) */

void __fastcall__ mikey_snd_taps (unsigned char chan, unsigned int val);
/* LFSR feedback taps, FEEDBACK + CONTROL bit 7 (val 0..511) */

void __fastcall__ mikey_snd_integrate (unsigned char chan, unsigned char val);
/* Integrate bit, CONTROL bit 5 (val 0 = square, 1 = integrate) */

void __fastcall__ mikey_snd_volume (unsigned char chan, unsigned char val);
/* Signed output volume, VOLUME (val 0..127) */



/*****************************************************************************/
/*                           Accessing the cart                              */
/*****************************************************************************/



/* The cart is ROM and its files are addressed by number, not by name: entry 0
** is the boot executable, entry 1 the next file appended after it, and so on.
** Access is therefore read-only and numbered - there is no POSIX open()/fopen()
** layer and no write()/creat() path. Persistent game data belongs in the EEPROM
** (see the lynx_eeread_93cNN / lynx_eewrite_93cNN family below). Only bank 0
** is read.
*/

void __fastcall__ openn (int fileno);
/* Open the numbered directory entry and position the cart at its first byte so
** that read() and lseek() can stream it. The first entry is fileno=0. This is
** the low-level primitive behind lynx_load()/lynx_exec().
*/

void __fastcall__ lynx_load (int fileno);
/* Load a file into ram. The first entry is fileno=0. */

void __fastcall__ lynx_exec (int fileno);
/* Load a file into ram and execute it. */



/*****************************************************************************/
/*                           Accessing the EEPROM                            */
/*****************************************************************************/



/* Size-selectable read/write family. Pick the variant matching the EEPROM chip
** fitted to the cart: 93C46 = 64 words, 93C66 = 256 words, 93C86 = 1024 words.
*/
unsigned __fastcall__ lynx_eeread_93c46 (unsigned char cell);
/* Read a 16 bit word from a 93C46 (cell 0..63) */

unsigned __fastcall__ lynx_eeread_93c66 (unsigned addr);
/* Read a 16 bit word from a 93C66 (addr 0..255) */

unsigned __fastcall__ lynx_eeread_93c86 (unsigned addr);
/* Read a 16 bit word from a 93C86 (addr 0..1023) */

void __fastcall__ lynx_eewrite_93c46 (unsigned addr, unsigned val);
/* Write the word at the given address of a 93C46 */

void __fastcall__ lynx_eewrite_93c66 (unsigned addr, unsigned val);
/* Write the word at the given address of a 93C66 */

void __fastcall__ lynx_eewrite_93c86 (unsigned addr, unsigned val);
/* Write the word at the given address of a 93C86 */



/*****************************************************************************/
/*                              Lynx graphics                                */
/*****************************************************************************/



/* Pull in the Lynx graphics API so that programs including <lynx.h> get the
** gfx_ names by default (not the deprecated tgi_ shim in <lynx/gfx.h>).
*/
#include <lynx/gfx.h>

/* The former upstream tgi_ioctl macros (now gfx_sprite, gfx_flip,
** gfx_setbgcolor, gfx_setframerate, gfx_busy, gfx_updatedisplay,
** gfx_setcollisiondetection) are real functions declared in <lynx/gfx.h>;
** existing call sites compile unchanged. The ioctl dispatcher itself is gone.
*/

/* Pull in the reusable sound-effect library so that programs including
** <lynx.h> get the sfx_<name>(ch) play macros by default.  It layers over the
** snd engine declared above; see <lynx/sfx.h>, doc/sound.html and
** design/LYNX_SFX_LIBRARY_DESIGN.md.
*/
#include <lynx/sfx.h>

/* Define Hardware */
#include <lynx/_mikey.h>
#define MIKEY (*(struct __mikey *)0xFD00)

#define _MIKEY_TIMERS (*(struct _mikey_all_timers *) 0xFD00)  // mikey_timers[8]
#define _HBL_TIMER (*(struct _mikey_timer *) 0xFD00)          // timer0 (HBL)
#define _VBL_TIMER (*(struct _mikey_timer *) 0xFD08)          // timer2 (VBL)
#define _UART_TIMER (*(struct _mikey_timer *) 0xFD14)         // timer4 (UART)
#define _VIDDMA (*(unsigned int *) 0xFD92)                    // dispctl/viddma

#include <lynx/_suzy.h>
#define SUZY        (*(struct __suzy*)0xFC00)



/* End of lynx.h */
#endif
