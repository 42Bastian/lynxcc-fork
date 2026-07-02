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
** bllrom.c
**
** BLL/BS93 object -> bootable raw Lynx cartridge ROM. See bllrom.h and
** design/LYNX_LNX_BLL_ROM_DESIGN.md.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bllrom.h"
#include "blloader_gen.h"

int BllIsObject(const unsigned char* buf, size_t size)
{
    return size >= BLL_HDR_LEN &&
           memcmp(buf + BLL_MAGIC_OFFSET, BLL_MAGIC, 4) == 0;
}

int BllParseObject(const unsigned char* buf, size_t size,
                   unsigned* load_addr, size_t* body_len)
{
    unsigned block_len;

    if (!BllIsObject(buf, size)) {
        fprintf(stderr, "lnx: not a BLL/BS93 object "
                        "(missing \"BS93\" magic at offset 6)\n");
        return -1;
    }

    /* Load address and block length are stored big-endian in the object. */
    *load_addr = ((unsigned)buf[2] << 8) | buf[3];
    block_len  = ((unsigned)buf[4] << 8) | buf[5];

    if (block_len < BLL_HDR_LEN) {
        fprintf(stderr, "lnx: bad BS93 length field (%u < %d)\n",
                block_len, BLL_HDR_LEN);
        return -1;
    }
    *body_len = (size_t)block_len - BLL_HDR_LEN;

    if (size < BLL_HDR_LEN + *body_len) {
        fprintf(stderr, "lnx: truncated BS93 object: header says %lu body bytes "
                        "but only %lu present\n",
                (unsigned long)*body_len, (unsigned long)(size - BLL_HDR_LEN));
        return -1;
    }
    return 0;
}

BllSize BllChooseSize(size_t body_len, BllSize forced)
{
    if (body_len > BLL_MAX_BODY) {
        fprintf(stderr, "lnx: object body (%lu bytes) is too large for a "
                        "512 KiB cart (max %d)\n",
                (unsigned long)body_len, BLL_MAX_BODY);
        return BLL_SIZE_AUTO;
    }

    if (forced != BLL_SIZE_AUTO) {
        if (body_len > (size_t)forced - BLL_BODY_OFFSET) {
            fprintf(stderr, "lnx: object body (%lu bytes) does not fit a "
                            "%d KiB cart\n",
                    (unsigned long)body_len, (int)forced / 1024);
            return BLL_SIZE_AUTO;
        }
        return forced;
    }

    if (body_len <= BLL_128K - BLL_BODY_OFFSET) { return BLL_128K; }
    if (body_len <= BLL_256K - BLL_BODY_OFFSET) { return BLL_256K; }
    return BLL_512K;
}

/* Select the generated loader region matching a cart size's block size. */
static const unsigned char* LoaderFor(BllSize size)
{
    switch (size) {
        case BLL_128K: return bll_loader_512;   /* block size 512  */
        case BLL_256K: return bll_loader_1024;  /* block size 1024 */
        case BLL_512K: return bll_loader_2048;  /* block size 2048 */
        default:       return NULL;
    }
}

unsigned char* BllBuildRom(const unsigned char* body, size_t body_len,
                           unsigned load_addr, BllSize size)
{
    const unsigned char* loader = LoaderFor(size);
    unsigned char*       rom;
    unsigned char*       dir;

    if (!loader) {
        return NULL;
    }
    rom = (unsigned char*)calloc((size_t)size, 1);
    if (!rom) {
        return NULL;
    }

    /* 1. Loader region (offsets 0x00..0xCA). */
    memcpy(rom, loader, BLL_LOADER_LEN);

    /* 2. One directory entry at 0xCB (matches runtime/lynx/defdir.s). */
    dir = rom + BLL_DIR_OFFSET;
    dir[0] = 0x00;                             /* block: body lives in block 0   */
    dir[1] = (unsigned char)(BLL_BODY_OFFSET & 0xFF);        /* offset LE        */
    dir[2] = (unsigned char)(BLL_BODY_OFFSET >> 8);
    dir[3] = 0x88;                             /* flags: executable              */
    dir[4] = (unsigned char)(load_addr & 0xFF);             /* load address LE  */
    dir[5] = (unsigned char)(load_addr >> 8);
    dir[6] = (unsigned char)(body_len & 0xFF);              /* length LE        */
    dir[7] = (unsigned char)((body_len >> 8) & 0xFF);

    /* 3. Body at 0xD3, then the calloc'd zero padding to `size`. */
    memcpy(rom + BLL_BODY_OFFSET, body, body_len);

    return rom;
}
