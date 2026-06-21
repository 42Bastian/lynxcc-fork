/*****************************************************************************/
/*                                                                           */
/*                                joystick.h                                 */
/*                                                                           */
/*                    Read the Atari Lynx joypad inputs                      */
/*                                                                           */
/*                                                                           */
/* Static, driver-less implementation for the Lynx-only cc65 tree            */
/* (design/LYNX_JOY_SER_DESIGN.md). There is exactly one joypad, it is       */
/* always present, and joy_read() reports every input the console has in a   */
/* single call: d-pad, A, B, Option 1, Option 2 (low byte) and Pause (bit    */
/* 8).                                                                       */
/*                                                                           */
/* The JOY_*_MASK values and test macros live in <lynx.h>.                  */
/*                                                                           */
/* Behavior notes (changes from the old driver API):                        */
/*  - Option 1/2 are no longer masked out of the low byte, and Pause is     */
/*    reported in bit 8: code that treated the whole return value as a      */
/*    boolean ("any input?") now also triggers on the switches.             */
/*  - The old conio kbhit()/cgetc() keyboard emulation is gone. Edge        */
/*    detection is the caller's one-liner:                                  */
/*        now = joy_read (JOY_1); pressed = now & ~prev; prev = now;        */
/*  - The Lynx conventions Pause+Opt1 = restart and Pause+Opt2 = flip       */
/*    screen are game conventions, not hardware: honor them yourself.       */
/*                                                                           */
/*****************************************************************************/



#ifndef _JOYSTICK_H
#define _JOYSTICK_H



#include <lynx/lynx.h>



/*****************************************************************************/
/*                                Definitions                                */
/*****************************************************************************/



/* Argument for joy_read (ignored, kept for source compatibility) */
#define JOY_1                   0



/*****************************************************************************/
/*                                 Functions                                 */
/*****************************************************************************/



unsigned __fastcall__ joy_read (unsigned char joystick);
/* Read the joypad. Low byte: d-pad, Opt1, Opt2, B, A (raw $FCB0); bit 8:
** Pause. Use the JOY_*_MASK macros from <lynx.h> to test individual inputs.
** The argument is ignored.
*/



/* End of joystick.h */
#endif
