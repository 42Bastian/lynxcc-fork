# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in this directory.

# sheet.pcx - a 4-frame sprite sheet: 4 cells of 16x16 laid out horizontally
# (64x16 total). Each cell is one frame of a bouncing ball, so playing the
# frames in order 0,1,2,3 animates the bounce. 4-index palette:
#   0 = background (transparent via penpal value 0)
#   1 = ball body
#   2 = ball highlight
#   3 = ball shadow/outline
#
# Every cell keeps its leftmost and rightmost columns on pen 0, so no scan line
# ends on an odd pen value - the same trick heart.pcx uses to side-step Suzy's
# last-pixel bug without a pad byte (see design/LYNX_SPRITE_PADBYTE_DESIGN.md).
#
# Both suzy/spritesheet.c (the --sprite-sheet driver) and suzy/spriteslice.c
# (the manual --slice/--pop chain) consume this one image, which is the whole
# point: one sheet, many frames.

from PIL import Image

FRAMES = 4
CELL   = 16
CY     = [11, 8, 5, 8]   # ball centre Y per frame: a bounce down/up
CX     = 8
R      = 4               # ball radius

img = Image.new("P", (CELL * FRAMES, CELL))
pal = [0, 0, 0,  60, 200, 255,  200, 240, 255,  0, 60, 120] + [0] * (252 * 3)
img.putpalette(pal)
px = img.load()

for f in range(FRAMES):
    ox = f * CELL
    cy = CY[f]
    for y in range(CELL):
        for x in range(1, CELL - 1):          # keep cols 0 and 15 transparent
            dx = x - CX
            dy = y - cy
            d2 = dx * dx + dy * dy
            if d2 <= R * R:
                if dx <= -1 and dy <= -1 and d2 <= (R - 1) * (R - 1):
                    px[ox + x, y] = 2          # upper-left highlight
                elif d2 >= (R - 1) * (R - 1):
                    px[ox + x, y] = 3          # rim shadow
                else:
                    px[ox + x, y] = 1          # body

img.save("sheet.pcx")
print("wrote sheet.pcx (%dx%d, %d frames)" % (CELL * FRAMES, CELL, FRAMES))
