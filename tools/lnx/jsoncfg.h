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
** jsoncfg.h
**
** Minimal JSON reader for the lnx per-game header config. Parses a single flat
** object of string/number values (the strict subset the .lnx header needs) and
** applies it to an LnxHeader. See design/LYNX_LNX_TOOL_DESIGN.md §4.
*/

#ifndef LNX_JSONCFG_H
#define LNX_JSONCFG_H

#include "lnxhdr.h"

/*
** Read JSON config file `path` and overlay its keys onto `h`. Only keys present
** in the file are changed (so it layers on top of defaults or an existing
** header). Returns 0 on success; on error prints a message to stderr and
** returns non-zero. Wrong value types, malformed JSON, and an unreadable file
** are all errors.
**
** When `allow_unknown` is zero an unrecognised key is an error (catching typos).
** When it is non-zero, keys the tool does not recognise are accepted and
** ignored, so a config may carry extra attributes the tool has no field for
** (their values must still be a scalar of the supported subset -- string,
** integer, or true/false/null; nested objects and arrays remain unsupported).
*/
int LnxApplyJsonConfig(const char* path, LnxHeader* h, int allow_unknown);

#endif /* LNX_JSONCFG_H */
