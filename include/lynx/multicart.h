/*****************************************************************************/
/*                                                                           */
/*                                multicart.h                                 */
/*                                                                           */
/*               Launch bundled game ROMs from a multicart menu              */
/*                                                                           */
/*                                                                           */
/* A multicart is a single .lnx cartridge image that bundles a menu program  */
/* together with several independent game ROMs. At power-on the SDK          */
/* bootloader runs the menu; when the player picks a game, the menu calls     */
/* multicart_run() to load that game off the cartridge over the top of        */
/* itself and run it. Build the pieces as BLL objects (cfg/lynx-bll.cfg) and  */
/* stitch them into a cart with "lnx multicart" + lynxdir. See                */
/* design/LYNX_MULTICART_DESIGN.md and doc/multicart.html.                    */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty. Use and redistribution are free for any purpose; see the        */
/* SPDX headers in the library sources.                                      */
/*                                                                           */
/*****************************************************************************/



#ifndef _MULTICART_H
#define _MULTICART_H



/* Byte offset of the game directory within cart block 0, and the fixed low
** address the runtime loader is copied to and executed at. These are baked into
** the runtime loader (runtime/lynx/multicartldr.s) and the .mak that
** "lnx multicart" writes; they are exposed here for reference/tooling only.
*/
#define MULTICART_DIROFFSET     0x0380  /* game directory offset (896)        */
#define MULTICART_LOADER_ADDR   0x0040  /* where the runtime loader runs      */



void __fastcall__ multicart_run (unsigned char romNum);
/* Launch bundled game ROM number romNum (0 = the first game in the directory
** the ROM builder laid down). Masks interrupts, stops the display timers,
** blanks the palette, copies the relocatable runtime loader to $0040 and jumps
** to it; the loader reads the selected game off the cartridge and runs it.
** This overwrites the menu, so multicart_run() never returns. Call it only from
** a program built and bundled as a multicart menu (see the multicart example).
*/



#endif
