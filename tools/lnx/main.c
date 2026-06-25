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
** main.c
**
** lnx -- inspect and patch the 64-byte Atari Lynx (.lnx) cartridge header, and
** wrap a raw image with a fresh header. A standalone SDK utility (tools/lnx);
** it is a post-build header editor, NOT a linker or sprite converter.
** See design/LYNX_LNX_TOOL_DESIGN.md.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lnxhdr.h"
#include "jsoncfg.h"

#define LNX_TOOL_VERSION "1.0"

/* Header field options gathered from the command line. Each *_set flag records
** whether the user supplied the option, so flags overlay onto config values. */
typedef struct {
    const char*   config;          /* --config path, or NULL */

    int           cartname_set;
    const char*   cartname;
    int           manuf_set;
    const char*   manuf;
    int           rotation_set;
    unsigned char rotation;
    int           audin_set;
    unsigned char audin;
    LnxEepromSpec ee;              /* EEPROM flag byte, field by field */
    int           bank0_set;
    unsigned      bank0;
    int           bank1_set;
    unsigned      bank1;
    int           version_set;
    unsigned      version;

    const char*   output;          /* -o path, or NULL */
    const char*   file;            /* positional input file */
} Options;

static void Usage(FILE* f)
{
    fprintf(f,
        "lnx %s -- Atari Lynx .lnx header tool\n"
        "\n"
        "usage: lnx <command> [options] <file>\n"
        "\n"
        "commands:\n"
        "  info    <file.lnx>           print the header fields\n"
        "  dump    <file.lnx>           hex+ASCII dump of the 64-byte header\n"
        "  patch   [field-opts] <file>  rewrite header fields of a .lnx\n"
        "  create  [field-opts] <raw>   wrap a raw image with a fresh header\n"
        "\n"
        "field options (patch/create):\n"
        "  --config <file.json>         read fields from a JSON config\n"
        "  --cartname <str>             set the 32-byte cart name\n"
        "  --manufacturer <str>         set the 16-byte manufacturer name\n"
        "  --rotation none|left|right   set the rotation flag (or 0|1|2)\n"
        "  --audin 0|1                  set the AUDIN-addressing flag\n"
        "  --eeprom <n>                 set the whole EEPROM flag byte (bit field)\n"
        "  --eeprom-chip <name|0-5>     set EEPROM chip: none|93c46|93c56|93c66|\n"
        "                               93c76|93c86 (bits 0-2)\n"
        "  --eeprom-lynxsd 0|1          set LynxSD-save bit (bit 6)\n"
        "  --eeprom-word 8|16           set EEPROM word size (bit 7)\n"
        "  --bank0 <n>                  set bank-0 page size\n"
        "  --bank1 <n>                  set bank-1 page size\n"
        "  --version <n>                set the header version word\n"
        "  -o, --output <file>          write result here (create requires -o;\n"
        "                               patch defaults to in place)\n"
        "\n"
        "  -h, --help                   this help\n"
        "  -V, --tool-version           print tool version\n"
        "\n"
        "config values are overlaid by any explicit --flag of the same field.\n",
        LNX_TOOL_VERSION);
}

/* Read an entire file into a malloc'd buffer. Returns the buffer (caller frees)
** and stores the size; NULL on error (message already printed). */
static unsigned char* ReadFile(const char* path, size_t* size_out)
{
    FILE*          f = fopen(path, "rb");
    long           size;
    unsigned char* buf;

    if (!f) {
        fprintf(stderr, "lnx: cannot open '%s'\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0) {
        fprintf(stderr, "lnx: cannot size '%s'\n", path);
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (unsigned char*)malloc((size_t)size ? (size_t)size : 1);
    if (!buf) {
        fprintf(stderr, "lnx: out of memory\n");
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "lnx: short read on '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = (size_t)size;
    return buf;
}

static int WriteFile(const char* path, const unsigned char* buf, size_t size)
{
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "lnx: cannot write '%s'\n", path);
        return -1;
    }
    if (fwrite(buf, 1, size, f) != size) {
        fprintf(stderr, "lnx: write error on '%s'\n", path);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

/* Apply config then CLI flags onto `h`, in that order (flags win). */
static int ApplyOptions(const Options* o, LnxHeader* h)
{
    if (o->config) {
        if (LnxApplyJsonConfig(o->config, h) != 0) {
            return -1;
        }
    }
    if (o->cartname_set) {
        LnxSetCartname(h, o->cartname);
    }
    if (o->manuf_set) {
        LnxSetManufname(h, o->manuf);
    }
    if (o->rotation_set) {
        h->rotation = o->rotation;
    }
    if (o->audin_set) {
        h->audin = o->audin;
    }
    if (o->ee.raw_set || o->ee.chip_set || o->ee.lynxsd_set || o->ee.word8_set) {
        h->eeprom = LnxEepromCompose(h->eeprom, &o->ee);
    }
    if (o->bank0_set) {
        h->page_size_bank0 = o->bank0;
    }
    if (o->bank1_set) {
        h->page_size_bank1 = o->bank1;
    }
    if (o->version_set) {
        h->version = o->version;
    }
    return 0;
}

/* Parse a non-negative number option; returns 0 on success. */
static int ParseUInt(const char* s, unsigned* out)
{
    char* e = NULL;
    long  v = strtol(s, &e, 0);
    if (e == s || *e != '\0' || v < 0 || v > 0xFFFF) {
        return -1;
    }
    *out = (unsigned)v;
    return 0;
}

/* Parse field options + positional file from argv[start..]. */
static int ParseFieldArgs(int argc, char** argv, int start, Options* o)
{
    int i;
    for (i = start; i < argc; i++) {
        const char* a = argv[i];

        if (strcmp(a, "--config") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --config needs a path\n"); return -1; }
            o->config = argv[i];
        } else if (strcmp(a, "--cartname") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --cartname needs a value\n"); return -1; }
            o->cartname = argv[i]; o->cartname_set = 1;
        } else if (strcmp(a, "--manufacturer") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --manufacturer needs a value\n"); return -1; }
            o->manuf = argv[i]; o->manuf_set = 1;
        } else if (strcmp(a, "--rotation") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --rotation needs a value\n"); return -1; }
            if (LnxParseRotation(argv[i], &o->rotation) != 0) {
                fprintf(stderr, "lnx: bad --rotation '%s' (use none|left|right)\n", argv[i]);
                return -1;
            }
            o->rotation_set = 1;
        } else if (strcmp(a, "--audin") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --audin needs a value\n"); return -1; }
            if (strcmp(argv[i], "0") == 0 || strcmp(argv[i], "no") == 0 || strcmp(argv[i], "off") == 0) {
                o->audin = 0;
            } else if (strcmp(argv[i], "1") == 0 || strcmp(argv[i], "yes") == 0 || strcmp(argv[i], "on") == 0) {
                o->audin = 1;
            } else {
                fprintf(stderr, "lnx: bad --audin '%s' (use 0|1)\n", argv[i]);
                return -1;
            }
            o->audin_set = 1;
        } else if (strcmp(a, "--eeprom") == 0) {
            unsigned v;
            if (++i >= argc || ParseUInt(argv[i], &v) != 0 || v > 0xFF) { fprintf(stderr, "lnx: bad --eeprom (0..255)\n"); return -1; }
            o->ee.raw = (unsigned char)v;
            o->ee.raw_set = 1;
        } else if (strcmp(a, "--eeprom-chip") == 0) {
            if (++i >= argc || LnxParseEepromChip(argv[i], &o->ee.chip) != 0) {
                fprintf(stderr, "lnx: bad --eeprom-chip '%s' (none|93c46|93c56|93c66|93c76|93c86 or 0-5)\n",
                        i < argc ? argv[i] : "");
                return -1;
            }
            o->ee.chip_set = 1;
        } else if (strcmp(a, "--eeprom-lynxsd") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --eeprom-lynxsd needs 0|1\n"); return -1; }
            if (strcmp(argv[i], "0") == 0)      { o->ee.lynxsd = 0; }
            else if (strcmp(argv[i], "1") == 0) { o->ee.lynxsd = 1; }
            else { fprintf(stderr, "lnx: bad --eeprom-lynxsd '%s' (use 0|1)\n", argv[i]); return -1; }
            o->ee.lynxsd_set = 1;
        } else if (strcmp(a, "--eeprom-word") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: --eeprom-word needs 8|16\n"); return -1; }
            if (strcmp(argv[i], "16") == 0)     { o->ee.word8 = 0; }
            else if (strcmp(argv[i], "8") == 0) { o->ee.word8 = 1; }
            else { fprintf(stderr, "lnx: bad --eeprom-word '%s' (use 8|16)\n", argv[i]); return -1; }
            o->ee.word8_set = 1;
        } else if (strcmp(a, "--bank0") == 0) {
            if (++i >= argc || ParseUInt(argv[i], &o->bank0) != 0) { fprintf(stderr, "lnx: bad --bank0\n"); return -1; }
            o->bank0_set = 1;
        } else if (strcmp(a, "--bank1") == 0) {
            if (++i >= argc || ParseUInt(argv[i], &o->bank1) != 0) { fprintf(stderr, "lnx: bad --bank1\n"); return -1; }
            o->bank1_set = 1;
        } else if (strcmp(a, "--version") == 0) {
            if (++i >= argc || ParseUInt(argv[i], &o->version) != 0) { fprintf(stderr, "lnx: bad --version\n"); return -1; }
            o->version_set = 1;
        } else if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
            if (++i >= argc) { fprintf(stderr, "lnx: -o needs a path\n"); return -1; }
            o->output = argv[i];
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "lnx: unknown option '%s'\n", a);
            return -1;
        } else {
            if (o->file) {
                fprintf(stderr, "lnx: more than one input file given\n");
                return -1;
            }
            o->file = a;
        }
    }
    if (!o->file) {
        fprintf(stderr, "lnx: no input file\n");
        return -1;
    }
    return 0;
}

static int CmdInfo(const Options* o, int dump)
{
    size_t         size;
    unsigned char* buf = ReadFile(o->file, &size);
    LnxRawHeader   raw;
    LnxHeader      h;

    if (!buf) {
        return 1;
    }
    if (size < LNX_HEADER_SIZE) {
        fprintf(stderr, "lnx: '%s' is smaller than a 64-byte header\n", o->file);
        free(buf);
        return 1;
    }
    memcpy(raw.bytes, buf, LNX_HEADER_SIZE);
    free(buf);

    if (!LnxHasMagic(&raw)) {
        fprintf(stderr, "lnx: '%s' has no LYNX header (use 'lnx create' to add one)\n", o->file);
        return 1;
    }
    if (dump) {
        LnxRawDump(&raw, stdout);
    } else {
        LnxHeaderDecode(&raw, &h);
        LnxHeaderPrint(&h, stdout);
    }
    return 0;
}

static int CmdPatch(const Options* o)
{
    size_t         size;
    unsigned char* buf = ReadFile(o->file, &size);
    LnxRawHeader   raw;
    LnxHeader      h;
    const char*    out;
    int            rc;

    if (!buf) {
        return 1;
    }
    if (size < LNX_HEADER_SIZE) {
        fprintf(stderr, "lnx: '%s' is smaller than a 64-byte header\n", o->file);
        free(buf);
        return 1;
    }
    memcpy(raw.bytes, buf, LNX_HEADER_SIZE);
    if (!LnxHasMagic(&raw)) {
        fprintf(stderr, "lnx: '%s' has no LYNX header (use 'lnx create' to add one)\n", o->file);
        free(buf);
        return 1;
    }

    LnxHeaderDecode(&raw, &h);
    if (ApplyOptions(o, &h) != 0) {
        free(buf);
        return 1;
    }
    LnxHeaderEncode(&h, &raw);
    memcpy(buf, raw.bytes, LNX_HEADER_SIZE); /* rewrite header, keep body */

    out = o->output ? o->output : o->file;
    rc  = WriteFile(out, buf, size);
    free(buf);
    return rc != 0;
}

static int CmdCreate(const Options* o)
{
    size_t         body;
    unsigned char* raw_body;
    LnxRawHeader   raw;
    LnxHeader      h;
    unsigned char* out;
    int            rc;

    if (!o->output) {
        fprintf(stderr, "lnx: 'create' requires -o <file.lnx>\n");
        return 1;
    }

    raw_body = ReadFile(o->file, &body);
    if (!raw_body) {
        return 1;
    }
    if (body >= 4 && memcmp(raw_body, LNX_MAGIC, 4) == 0) {
        fprintf(stderr, "lnx: warning: '%s' already starts with LYNX; "
                        "creating would double-head it\n", o->file);
    }

    LnxHeaderDefaults(&h);
    if (ApplyOptions(o, &h) != 0) {
        free(raw_body);
        return 1;
    }
    LnxHeaderEncode(&h, &raw);

    out = (unsigned char*)malloc(LNX_HEADER_SIZE + (body ? body : 1));
    if (!out) {
        fprintf(stderr, "lnx: out of memory\n");
        free(raw_body);
        return 1;
    }
    memcpy(out, raw.bytes, LNX_HEADER_SIZE);
    memcpy(out + LNX_HEADER_SIZE, raw_body, body);
    free(raw_body);

    rc = WriteFile(o->output, out, LNX_HEADER_SIZE + body);
    free(out);
    return rc != 0;
}

int main(int argc, char** argv)
{
    const char* cmd;
    Options     o;

    if (argc < 2) {
        Usage(stderr);
        return 1;
    }

    cmd = argv[1];
    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        Usage(stdout);
        return 0;
    }
    if (strcmp(cmd, "-V") == 0 || strcmp(cmd, "--tool-version") == 0) {
        printf("lnx %s\n", LNX_TOOL_VERSION);
        return 0;
    }

    memset(&o, 0, sizeof(o));

    if (strcmp(cmd, "info") == 0) {
        if (ParseFieldArgs(argc, argv, 2, &o) != 0) { return 1; }
        return CmdInfo(&o, 0);
    }
    if (strcmp(cmd, "dump") == 0) {
        if (ParseFieldArgs(argc, argv, 2, &o) != 0) { return 1; }
        return CmdInfo(&o, 1);
    }
    if (strcmp(cmd, "patch") == 0) {
        if (ParseFieldArgs(argc, argv, 2, &o) != 0) { return 1; }
        return CmdPatch(&o);
    }
    if (strcmp(cmd, "create") == 0) {
        if (ParseFieldArgs(argc, argv, 2, &o) != 0) { return 1; }
        return CmdCreate(&o);
    }

    fprintf(stderr, "lnx: unknown command '%s'\n\n", cmd);
    Usage(stderr);
    return 1;
}
