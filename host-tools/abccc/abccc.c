/* SPDX-License-Identifier: MIT
 *
 * abccc - ABC tune cross-compiler for the lynxcc sound engine.
 *
 * Part of the Lynx Game Development SDK (lynxcc).  abccc turns a human-readable
 * ABC tune string (see design/LYNX_SND_ENGINE_DESIGN.md sec. 4) into the binary
 * event stream interpreted on target by the snd engine (sec. 5/6).  It is a
 * self-contained host tool maintained outside the cc65 build.
 *
 * Usage:
 *   abccc [options] input.abc
 *     -o FILE        output file (default: stdout)
 *     -f s|h|bin     output format: ca65 asm (.s), C header (.h), raw binary
 *     -l LABEL       symbol/label for the emitted stream (default: from -o)
 *     --org ADDR     load address for bin/h output that uses envelope pointers
 *     --no-compact   emit 2-byte legacy notes (disable default-duration packing)
 *     --validate     range/structure checks only, no output
 *     --version      print the stream format version and exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* Stream / player format version.  Bump when opcodes change; abcrom checks the
 * template's version byte against this. */
#define ABCCC_FORMAT_VERSION 1

/* Opcodes (design sec. 6.2). */
enum {
    OP_LOOP = 0x80, OP_DO = 0x81, OP_PAUSE = 0x82, OP_NOTEOFF = 0x83,
    OP_SETINSTR = 0x84, OP_NEWNOTE2 = 0x85, OP_CALLPAT = 0x86, OP_RET = 0x87,
    OP_DEFENVVOL = 0x88, OP_SETENVVOL = 0x89, OP_DEFENVFRQ = 0x8A,
    OP_SETENVFRQ = 0x8B, OP_DEFENVWAVE = 0x8C, OP_SETENVWAVE = 0x8D,
    OP_SETSTEREO = 0x8E, OP_SETATTON = 0x8F, OP_SETCHNATT = 0x90,
    OP_PLAYERFREQ = 0x91, OP_RETURNALL = 0x92,
    OP_MODE = 0x93, OP_SETDUR = 0x94, OP_SETINTEG = 0x95,
    OP_END = 0x00
};

/* Pitch mapping: the engine's 128-entry chromatic table is the canonical
 * tuning; abccc maps each ABC note to a table index of 12 semitones / octave.
 * Uppercase middle-octave C (O0) -> index BASE_C. */
#define BASE_C 36
static const int semis[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* A B C D E F G */

/* --- output buffer ------------------------------------------------------- */
static uint8_t buf[8192];
static int     blen = 0;

/* address placeholders that point at appended data blocks */
struct Ref { int pos; int block; };
static struct Ref refs[64];
static int nref = 0;
static int blockoff[64];
static int nblock = 0;

static const char *prog = "abccc";
static int errors = 0;

static void emit(uint8_t b) {
    if (blen >= (int)sizeof buf) { fprintf(stderr, "%s: stream too large\n", prog); exit(1); }
    buf[blen++] = b;
}

/* reserve a new data block id; its bytes are appended later */
static int new_block(void) { return nblock++; }

/* emit a 16-bit pointer to data block `blk` (lo,hi) as a placeholder */
static void emit_blockptr(int blk) {
    refs[nref].pos = blen; refs[nref].block = blk; nref++;
    emit(0); emit(0);
}

static void verr(const char *msg) { fprintf(stderr, "%s: %s\n", prog, msg); errors++; }

/* --- parser state -------------------------------------------------------- */
static int T = 6;     /* tempo: frames per duration unit */
static int Obase = 0; /* octave base added to every note */
static int Vval = 127, Rval = 4, Hval = 4, Kval = 4; /* AHD */
static int loopdepth = 0;
static int next_env = 1;       /* next envelope slot number */
static int ahd_emitted = 0;    /* whether the current AHD instrument is live */
static int cur_dur = -1;       /* SetDur value currently in effect (compact) */
static int compact = 1;

/* emit (or re-emit) the AHD instrument as a volume envelope (design sec. 2.1) */
static void emit_ahd(void) {
    int env = next_env++;
    int blk = new_block();
    int ac = (Vval + Rval - 1) / (Rval ? Rval : 1); /* attack segments */
    int dc = (Vval + Kval - 1) / (Kval ? Kval : 1); /* decay segments  */
    if (ac < 1) ac = 1;
    if (dc < 1) dc = 1;
    if (ac > 255) ac = 255;
    if (dc > 255) dc = 255;
    /* instrument: square tone (feedback $01), start at vol 0, ceiling V */
    emit(OP_SETINSTR); emit(0x00); emit(0x00); emit(0x01); emit(0x00); emit((uint8_t)Vval);
    /* define + select the volume envelope */
    emit(OP_DEFENVVOL); emit((uint8_t)env); emit_blockptr(blk);
    emit(OP_SETENVVOL); emit((uint8_t)env);
    /* record the block's bytes for the append phase */
    /* layout: [loopStart=0][numSegments=3][ac,+R][H,0][dc,-K] */
    blockoff[blk] = -1; /* filled at append time; store params via a side table */
    /* stash params in a parallel array keyed by block id */
    extern void ahd_stash(int blk, int ac, int R, int H, int dc, int K);
    ahd_stash(blk, ac, Rval, Hval, dc, Kval);
    ahd_emitted = 1;
}

/* parallel store of AHD block payloads */
static struct { int blk, ac, R, H, dc, K; } ahdtab[64];
static int nahd = 0;
void ahd_stash(int blk, int ac, int R, int H, int dc, int K) {
    ahdtab[nahd].blk = blk; ahdtab[nahd].ac = ac; ahdtab[nahd].R = R;
    ahdtab[nahd].H = H; ahdtab[nahd].dc = dc; ahdtab[nahd].K = K; nahd++;
}

/* note duration in frames for a duration multiplier */
static void emit_note(int idx, int frames) {
    while (frames > 255) { /* split long notes into tied chunks (sec. 11) */
        if (compact) { if (cur_dur != 255) { emit(OP_SETDUR); emit(255); cur_dur = 255; } emit((uint8_t)idx); }
        else { emit((uint8_t)idx); emit(255); }
        frames -= 255;
    }
    if (frames <= 0) frames = 1;
    if (compact) {
        if (cur_dur != frames) { emit(OP_SETDUR); emit((uint8_t)frames); cur_dur = frames; }
        emit((uint8_t)idx);
    } else {
        emit((uint8_t)idx); emit((uint8_t)frames);
    }
}

static void emit_rest(int frames) {
    while (frames > 255) { emit(OP_PAUSE); emit(255); frames -= 255; }
    if (frames < 1) frames = 1;
    emit(OP_PAUSE); emit((uint8_t)frames);
}

/* read a non-negative integer from s[*i]; returns -1 if no digits */
static int read_num(const char *s, int *i) {
    int n = -1;
    while (isdigit((unsigned char)s[*i])) { if (n < 0) n = 0; n = n * 10 + (s[*i] - '0'); (*i)++; }
    return n;
}

static void compile(const char *s) {
    int i = 0;
    /* header: compact-note mode + initial default duration */
    /* SetDur is emitted lazily by emit_note before the first note, so the
    ** initial default duration reflects whatever T is in effect there (no
    ** redundant header SetDur when the tune opens with a T command). */
    if (compact) { emit(OP_MODE); emit(0x01); }
    else { emit(OP_MODE); emit(0x00); }

    while (s[i]) {
        char c = s[i];
        if (isspace((unsigned char)c)) { i++; continue; }

        /* structure */
        if (c == '|') {
            if (s[i+1] == ':') {           /* start repeat */
                i += 2;
                int cnt = read_num(s, &i);
                if (cnt < 0) cnt = 0;       /* no count => 0 (engine loops 256x) */
                if (loopdepth) { verr("nested repeats are not supported (one loop level per channel)"); }
                emit(OP_LOOP); emit((uint8_t)cnt);
                loopdepth++;
            } else { i++; }                 /* bar line: ignore */
            continue;
        }
        if (c == ':') {                     /* end repeat (':' or ':|') */
            i++; if (s[i] == '|') i++;
            if (!loopdepth) { verr("':' without matching '|:'"); }
            else { emit(OP_DO); loopdepth--; }
            continue;
        }

        /* inline commands */
        if (c=='V'||c=='R'||c=='H'||c=='K'||c=='T'||c=='O'||c=='I'||c=='X') {
            i++; int n = read_num(s, &i);
            if (n < 0) { verr("command needs a numeric argument"); continue; }
            switch (c) {
                case 'V': Vval=n; ahd_emitted=0; break;
                case 'R': Rval=n; ahd_emitted=0; break;
                case 'H': Hval=n; ahd_emitted=0; break;
                case 'K': Kval=n; ahd_emitted=0; break;
                case 'T': T=n; break;   /* takes effect via emit_note's lazy SetDur */
                case 'O': Obase=n; break;
                case 'I': emit(OP_SETINTEG); emit(n ? 0x20 : 0x00); break; /* integrate bit5 */
                case 'X': /* taps: low 8 -> instrument feedback; 9th -> integ bit7 */
                    emit(OP_SETINSTR); emit(0x00); emit(0x00); emit((uint8_t)(n & 0xFF)); emit(0x00); emit((uint8_t)Vval);
                    emit(OP_SETINTEG); emit((n & 0x100) ? 0x80 : 0x00);
                    ahd_emitted = 0;
                    break;
            }
            continue;
        }

        /* rest */
        if (c == 'z') {
            i++; int d = read_num(s, &i); if (d < 1) d = 1;
            emit_rest(d * T);
            continue;
        }

        /* note: optional accidental, A-G / a-g, octave marks, duration */
        {
            int acc = 0;
            while (s[i]=='^'||s[i]=='_'||s[i]=='=') {
                if (s[i]=='^') acc++; else if (s[i]=='_') acc--; i++;
            }
            char nc = s[i];
            int up = 0, lower = 0;
            if (nc>='A'&&nc<='G') { up = 1; }
            else if (nc>='a'&&nc<='g') { up = 1; lower = 1; }
            if (!up) { fprintf(stderr,"%s: unexpected character '%c'\n",prog,nc); errors++; i++; continue; }
            int pc = lower ? (nc-'a') : (nc-'A'); /* 0=A..6=G */
            i++;
            int oct = Obase + (lower ? 1 : 0);
            while (s[i]==','||s[i]=='\'') { if (s[i]==',') oct--; else oct++; i++; }
            int d = read_num(s, &i); if (d < 1) d = 1;

            /* ensure an instrument/envelope is live before the first note */
            if (!ahd_emitted) emit_ahd();

            int idx = BASE_C + semis[pc] + acc + oct*12;
            if (idx < 1 || idx > 127) {
                fprintf(stderr,"%s: note out of range (index %d)\n", prog, idx);
                errors++; idx = idx < 1 ? 1 : 127;
            }
            emit_note(idx, d * T);
        }
    }
    if (loopdepth) verr("unterminated '|:' repeat");
    emit(OP_END);

    /* append AHD envelope data blocks and fix their offsets */
    for (int a = 0; a < nahd; a++) {
        blockoff[ahdtab[a].blk] = blen;
        emit(0x00);                 /* loop start (0 = play once, no loop) */
        emit(3);                    /* segment count */
        emit((uint8_t)ahdtab[a].ac); emit((uint8_t)ahdtab[a].R);   /* attack +R */
        emit((uint8_t)ahdtab[a].H);  emit(0x00);                   /* hold   0  */
        emit((uint8_t)ahdtab[a].dc); emit((uint8_t)(256 - ahdtab[a].K)); /* decay -K */
    }
}

/* --- output emitters ----------------------------------------------------- */
static int is_blockstart(int off) {
    for (int b = 0; b < nblock; b++) if (blockoff[b] == off) return b;
    return -1;
}
static const char *ref_label_at(int pos, int *which) {
    for (int r = 0; r < nref; r++) if (refs[r].pos == pos) { *which = refs[r].block; return "L"; }
    return NULL;
}

static void out_asm(FILE *f, const char *label) {
    fprintf(f, "; Generated by abccc - do not edit by hand.\n");
    fprintf(f, "; Stream format version %d.\n", ABCCC_FORMAT_VERSION);
    fprintf(f, "        .export _%s\n", label);
    fprintf(f, "        .segment \"RODATA\"\n");
    fprintf(f, "_%s:\n", label);
    for (int i = 0; i < blen; ) {
        int blk = is_blockstart(i);
        if (blk >= 0) fprintf(f, "Lenv%d:\n", blk);
        /* address placeholder? */
        int dummy;
        if (ref_label_at(i, &dummy)) {
            int which = -1;
            for (int r = 0; r < nref; r++) if (refs[r].pos == i) which = refs[r].block;
            fprintf(f, "        .byte <Lenv%d, >Lenv%d\n", which, which);
            i += 2;
            continue;
        }
        fprintf(f, "        .byte $%02X\n", buf[i]);
        i++;
    }
}

static void resolve_addresses(int org) {
    for (int r = 0; r < nref; r++) {
        int addr = org + blockoff[refs[r].block];
        buf[refs[r].pos]     = addr & 0xFF;
        buf[refs[r].pos + 1] = (addr >> 8) & 0xFF;
    }
}

static void out_header(FILE *f, const char *label) {
    fprintf(f, "/* Generated by abccc - do not edit by hand. */\n");
    fprintf(f, "/* Stream format version %d. */\n", ABCCC_FORMAT_VERSION);
    fprintf(f, "#ifndef ABCCC_%s_H\n#define ABCCC_%s_H\n", label, label);
    fprintf(f, "static const unsigned char %s[] = {\n    ", label);
    for (int i = 0; i < blen; i++) {
        fprintf(f, "0x%02X%s", buf[i], i+1<blen ? "," : "");
        if ((i & 15) == 15) fprintf(f, "\n    ");
    }
    fprintf(f, "\n};\n#endif\n");
}

static void out_bin(FILE *f) { fwrite(buf, 1, blen, f); }

/* --- driver -------------------------------------------------------------- */
int main(int argc, char **argv) {
    const char *outfile = NULL, *label = NULL, *infile = NULL;
    char fmt = 's';
    int validate = 0, org = 0, org_set = 0, have_addr_refs;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i+1 < argc) outfile = argv[++i];
        else if (!strcmp(argv[i], "-l") && i+1 < argc) label = argv[++i];
        else if (!strcmp(argv[i], "-f") && i+1 < argc) fmt = argv[++i][0];
        else if (!strcmp(argv[i], "--org") && i+1 < argc) { org = (int)strtol(argv[++i], NULL, 0); org_set = 1; }
        else if (!strcmp(argv[i], "--no-compact")) compact = 0;
        else if (!strcmp(argv[i], "--validate")) validate = 1;
        else if (!strcmp(argv[i], "--version")) { printf("abccc stream format version %d\n", ABCCC_FORMAT_VERSION); return 0; }
        else if (argv[i][0] != '-') infile = argv[i];
        else { fprintf(stderr, "%s: unknown option %s\n", prog, argv[i]); return 2; }
    }
    if (!infile) { fprintf(stderr, "%s: no input file\n", prog); return 2; }

    FILE *in = fopen(infile, "rb");
    if (!in) { fprintf(stderr, "%s: cannot open %s\n", prog, infile); return 1; }
    static char src[16384];
    size_t n = fread(src, 1, sizeof src - 1, in); src[n] = 0; fclose(in);

    compile(src);
    if (errors) { fprintf(stderr, "%s: %d error(s)\n", prog, errors); return 1; }

    if (!label) {
        /* derive from -o basename, else "tune" */
        label = "tune";
        if (outfile) {
            const char *b = strrchr(outfile, '/'); b = b ? b+1 : outfile;
            static char lb[64]; int k = 0;
            for (; b[k] && b[k] != '.' && k < 63; k++) lb[k] = b[k];
            lb[k] = 0; if (k) label = lb;
        }
    }

    if (validate) { fprintf(stderr, "%s: ok (%d bytes)\n", prog, blen); return 0; }

    have_addr_refs = (nref > 0);

    FILE *out = stdout;
    if (outfile) { out = fopen(outfile, (fmt=='b') ? "wb" : "w"); if (!out) { fprintf(stderr,"%s: cannot write %s\n",prog,outfile); return 1; } }

    if (fmt == 's') {
        out_asm(out, label);
    } else {
        if (have_addr_refs && !org_set)
            fprintf(stderr, "%s: warning: stream uses envelope pointers; pass --org for bin/h output\n", prog);
        resolve_addresses(org);
        if (fmt == 'h') out_header(out, label);
        else out_bin(out);
    }
    if (out != stdout) fclose(out);
    return 0;
}
