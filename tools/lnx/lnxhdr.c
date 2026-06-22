/*
** lnxhdr.c
**
** Implementation of the 64-byte .lnx cartridge header model.
** See design/LYNX_LNX_TOOL_DESIGN.md and tools/lnx/lnxhdr.h.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "lnxhdr.h"

/* Field offsets within the 64-byte header (little-endian words). */
#define OFF_MAGIC    0
#define OFF_BANK0    4
#define OFF_BANK1    6
#define OFF_VERSION  8
#define OFF_CARTNAME 10
#define OFF_MANUF    42
#define OFF_ROTATION 58
#define OFF_AUDIN    59
#define OFF_EEPROM   60
#define OFF_SPARE    61

static unsigned ReadWord(const unsigned char* p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static void WriteWord(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

void LnxHeaderDefaults(LnxHeader* h)
{
    memset(h, 0, sizeof(*h));
    h->page_size_bank0 = 1024;   /* matches cfg/lynx.cfg __BANK0BLOCKSIZE__ */
    h->page_size_bank1 = 0;
    h->version         = 1;
    h->rotation        = LNX_ROT_NONE;
    h->audin           = 0;
    h->eeprom          = 0;
    /* name fields stay all-zero (blank) until set */
}

int LnxHasMagic(const LnxRawHeader* raw)
{
    return memcmp(raw->bytes + OFF_MAGIC, LNX_MAGIC, 4) == 0;
}

void LnxHeaderDecode(const LnxRawHeader* raw, LnxHeader* h)
{
    memset(h, 0, sizeof(*h));
    h->page_size_bank0 = ReadWord(raw->bytes + OFF_BANK0);
    h->page_size_bank1 = ReadWord(raw->bytes + OFF_BANK1);
    h->version         = ReadWord(raw->bytes + OFF_VERSION);
    memcpy(h->cartname,  raw->bytes + OFF_CARTNAME, LNX_CARTNAME_LEN);
    memcpy(h->manufname, raw->bytes + OFF_MANUF,    LNX_MANUFNAME_LEN);
    h->rotation = raw->bytes[OFF_ROTATION];
    h->audin    = raw->bytes[OFF_AUDIN];
    h->eeprom   = raw->bytes[OFF_EEPROM];
    memcpy(h->spare, raw->bytes + OFF_SPARE, LNX_SPARE_LEN);
}

void LnxHeaderEncode(const LnxHeader* h, LnxRawHeader* raw)
{
    memset(raw->bytes, 0, LNX_HEADER_SIZE);
    memcpy(raw->bytes + OFF_MAGIC, LNX_MAGIC, 4);
    WriteWord(raw->bytes + OFF_BANK0,   h->page_size_bank0);
    WriteWord(raw->bytes + OFF_BANK1,   h->page_size_bank1);
    WriteWord(raw->bytes + OFF_VERSION, h->version);
    memcpy(raw->bytes + OFF_CARTNAME, h->cartname,  LNX_CARTNAME_LEN);
    memcpy(raw->bytes + OFF_MANUF,    h->manufname, LNX_MANUFNAME_LEN);
    raw->bytes[OFF_ROTATION] = h->rotation;
    raw->bytes[OFF_AUDIN]    = h->audin;
    raw->bytes[OFF_EEPROM]   = h->eeprom;
    /* spare (offsets 61-63) always written as zero (reserved) */
}

/* Copy `s` into a fixed-width NUL-padded field. Returns 1 if truncated. */
static int SetField(unsigned char* field, size_t width, const char* s,
                    const char* what)
{
    size_t len = strlen(s);
    int    truncated = 0;

    /* keep one byte for the NUL terminator, so usable text is width-1 */
    if (len > width - 1) {
        len = width - 1;
        truncated = 1;
        fprintf(stderr,
                "lnx: warning: %s truncated to %zu characters\n",
                what, width - 1);
    }
    memset(field, 0, width);
    memcpy(field, s, len);
    return truncated;
}

int LnxSetCartname(LnxHeader* h, const char* s)
{
    return SetField(h->cartname, LNX_CARTNAME_LEN, s, "cart name");
}

int LnxSetManufname(LnxHeader* h, const char* s)
{
    return SetField(h->manufname, LNX_MANUFNAME_LEN, s, "manufacturer name");
}

int LnxParseRotation(const char* s, unsigned char* out)
{
    if (strcmp(s, "none") == 0 || strcmp(s, "0") == 0) {
        *out = LNX_ROT_NONE;
        return 0;
    }
    if (strcmp(s, "left") == 0 || strcmp(s, "1") == 0) {
        *out = LNX_ROT_LEFT;
        return 0;
    }
    if (strcmp(s, "right") == 0 || strcmp(s, "2") == 0) {
        *out = LNX_ROT_RIGHT;
        return 0;
    }
    return -1;
}

const char* LnxRotationName(unsigned char r)
{
    switch (r) {
        case LNX_ROT_NONE:  return "none";
        case LNX_ROT_LEFT:  return "left";
        case LNX_ROT_RIGHT: return "right";
        default:            return "unknown";
    }
}

/* Print a fixed-width field as text, stopping at the first NUL and trimming
** trailing spaces, so "Cart name      \0" reads back as "Cart name". */
static void PrintField(FILE* f, const unsigned char* field, size_t width)
{
    size_t len = 0;
    while (len < width && field[len] != '\0') {
        len++;
    }
    while (len > 0 && field[len - 1] == ' ') {
        len--;
    }
    fputc('"', f);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = field[i];
        fputc(isprint(c) ? (int)c : '.', f);
    }
    fputc('"', f);
}

static const char* EepromChipName(unsigned chip)
{
    switch (chip) {
        case 0:  return "none";
        case 1:  return "93c46";
        case 2:  return "93c56";
        case 3:  return "93c66";
        case 4:  return "93c76";
        case 5:  return "93c86";
        default: return "reserved";
    }
}

int LnxParseEepromChip(const char* s, unsigned char* out)
{
    unsigned i;
    /* accept a bare number 0..5 */
    if (s[0] >= '0' && s[0] <= '9' && s[1] == '\0') {
        unsigned v = (unsigned)(s[0] - '0');
        if (v <= LNX_EE_CHIP_MAX) {
            *out = (unsigned char)v;
            return 0;
        }
        return -1;
    }
    /* accept a chip name */
    for (i = 0; i <= LNX_EE_CHIP_MAX; i++) {
        if (strcmp(s, EepromChipName(i)) == 0) {
            *out = (unsigned char)i;
            return 0;
        }
    }
    return -1;
}

unsigned char LnxEepromCompose(unsigned char base, const LnxEepromSpec* s)
{
    unsigned char b = s->raw_set ? s->raw : base;

    if (s->chip_set) {
        b = (unsigned char)((b & ~LNX_EE_CHIP_MASK) | (s->chip & LNX_EE_CHIP_MASK));
    }
    if (s->lynxsd_set) {
        b = (unsigned char)(s->lynxsd ? (b | LNX_EE_LYNXSD) : (b & ~LNX_EE_LYNXSD));
    }
    if (s->word8_set) {
        b = (unsigned char)(s->word8 ? (b | LNX_EE_8BIT) : (b & ~LNX_EE_8BIT));
    }
    return b;
}

/* Describe the EEPROM flag byte into `buf` (e.g. "93c46, real chip, 16-bit"). */
static void EepromDescribe(unsigned char ee, char* buf, size_t cap)
{
    unsigned chip = ee & LNX_EE_CHIP_MASK;
    if (chip == 0) {
        snprintf(buf, cap, "no EEPROM");
        return;
    }
    snprintf(buf, cap, "%s (%u bits), %s, %s",
             EepromChipName(chip),
             (unsigned)(1u << (chip + 9)),
             (ee & LNX_EE_LYNXSD) ? "LynxSD save" : "real chip",
             (ee & LNX_EE_8BIT)   ? "8-bit"       : "16-bit");
}

void LnxHeaderPrint(const LnxHeader* h, FILE* f)
{
    char ee[64];

    fprintf(f, "cart name    : ");
    PrintField(f, h->cartname, LNX_CARTNAME_LEN);
    fputc('\n', f);

    fprintf(f, "manufacturer : ");
    PrintField(f, h->manufname, LNX_MANUFNAME_LEN);
    fputc('\n', f);

    fprintf(f, "rotation     : %s (%u)\n",
            LnxRotationName(h->rotation), (unsigned)h->rotation);
    fprintf(f, "bank0 page   : %u bytes\n", h->page_size_bank0);
    fprintf(f, "bank1 page   : %u bytes\n", h->page_size_bank1);
    fprintf(f, "version      : %u\n", h->version);
    fprintf(f, "audin        : %s (%u)\n",
            h->audin ? "yes" : "no", (unsigned)h->audin);

    EepromDescribe(h->eeprom, ee, sizeof(ee));
    fprintf(f, "eeprom       : 0x%02x (%s)\n", (unsigned)h->eeprom, ee);
}

void LnxRawDump(const LnxRawHeader* raw, FILE* f)
{
    for (int row = 0; row < LNX_HEADER_SIZE; row += 16) {
        fprintf(f, "%04x  ", row);
        for (int i = 0; i < 16; i++) {
            fprintf(f, "%02x ", raw->bytes[row + i]);
            if (i == 7) {
                fputc(' ', f);
            }
        }
        fputc(' ', f);
        for (int i = 0; i < 16; i++) {
            unsigned char c = raw->bytes[row + i];
            fputc(isprint(c) ? (int)c : '.', f);
        }
        fputc('\n', f);
    }
}
