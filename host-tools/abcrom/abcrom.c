/* SPDX-License-Identifier: MIT
 *
 * abcrom - tune test-ROM utility for the lynxcc sound engine.
 *
 * Part of the Lynx Game Development SDK (lynxcc).  abcrom turns one or more ABC
 * tunes into a runnable .lnx by patching a pre-assembled template ROM, so a
 * tune reaches a playable cartridge in milliseconds with no cc65 at test time
 * (design/LYNX_SND_ENGINE_DESIGN.md sec. 9).  It does NOT invoke cc65; it
 * compiles each tune with abccc (which must be on PATH) and patches the bytes
 * into a fixed reserved region of the template.
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
 *   abcrom [opts] tune.abc
 *     -o FILE          output .lnx (default: <tune>.lnx)
 *     -t FILE          template .lnx (default: ./template.lnx)
 *     --channels MAP   place tunes on channels, e.g. 0:bass.abc,1:lead.abc
 *     --name STR       cart-name field in the .lnx header
 *     --run [EMU]      launch emulator after patching
 *     --bin            input is already a compiled stream (skip abccc)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define ABCCC_FORMAT_VERSION 1
#define MAXROM (1<<20)

static const char *prog = "abcrom";
static uint8_t rom[MAXROM];
static long romlen;

/* one channel -> tune assignment */
struct Slot { int chan; const char *abc; };
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

/* read a region's runtime address from the template's ld65 map sidecar so abccc
 * can resolve envelope pointers.  Returns address, or -1 if not found. */
static long region_runtime_addr(const char *tmpl, int chan) {
    char mappath[1024];
    snprintf(mappath, sizeof mappath, "%s.map", tmpl);
    FILE *f = fopen(mappath, "r");
    if (!f) return -1;
    char sym[64];
    snprintf(sym, sizeof sym, "_abcr_region%d", chan);
    char line[512];
    long addr = -1;
    while (fgets(line, sizeof line, f)) {
        char *p = strstr(line, sym);
        if (!p) continue;
        /* map line: "<symbol>  <hexaddr>  ..."; take first hex after the name */
        p += strlen(sym);
        while (*p == ' ' || *p == '\t') p++;
        addr = strtol(p, NULL, 16);
        break;
    }
    fclose(f);
    return addr;
}

static void patch_region(const char *tmpl, int chan, const char *abc, int isbin) {
    long off = find_region(chan);
    if (off < 0) { fprintf(stderr, "%s: template has no region for channel %d\n", prog, chan); exit(1); }
    if (rom[off+4] != ABCCC_FORMAT_VERSION) {
        fprintf(stderr, "%s: template region %d format version %d != %d (rebuild template)\n",
                prog, chan, rom[off+4], ABCCC_FORMAT_VERSION);
        exit(1);
    }
    int cap = rom[off+6] | (rom[off+7] << 8);

    /* obtain the compiled stream bytes */
    uint8_t payload[8192];
    long plen;
    if (isbin) {
        plen = load_file(abc, payload, sizeof payload);
    } else {
        long addr = region_runtime_addr(tmpl, chan);
        char cmd[2048], tmp[] = "/tmp/abcromXXXXXX.bin";
        int fd = mkstemps(tmp, 4);
        if (fd >= 0) close(fd);
        if (addr >= 0)
            snprintf(cmd, sizeof cmd, "abccc -f bin --org %ld -o %s %s", addr + 10, tmp, abc);
        else
            snprintf(cmd, sizeof cmd, "abccc -f bin -o %s %s", tmp, abc);
        if (system(cmd) != 0) { fprintf(stderr, "%s: abccc failed for %s\n", prog, abc); exit(1); }
        plen = load_file(tmp, payload, sizeof payload);
        remove(tmp);
    }

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
    const char *outfile = NULL, *tmpl = "template.lnx", *name = NULL, *emu = NULL;
    const char *single = NULL;
    int isbin = 0, run = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i],"-o") && i+1<argc) outfile = argv[++i];
        else if (!strcmp(argv[i],"-t") && i+1<argc) tmpl = argv[++i];
        else if (!strcmp(argv[i],"--name") && i+1<argc) name = argv[++i];
        else if (!strcmp(argv[i],"--bin")) isbin = 1;
        else if (!strcmp(argv[i],"--run")) { run = 1; if (i+1<argc && argv[i+1][0] != '-') emu = argv[++i]; }
        else if (!strcmp(argv[i],"--channels") && i+1<argc) {
            char *map = argv[++i], *tok = strtok(map, ",");
            while (tok) {
                int ch; char *colon = strchr(tok, ':');
                if (colon) { ch = atoi(tok); slots[nslots].chan = ch; slots[nslots].abc = colon+1; nslots++; }
                tok = strtok(NULL, ",");
            }
        }
        else if (argv[i][0] != '-') single = argv[i];
        else { fprintf(stderr, "%s: unknown option %s\n", prog, argv[i]); return 2; }
    }

    if (single && nslots == 0) { slots[0].chan = 0; slots[0].abc = single; nslots = 1; }
    if (nslots == 0) { fprintf(stderr, "%s: no tune given\n", prog); return 2; }

    romlen = load_file(tmpl, rom, MAXROM);
    if (romlen < 64) { fprintf(stderr, "%s: %s is not a .lnx\n", prog, tmpl); return 1; }

    for (int s = 0; s < nslots; s++)
        patch_region(tmpl, slots[s].chan, slots[s].abc, isbin);

    /* optional cart name (.lnx header field at offset 10, 32 bytes) */
    if (name) {
        memset(rom + 10, 0, 32);
        strncpy((char *)rom + 10, name, 31);
    }

    /* default output name from the first tune */
    char outbuf[1024];
    if (!outfile) {
        const char *b = strrchr(slots[0].abc, '/'); b = b ? b+1 : slots[0].abc;
        snprintf(outbuf, sizeof outbuf, "%.*s.lnx", (int)(strrchr(b,'.') ? strrchr(b,'.')-b : (long)strlen(b)), b);
        outfile = outbuf;
    }
    FILE *out = fopen(outfile, "wb");
    if (!out) { fprintf(stderr, "%s: cannot write %s\n", prog, outfile); return 1; }
    fwrite(rom, 1, romlen, out);
    fclose(out);
    fprintf(stderr, "%s: wrote %s (%ld bytes)\n", prog, outfile, romlen);

    if (run) {
        char cmd[2048];
        snprintf(cmd, sizeof cmd, "%s %s", emu ? emu : "handy", outfile);
        return system(cmd) == 0 ? 0 : 1;
    }
    return 0;
}
