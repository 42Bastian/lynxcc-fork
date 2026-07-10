# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in this directory.

# Reproduces logo.bmp, the BMP input for the sprpcktest example. sprpck reads
# 8-bit (and 4-bit) uncompressed Windows BMP files, so an ordinary indexed BMP
# saved by PIL is a valid source image; the Makefile runs
#
#   sprpck -t6 -s4 logo.bmp logo.spr
#
# to pack it into 4-bpp Lynx sprite data. Pixel index == Lynx pen, so keep every
# index in 0..15; the sprpcktest palette colours pens 3,4,5 (a blue crystal) and
# leaves pens 6,7,8 for the SPS icon. Pen 0 is the transparent background.
from PIL import Image

# 40x26 blue crystal: 0=transparent, 3=dark edge, 4=body, 5=highlight.
G = [
    "0000000000000003333333000000000000000000",
    "0000000000000334444444330000000000000000",
    "0000000000033445555554443300000000000000",
    "0000000000344455555555444433000000000000",
    "0000000033444555555555554444330000000000",
    "0000000344445555555555555444443300000000",
    "0000033444455555555555555554444433000000",
    "0000344444555555555555555555444444300000",
    "0033444445555555555555555555544444433000",
    "0344444455555555555555555555554444444300",
    "3444444555555555555555555555555444444443",
    "3444445555555555555555555555555544444443",
    "0344444455555555555555555555554444444300",
    "0034444444555555555555555555444444443000",
    "0003444444445555555555555544444444430000",
    "0000344444444455555555554444444444300000",
    "0000034444444444455555444444444443000000",
    "0000003444444444444444444444444430000000",
    "0000000344444444444444444444444300000000",
    "0000000033444444444444444444443300000000",
    "0000000000334444444444444444330000000000",
    "0000000000003344444444444433000000000000",
    "0000000000000033444444443300000000000000",
    "0000000000000000334444330000000000000000",
    "0000000000000000003443000000000000000000",
    "0000000000000000000033000000000000000000",
]
w = len(G[0])
h = len(G)
img = Image.new("P", (w, h))
pal = [0, 0, 0,          # 0 transparent
       255, 40, 60,      # 1 red   (used by the SPS icon, not here)
       255, 200, 210,    # 2 pink
       30, 40, 120,      # 3 dark blue edge
       60, 110, 240,     # 4 blue body
       170, 210, 255]    # 5 highlight
pal += [0] * (256 * 3 - len(pal))
img.putpalette(pal)
px = img.load()
for y, row in enumerate(G):
    for x, c in enumerate(row):
        px[x, y] = int(c)
img.save("logo.bmp")
print("wrote logo.bmp %dx%d" % (w, h))
