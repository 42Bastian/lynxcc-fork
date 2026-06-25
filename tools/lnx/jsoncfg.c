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
** jsoncfg.c
**
** A deliberately tiny JSON reader for the lnx per-game header config. It accepts
** exactly one flat object whose values are strings, integers or the
** true/false/null literals -- the subset documented in
** design/LYNX_LNX_TOOL_DESIGN.md §4. Nested objects, arrays and floating point
** are rejected on purpose so the tool needs no third-party JSON dependency.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "jsoncfg.h"

typedef enum {
    VAL_STRING,
    VAL_NUMBER,
    VAL_BOOL,
    VAL_NULL
} ValType;

typedef struct {
    ValType type;
    char    str[64];   /* decoded string value (truncated past 63 chars) */
    long    num;       /* integer value when type == VAL_NUMBER/VAL_BOOL */
} Value;

typedef struct {
    const char*   p;     /* cursor */
    const char*   end;
    int           error; /* set once an error has been reported */
    LnxEepromSpec ee;    /* EEPROM fields accumulated, composed after parse */
} Parser;

static void Fail(Parser* ps, const char* msg)
{
    if (!ps->error) {
        fprintf(stderr, "lnx: config: %s\n", msg);
        ps->error = 1;
    }
}

static void SkipWs(Parser* ps)
{
    while (ps->p < ps->end && isspace((unsigned char)*ps->p)) {
        ps->p++;
    }
}

/* Parse a JSON string into `out` (NUL-terminated, truncated to cap-1). */
static int ParseString(Parser* ps, char* out, size_t cap)
{
    size_t n = 0;

    if (ps->p >= ps->end || *ps->p != '"') {
        Fail(ps, "expected a string");
        return -1;
    }
    ps->p++; /* opening quote */

    while (ps->p < ps->end && *ps->p != '"') {
        char c = *ps->p++;
        if (c == '\\') {
            if (ps->p >= ps->end) {
                break;
            }
            char e = *ps->p++;
            switch (e) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case 'r':  c = '\r'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'u':
                    Fail(ps, "\\u escapes are not supported in config strings");
                    return -1;
                default:
                    Fail(ps, "invalid string escape");
                    return -1;
            }
        }
        if (n < cap - 1) {
            out[n++] = c;
        }
    }
    if (ps->p >= ps->end || *ps->p != '"') {
        Fail(ps, "unterminated string");
        return -1;
    }
    ps->p++; /* closing quote */
    out[n] = '\0';
    return 0;
}

static int ParseValue(Parser* ps, Value* v)
{
    SkipWs(ps);
    if (ps->p >= ps->end) {
        Fail(ps, "unexpected end of input");
        return -1;
    }

    char c = *ps->p;

    if (c == '"') {
        v->type = VAL_STRING;
        return ParseString(ps, v->str, sizeof(v->str));
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        char*       e = NULL;
        long        n;
        const char* start = ps->p;
        /* reject floats: a '.' or exponent is not allowed for our integer fields */
        const char* scan = ps->p;
        if (*scan == '-') {
            scan++;
        }
        while (scan < ps->end && isdigit((unsigned char)*scan)) {
            scan++;
        }
        if (scan < ps->end && (*scan == '.' || *scan == 'e' || *scan == 'E')) {
            Fail(ps, "fractional/exponent numbers are not supported");
            return -1;
        }
        n = strtol(start, &e, 10);
        if (e == start) {
            Fail(ps, "malformed number");
            return -1;
        }
        ps->p = e;
        v->type = VAL_NUMBER;
        v->num  = n;
        return 0;
    }
    if (strncmp(ps->p, "true", 4) == 0) {
        ps->p += 4; v->type = VAL_BOOL; v->num = 1; return 0;
    }
    if (strncmp(ps->p, "false", 5) == 0) {
        ps->p += 5; v->type = VAL_BOOL; v->num = 0; return 0;
    }
    if (strncmp(ps->p, "null", 4) == 0) {
        ps->p += 4; v->type = VAL_NULL; return 0;
    }
    if (c == '{' || c == '[') {
        Fail(ps, "nested objects/arrays are not supported in config");
        return -1;
    }
    Fail(ps, "unexpected token");
    return -1;
}

/* Apply one decoded key/value pair to the header. */
static int ApplyPair(Parser* ps, const char* key, const Value* v, LnxHeader* h)
{
    if (strcmp(key, "cartname") == 0) {
        if (v->type != VAL_STRING) { Fail(ps, "\"cartname\" must be a string"); return -1; }
        LnxSetCartname(h, v->str);
        return 0;
    }
    if (strcmp(key, "manufacturer") == 0) {
        if (v->type != VAL_STRING) { Fail(ps, "\"manufacturer\" must be a string"); return -1; }
        LnxSetManufname(h, v->str);
        return 0;
    }
    if (strcmp(key, "rotation") == 0) {
        if (v->type == VAL_STRING) {
            unsigned char r;
            if (LnxParseRotation(v->str, &r) != 0) {
                Fail(ps, "\"rotation\" must be none/left/right or 0/1/2");
                return -1;
            }
            h->rotation = r;
            return 0;
        }
        if (v->type == VAL_NUMBER && v->num >= 0 && v->num <= 2) {
            h->rotation = (unsigned char)v->num;
            return 0;
        }
        Fail(ps, "\"rotation\" must be none/left/right or 0/1/2");
        return -1;
    }
    if (strcmp(key, "audin") == 0) {
        if (v->type == VAL_BOOL) { h->audin = (unsigned char)(v->num ? 1 : 0); return 0; }
        if (v->type == VAL_NUMBER && (v->num == 0 || v->num == 1)) { h->audin = (unsigned char)v->num; return 0; }
        Fail(ps, "\"audin\" must be 0, 1, true or false");
        return -1;
    }
    if (strcmp(key, "eeprom_flag") == 0 || strcmp(key, "eeprom") == 0) {
        if (v->type != VAL_NUMBER || v->num < 0 || v->num > 0xFF) { Fail(ps, "\"eeprom_flag\" must be 0..255"); return -1; }
        ps->ee.raw = (unsigned char)v->num;
        ps->ee.raw_set = 1;
        return 0;
    }
    if (strcmp(key, "eeprom_chip") == 0) {
        if (v->type == VAL_STRING) {
            if (LnxParseEepromChip(v->str, &ps->ee.chip) != 0) {
                Fail(ps, "\"eeprom_chip\" must be none/93c46/93c56/93c66/93c76/93c86 or 0-5");
                return -1;
            }
        } else if (v->type == VAL_NUMBER && v->num >= 0 && v->num <= LNX_EE_CHIP_MAX) {
            ps->ee.chip = (unsigned char)v->num;
        } else {
            Fail(ps, "\"eeprom_chip\" must be a chip name or 0-5");
            return -1;
        }
        ps->ee.chip_set = 1;
        return 0;
    }
    if (strcmp(key, "eeprom_lynxsd") == 0) {
        if (v->type == VAL_BOOL) { ps->ee.lynxsd = v->num ? 1 : 0; }
        else if (v->type == VAL_NUMBER && (v->num == 0 || v->num == 1)) { ps->ee.lynxsd = (int)v->num; }
        else { Fail(ps, "\"eeprom_lynxsd\" must be 0, 1, true or false"); return -1; }
        ps->ee.lynxsd_set = 1;
        return 0;
    }
    if (strcmp(key, "eeprom_word_size") == 0) {
        if (v->type != VAL_NUMBER || (v->num != 8 && v->num != 16)) { Fail(ps, "\"eeprom_word_size\" must be 8 or 16"); return -1; }
        ps->ee.word8 = (v->num == 8) ? 1 : 0;
        ps->ee.word8_set = 1;
        return 0;
    }
    if (strcmp(key, "bank0_page_size") == 0) {
        if (v->type != VAL_NUMBER || v->num < 0 || v->num > 0xFFFF) { Fail(ps, "\"bank0_page_size\" must be 0..65535"); return -1; }
        h->page_size_bank0 = (unsigned)v->num;
        return 0;
    }
    if (strcmp(key, "bank1_page_size") == 0) {
        if (v->type != VAL_NUMBER || v->num < 0 || v->num > 0xFFFF) { Fail(ps, "\"bank1_page_size\" must be 0..65535"); return -1; }
        h->page_size_bank1 = (unsigned)v->num;
        return 0;
    }
    if (strcmp(key, "version") == 0) {
        if (v->type != VAL_NUMBER || v->num < 0 || v->num > 0xFFFF) { Fail(ps, "\"version\" must be 0..65535"); return -1; }
        h->version = (unsigned)v->num;
        return 0;
    }

    fprintf(stderr, "lnx: config: unknown key \"%s\"\n", key);
    ps->error = 1;
    return -1;
}

static int ParseObject(Parser* ps, LnxHeader* h)
{
    SkipWs(ps);
    if (ps->p >= ps->end || *ps->p != '{') {
        Fail(ps, "config must be a JSON object");
        return -1;
    }
    ps->p++; /* '{' */
    SkipWs(ps);

    if (ps->p < ps->end && *ps->p == '}') {
        ps->p++;
        return 0; /* empty object: nothing to change */
    }

    for (;;) {
        char  key[32];
        Value v;

        SkipWs(ps);
        if (ParseString(ps, key, sizeof(key)) != 0) {
            return -1;
        }
        SkipWs(ps);
        if (ps->p >= ps->end || *ps->p != ':') {
            Fail(ps, "expected ':' after key");
            return -1;
        }
        ps->p++; /* ':' */

        if (ParseValue(ps, &v) != 0) {
            return -1;
        }
        if (ApplyPair(ps, key, &v, h) != 0) {
            return -1;
        }

        SkipWs(ps);
        if (ps->p >= ps->end) {
            Fail(ps, "unterminated object");
            return -1;
        }
        if (*ps->p == ',') {
            ps->p++;
            continue;
        }
        if (*ps->p == '}') {
            ps->p++;
            break;
        }
        Fail(ps, "expected ',' or '}'");
        return -1;
    }
    return 0;
}

int LnxApplyJsonConfig(const char* path, LnxHeader* h)
{
    FILE*  f;
    long   size;
    char*  buf;
    Parser ps;
    int    rc;

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "lnx: cannot open config '%s'\n", path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0) {
        fprintf(stderr, "lnx: cannot size config '%s'\n", path);
        fclose(f);
        return -1;
    }
    rewind(f);

    buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "lnx: out of memory reading config\n");
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "lnx: short read on config '%s'\n", path);
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[size] = '\0';

    ps.p     = buf;
    ps.end   = buf + size;
    ps.error = 0;
    memset(&ps.ee, 0, sizeof(ps.ee));

    rc = ParseObject(&ps, h);
    if (rc == 0) {
        SkipWs(&ps);
        if (ps.p < ps.end) {
            Fail(&ps, "trailing data after config object");
            rc = -1;
        }
    }
    if (rc == 0) {
        /* Apply the accumulated EEPROM fields once, in fixed order, onto the
        ** header's current byte -- so the result is independent of the order
        ** the eeprom_* keys appeared in the file. */
        h->eeprom = LnxEepromCompose(h->eeprom, &ps.ee);
    }
    if (ps.error) {
        rc = -1;
    }

    free(buf);
    return rc;
}
