/*
** SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/*
** multicart.h
**
** "lnx multicart": emit a lynxdir .mak that lays out a multicart -- a menu
** executable plus several bundled game ROMs, all built as BLL objects
** (cfg/lynx-bll.cfg). The .mak selects the CC65/Karri mini-loader (NEWMINI_FB68)
** that runs the menu, records the block size and directory offsets, and lists
** the menu (EPYX entry) and the games (BLL entries). Feed the result to lynxdir
** to produce the bootable .lnx. This writes a build recipe only; it does not
** invoke lynxdir. See design/LYNX_MULTICART_DESIGN.md and doc/multicart.html.
*/

#ifndef LNX_MULTICART_H
#define LNX_MULTICART_H

/* Defaults for the multicart layout. These match the runtime loader
** (runtime/lynx/multicartldr.s) and the proven LynxJam layout. */
#define MULTICART_BLOCKSIZE 2048   /* 512 KiB cart, no bank switching */
#define MULTICART_DIRSTART  203    /* 0xCB: first (menu) directory entry */
#define MULTICART_DIROFFSET 896    /* 0x380: game directory offset */

/*
** Parse the "multicart" command's arguments from argv[start..] and write the
** .mak. Returns 0 on success, non-zero on error (a message is printed). See the
** usage text in main.c.
*/
int MulticartMak(int argc, char** argv, int start);

#endif /* LNX_MULTICART_H */
