/*
** lnxhdr.h
**
** The 64-byte Atari Lynx (.lnx) cartridge header: in-memory model plus
** load/save, field setters and pretty-printing. Part of the standalone `lnx`
** SDK tool (tools/lnx). See design/LYNX_LNX_TOOL_DESIGN.md.
*/

#ifndef LNX_LNXHDR_H
#define LNX_LNXHDR_H

#include <stddef.h>
#include <stdio.h>

#define LNX_HEADER_SIZE   64
#define LNX_MAGIC         "LYNX"
#define LNX_CARTNAME_LEN  32
#define LNX_MANUFNAME_LEN 16
#define LNX_SPARE_LEN     3

/* rotation flag values */
#define LNX_ROT_NONE  0
#define LNX_ROT_LEFT  1
#define LNX_ROT_RIGHT 2

/*
** EEPROM flag byte (offset 60) bit field. Bits 0-2 select the chip; bit 6 marks
** LynxSD save-file support; bit 7 selects the EEPROM word size. Bits 3-5 are
** reserved (zero). EEPROM size in bits is 2^(chip+9) when chip != 0.
*/
#define LNX_EE_CHIP_MASK  0x07  /* bits 0-2: 0 none,1 93c46,2 93c56,3 93c66,
                                            4 93c76,5 93c86 */
#define LNX_EE_CHIP_MAX   5     /* highest defined chip code */
#define LNX_EE_LYNXSD     0x40  /* bit 6: 1 = LynxSD save file (else real chip) */
#define LNX_EE_8BIT       0x80  /* bit 7: 1 = 8-bit word size (else 16-bit) */

/*
** A request to set the EEPROM flag byte, field by field. Each component is
** optional (its *_set flag records whether it was supplied), so callers can set
** just the chip, just the word size, the whole raw byte, or any mix. The
** components are composed onto a base byte in a fixed order by LnxEepromCompose,
** so the result never depends on the order the fields were given.
*/
typedef struct {
    int           raw_set;     unsigned char raw;    /* whole byte */
    int           chip_set;    unsigned char chip;   /* 0..5 -> bits 0-2 */
    int           lynxsd_set;  int           lynxsd; /* 0/1   -> bit 6 */
    int           word8_set;   int           word8;  /* 1 = 8-bit -> bit 7 */
} LnxEepromSpec;

/*
** Decoded header. String fields hold the raw fixed-width bytes exactly as they
** sit in the file (NUL-padded); they are NOT guaranteed NUL-terminated, so use
** the field length when printing.
*/
typedef struct {
    unsigned       page_size_bank0;
    unsigned       page_size_bank1;
    unsigned       version;
    unsigned char  cartname[LNX_CARTNAME_LEN];
    unsigned char  manufname[LNX_MANUFNAME_LEN];
    unsigned char  rotation;
    unsigned char  audin;      /* offset 59: AUDIN used for addressing (0/1) */
    unsigned char  eeprom;     /* offset 60: EEPROM flag bit field */
    unsigned char  spare[LNX_SPARE_LEN]; /* offsets 61-63, reserved */
} LnxHeader;

/* The raw 64 bytes, for `dump` and exact round-trip checks. */
typedef struct {
    unsigned char bytes[LNX_HEADER_SIZE];
} LnxRawHeader;

/* Initialise a header to the documented defaults (matches exehdr.s). */
void LnxHeaderDefaults(LnxHeader* h);

/* True if the first four bytes of `raw` are the LYNX magic. */
int LnxHasMagic(const LnxRawHeader* raw);

/* Decode 64 raw bytes into a struct. */
void LnxHeaderDecode(const LnxRawHeader* raw, LnxHeader* h);

/* Encode a struct back into 64 raw bytes (magic + spare written canonically). */
void LnxHeaderEncode(const LnxHeader* h, LnxRawHeader* raw);

/* Field setters. The string setters truncate (with a warning on stderr) to the
** fixed field width and NUL-pad the remainder. Return 0 on clean set, 1 if the
** value had to be truncated. */
int LnxSetCartname(LnxHeader* h, const char* s);
int LnxSetManufname(LnxHeader* h, const char* s);

/* Parse "none"/"left"/"right" or "0"/"1"/"2"; returns 0 on success and stores
** the flag, -1 on an unrecognised value. */
int LnxParseRotation(const char* s, unsigned char* out);
const char* LnxRotationName(unsigned char r);

/* Parse an EEPROM chip: a name ("none","93c46".."93c86") or a number 0..5.
** Returns 0 and stores the code on success, -1 on an unrecognised value. */
int LnxParseEepromChip(const char* s, unsigned char* out);

/* Compose an EEPROM flag byte: start from the spec's raw byte if it set one,
** otherwise from `base`, then overlay each supplied bit-field component (chip,
** LynxSD, word size). Components left unset keep `base`'s bits. */
unsigned char LnxEepromCompose(unsigned char base, const LnxEepromSpec* s);

/* Human-readable dump of the decoded fields to `f` (the `info` command). */
void LnxHeaderPrint(const LnxHeader* h, FILE* f);

/* Hex + ASCII dump of the raw 64 bytes (the `dump` command). */
void LnxRawDump(const LnxRawHeader* raw, FILE* f);

#endif /* LNX_LNXHDR_H */
