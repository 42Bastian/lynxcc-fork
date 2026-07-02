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
** bllrom.h
**
** Convert a BLL/BS93 object (as produced by cfg/lynx-bll.cfg) into a bootable
** raw Lynx cartridge ROM: prepend the SDK secondary loader (blloader_gen), write
** a one-entry directory pointing at the object body, and pad to a real cart size.
** Part of the standalone `lnx` SDK tool (tools/lnx). A post-build format
** conversion, NOT a linker. See design/LYNX_LNX_BLL_ROM_DESIGN.md.
*/

#ifndef LNX_BLLROM_H
#define LNX_BLLROM_H

#include <stddef.h>

#define BLL_MAGIC        "BS93"   /* at object offset 6 */
#define BLL_MAGIC_OFFSET 6
#define BLL_HDR_LEN      10       /* BS93 10-byte object header */
#define BLL_BODY_OFFSET  211      /* 0xD3: loader (203) + directory (8) */
#define BLL_DIR_OFFSET   203      /* 0xCB: __STARTOFDIRECTORY__ */
#define BLL_DIR_LEN      8

/* Supported cart sizes (bytes). The numeric value is the ROM byte count. */
typedef enum {
    BLL_SIZE_AUTO = 0,
    BLL_128K = 131072,
    BLL_256K = 262144,
    BLL_512K = 524288
} BllSize;

/* Largest body that still leaves room for the 211-byte prefix in a 512 KiB cart. */
#define BLL_MAX_BODY (524288 - BLL_BODY_OFFSET)

/*
** True if `buf` (of `size` bytes) looks like a BLL/BS93 object: at least a
** 10-byte header whose bytes 6..9 are "BS93".
*/
int BllIsObject(const unsigned char* buf, size_t size);

/*
** Parse the 10-byte BS93 header. On success returns 0 and stores the little-
** endian load address and the body length (block_len - 10); returns -1 on a bad
** magic, a length field < 10, or a file too short to hold the stated body (a
** clear message is printed to stderr).
*/
int BllParseObject(const unsigned char* buf, size_t size,
                   unsigned* load_addr, size_t* body_len);

/*
** Pick the cart size for a body of `body_len` bytes. With `forced` == AUTO,
** returns the smallest of 128/256/512 KiB that fits; otherwise validates that
** the body fits the forced size. Returns 0 (and prints a message) if it does not
** fit even 512 KiB, or if `forced` is too small.
*/
BllSize BllChooseSize(size_t body_len, BllSize forced);

/*
** Build the raw cart image into a freshly malloc'd buffer of exactly `size`
** bytes: the SDK loader region for `size`, then the 8-byte directory (block 0,
** offset 211, flags 0x88, load_addr, body_len), then the body, then zero pad.
** Returns the buffer (caller frees) or NULL on out of memory.
*/
unsigned char* BllBuildRom(const unsigned char* body, size_t body_len,
                           unsigned load_addr, BllSize size);

#endif /* LNX_BLLROM_H */
