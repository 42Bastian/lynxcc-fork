/* SPDX-License-Identifier: MIT
 *
 * abcrom - tune test-ROM utility for the lynxcc sound engine.
 *
 * Part of the Lynx Game Development SDK (lynxcc).  abcrom patches one or more
 * compiled tune streams into a runnable .lnx by overwriting a fixed reserved
 * region of a pre-assembled template ROM, so a tune reaches a playable
 * cartridge in milliseconds with no cc65 at test time
 * (design/LYNX_SND_ENGINE_DESIGN.md sec. 9).
 *
 * abcrom is a pure file-to-file transform: it never invokes cc65, abccc, or any
 * other program (no system()), never creates temporary files, and never launches
 * an emulator.  Its input is an already-compiled event stream (a .bin produced by
 * `abccc -f bin`); compile the tune first, then hand abcrom the named file.  This
 * keeps the host build free of POSIX-only facilities and identical across Linux,
 * macOS, and Windows/MSVC (design sec. 9.6).
 *
 * Reserved-region layout (sec. 9.3):
 *   0  4  magic "ABCR"
 *   4  1  format version
 *   5  1  channel index (0..3)
 *   6  2  capacity (LE)
 *   8  2  used     (LE)
 *  10  N  payload (event stream, must end with END within capacity)
 *
 * Usage:
 *   abcrom -o FILE.lnx [opts] tune.bin
 *     -o FILE          output .lnx (REQUIRED)
 *     -t FILE          template .lnx (default: ./template.lnx)
 *     --channels MAP   place streams on channels, e.g. 0:bass.bin,1:lead.bin
 *     --name STR       cart-name field in the .lnx header
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ABCCC_FORMAT_VERSION 1
#define MAXROM (1<<20)

static const char *prog = "abcrom";
static uint8_t rom[MAXROM];
static long romlen;

/* one channel -> compiled stream assignment */
struct Slot { int chan; const char *bin; };
static struct Slot slots[4];
static int nslots = 0;

static long load_file(const char *path, uint8_t *dst, long cap) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "%s: cannot open %s\n", prog, path); exit(1); }
    long n = (long)fread(dst, 1, cap, f);
    fclose(f);
    return n;
}

/* find the ABCR region for `chan` in rom[]; returns offset of magic, or -1 */
static long find_region(int chan) {
    for (long i = 64; i + 10 <= romlen; i++) {
        if (rom[i]=='A' && rom[i+1]=='B' && rom[i+2]=='C' && rom[i+3]=='R'
            && rom[i+5] == (uint8_t)chan)
            return i;
    }
    return -1;
}

static void patch_region(int chan, const char *bin) {
    long off = find_region(chan);
    if (off < 0) { fprintf(stderr, "%s: template has no region for channel %d\n", prog, chan); exit(1); }
    if (rom[off+4] != ABCCC_FORMAT_VERSION) {
        fprintf(stderr, "%s: template region %d format version %d != %d (rebuild template)\n",
                prog, chan, rom[off+4], ABCCC_FORMAT_VERSION);
        exit(1);
    }
    int cap = rom[off+6] | (rom[off+7] << 8);

    /* load the pre-compiled stream bytes (abccc -f bin output) */
    uint8_t payload[8192];
    long plen = load_file(bin, payload, sizeof payload);

    if (plen > cap) {
        fprintf(stderr, "%s: tune (%ld bytes) exceeds region capacity (%d) for channel %d\n",
                prog, plen, cap, chan);
        exit(1);
    }
    /* write used + payload */
    rom[off+8] = plen & 0xFF;
    rom[off+9] = (plen >> 8) & 0xFF;
    memcpy(rom + off + 10, payload, plen);
}

int main(int argc, char **argv) {
    const char *outfile = NULL, *tmpl = "template.lnx", *name = NULL;
    const char *single = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"-o") && i+1<argc) outfile = argv[++i];
        else if (!strcmp(argv[i],"-t") && i+1<argc) tmpl = argv[++i];
        else if (!strcmp(argv[i],"--name") && i+1<argc) name = argv[++i];
        else if (!strcmp(argv[i],"--channels") && i+1<argc) {
            char *map = argv[++i], *tok = strtok(map, ",");
            while (tok) {
                int ch; char *colon = strchr(tok, ':');
                if (colon) { ch = atoi(tok); slots[nslots].chan = ch; slots[nslots].bin = colon+1; nslots++; }
                tok = strtok(NULL, ",");
            }
        }
        else if (argv[i][0] != '-') single = argv[i];
        else { fprintf(stderr, "%s: unknown option %s\n", prog, argv[i]); return 2; }
    }

    if (single && nslots == 0) { slots[0].chan = 0; slots[0].bin = single; nslots = 1; }
    if (nslots == 0) { fprintf(stderr, "%s: no compiled tune stream given (abccc -f bin output)\n", prog); return 2; }
    if (!outfile) { fprintf(stderr, "%s: -o FILE.lnx is required\n", prog); return 2; }

    romlen = load_file(tmpl, rom, MAXROM);
    if (romlen < 64) { fprintf(stderr, "%s: %s is not a .lnx\n", prog, tmpl); return 1; }

    for (int s = 0; s < nslots; s++)
        patch_region(slots[s].chan, slots[s].bin);

    /* optional cart name (.lnx header field at offset 10, 32 bytes) */
    if (name) {
        memset(rom + 10, 0, 32);
        strncpy((char *)rom + 10, name, 31);
    }

    FILE *out = fopen(outfile, "wb");
    if (!out) { fprintf(stderr, "%s: cannot write %s\n", prog, outfile); return 1; }
    fwrite(rom, 1, romlen, out);
    fclose(out);
    fprintf(stderr, "%s: wrote %s (%ld bytes)\n", prog, outfile, romlen);

    return 0;
}
