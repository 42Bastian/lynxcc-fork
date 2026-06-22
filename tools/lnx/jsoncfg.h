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
** returns non-zero. Unknown keys, wrong value types, malformed JSON, and an
** unreadable file are all errors.
*/
int LnxApplyJsonConfig(const char* path, LnxHeader* h);

#endif /* LNX_JSONCFG_H */
