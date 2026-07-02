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
** blloader_gen.h
**
** Declarations for the generated Lynx secondary-loader regions used by the
** `lnx bll` command (see bllrom.c). The arrays themselves live in the GENERATED
** file blloader_gen.c; regenerate both with tools/lnx/gen-loader.sh
** (top-level: make lnx-loader-gen). See design/LYNX_LNX_BLL_ROM_DESIGN.md.
*/

#ifndef LNX_BLLOADER_GEN_H
#define LNX_BLLOADER_GEN_H

/* Length of the loader region: 0xCB == __STARTOFDIRECTORY__ in cfg/lynx.cfg. */
#define BLL_LOADER_LEN 203

/*
** The 203-byte loader region for each supported cart block size (bytes per
** block = cart size / 256): 512 -> 128 KiB, 1024 -> 256 KiB, 2048 -> 512 KiB.
** The variants differ only in the block-size immediate at offset 0xC4.
*/
extern const unsigned char bll_loader_512[BLL_LOADER_LEN];
extern const unsigned char bll_loader_1024[BLL_LOADER_LEN];
extern const unsigned char bll_loader_2048[BLL_LOADER_LEN];

#endif /* LNX_BLLOADER_GEN_H */
