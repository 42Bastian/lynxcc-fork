/* SPDX-License-Identifier: MIT
 *
 * abcrom template ROM harness (Lynx Game Development SDK).
 *
 * Built once with the normal toolchain into template.lnx; abcrom patches a
 * compiled tune stream into one of the reserved ABCR regions (regions.s) and
 * writes a fresh .lnx, with no compile or link at test time
 * (design/LYNX_SND_ENGINE_DESIGN.md sec. 9).
 *
 * On reset the harness installs the sound engine and starts playback for every
 * region whose payload is not an immediate END ($00); an unpatched region holds
 * a single END byte and stays silent.
 */

#include <lynx/lynx.h>

#define HDR 10                  /* ABCR header size (sec. 9.3) */

/* Reserved tune regions, one per channel; defined and exported in regions.s. */
extern const unsigned char abcr_region0[];
extern const unsigned char abcr_region1[];
extern const unsigned char abcr_region2[];
extern const unsigned char abcr_region3[];

static const unsigned char *const regions[4] = {
    abcr_region0, abcr_region1, abcr_region2, abcr_region3
};

void main(void)
{
    unsigned char c;

    snd_init();
    for (c = 0; c < 4; ++c) {
        const unsigned char *r = regions[c];
        unsigned used = (unsigned)r[8] | ((unsigned)r[9] << 8);
        if (used > 0 && r[HDR] != 0x00)        /* patched + not immediately END */
            snd_play(r[5], r + HDR);
    }
    for (;;) { }
}
