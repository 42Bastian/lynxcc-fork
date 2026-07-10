# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in this directory.
#
# Generates the five sprite assets used by spritefx.c:
#
#   cube.pcx crystal.pcx star.pcx ball.pcx  - four 16x16 shapes
#   marker.pcx                              - a 5x5 magenta action-point pip
#
# The four shapes deliberately use DISJOINT pen-index ranges so a single
# 16-entry display palette (declared once in spritefx.c) colours every one of
# them with an identity SCB penpal (pixel value v -> pen v):
#
#   pen 0        transparent background (black)
#   pens 1..3    cube      (blue-grey faces)
#   pens 4..6    crystal   (cyan/teal facets)
#   pens 7..9    star      (yellow/gold)
#   pens 10..12  ball      (red/orange sphere)
#   pen 13       white     (HUD text)
#   pen 15       magenta   (action-point marker)
#
# sp65 picks each sprite's bit depth from its highest pen index, so the four
# shapes come out at 2/3/4/4 bpp; spritefx.c reads the generated _COLORS
# define back to set the matching SPRCTL0 depth bits. The marker tops out at
# pen 15, so it is a 4bpp sprite whose lone lit pixel value draws magenta.
#
# The PCX RGB palette written here is cosmetic only - sp65's lynx-sprite
# converter encodes pixel INDICES, not colours - but we fill it with the same
# hues spritefx.c uses so the .pcx files preview sensibly in an image viewer.

from PIL import Image, ImageDraw

W = H = 16

# RGB preview palette, index -> (r,g,b). Mirrors the 12-bit palette in
# spritefx.c (each channel is the 4-bit value scaled to 8 bits: n*17).
def c(r, g, b):
    return (r * 17, g * 17, b * 17)

PAL = {
    0:  c(0, 0, 0),
    1:  c(7, 9, 12), 2:  c(4, 6, 9),  3:  c(2, 3, 6),      # cube
    4:  c(7, 15, 15), 5: c(2, 10, 12), 6: c(1, 5, 8),      # crystal
    7:  c(15, 14, 3), 8: c(14, 10, 1), 9: c(9, 6, 0),      # star
    10: c(15, 13, 11), 11: c(14, 4, 3), 12: c(9, 1, 1),    # ball
    13: c(15, 15, 15),                                     # text (white)
    14: c(8, 8, 8),                                        # spare
    15: c(15, 0, 15),                                      # marker (magenta)
}


def new_img():
    img = Image.new("P", (W, H), 0)
    flat = []
    for i in range(256):
        flat.extend(PAL.get(i, (0, 0, 0)))
    img.putpalette(flat)
    return img


def save(img, name):
    img.save(name)
    print("wrote", name)


# ---- cube: an isometric box with top / left / right faces ----------------
def make_cube():
    img = new_img()
    d = ImageDraw.Draw(img)
    top   = [(8, 1), (14, 4), (8, 7), (2, 4)]     # lightest
    left  = [(2, 4), (8, 7), (8, 14), (2, 11)]    # darkest
    right = [(8, 7), (14, 4), (14, 11), (8, 14)]  # mid
    d.polygon(left,  fill=3)
    d.polygon(right, fill=2)
    d.polygon(top,   fill=1)
    return img


# ---- crystal: a faceted gem / diamond ------------------------------------
def make_crystal():
    img = new_img()
    d = ImageDraw.Draw(img)
    d.polygon([(8, 0), (13, 6), (8, 15), (3, 6)], fill=5)   # whole stone
    d.polygon([(8, 0), (3, 6), (8, 15)], fill=4)            # left facet (light)
    d.polygon([(3, 6), (13, 6), (8, 0)], fill=6)            # table (dark)
    return img


# ---- star: a five-point star with a bright core --------------------------
def make_star():
    import math
    img = new_img()
    d = ImageDraw.Draw(img)
    cx = cy = 8.0
    outer, inner = 7.5, 3.0
    pts = []
    for k in range(10):
        ang = -math.pi / 2 + k * math.pi / 5
        r = outer if k % 2 == 0 else inner
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    d.polygon(pts, fill=7)                                  # body
    core = [(cx + (inner - 0.5) * math.cos(-math.pi / 2 + k * math.pi / 5),
             cy + (inner - 0.5) * math.sin(-math.pi / 2 + k * math.pi / 5))
            if k % 2 else
            (cx + 3.2 * math.cos(-math.pi / 2 + k * math.pi / 5),
             cy + 3.2 * math.sin(-math.pi / 2 + k * math.pi / 5))
            for k in range(10)]
    d.polygon(core, fill=8)                                 # inner highlight
    d.point((8, 8), fill=8)
    return img


# ---- ball: a shaded sphere ------------------------------------------------
def make_ball():
    img = new_img()
    d = ImageDraw.Draw(img)
    d.ellipse([1, 1, 14, 14], fill=12)          # shadow base
    d.ellipse([1, 1, 13, 13], fill=11)          # main body (offset up-left)
    d.ellipse([4, 4, 8, 8],   fill=10)          # specular highlight
    return img


# ---- marker: a magenta plus on a 16x16 field, action point at centre (8,8).
# A full 16x16 (like the shapes) keeps sp65's PCX reader happy - odd, narrow
# widths trip its end-of-data check - while pen 0 stays transparent so only the
# cross shows. Arms reach 3 px from the centre; ax=ay=8 pins the pivot there.
def make_marker():
    img = new_img()
    px = img.load()
    for d in range(-3, 4):
        px[8 + d, 8] = 15
        px[8, 8 + d] = 15
    return img


save(make_cube(),    "cube.pcx")
save(make_crystal(), "crystal.pcx")
save(make_star(),    "star.pcx")
save(make_ball(),    "ball.pcx")
save(make_marker(),  "marker.pcx")
