# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in this directory.
#
# Generates the two rotation sprite sheets used by lander.c:
#
#   lander.pcx  - 24 frames (24x24) of the lunar module, one per 15 degrees
#   flame.pcx   - 24 frames (24x24) of the descent-engine exhaust plume
#
# Each strip is FRAMES cells wide by CELL tall; sp65's --sprite-sheet driver
# slices it into a frame-pointer table (lander_anim[]/flame_anim[]) so the game
# just does "scb.data = lander_anim[angle]" to draw the ship at any heading.
# The ship is authored pointing straight up (frame 0); frame f is that art
# rotated CLOCKWISE by f*(360/FRAMES) degrees, rasterised here so no rotation
# math runs on the 6502.
#
# Pen indices (the PCX RGB palette is cosmetic; sp65 encodes indices):
#   lander: 0 transparent  1 hull-light  2 hull-dark/legs  3 window
#   flame : 0 transparent  1 inner core  2 outer plume
# The display colours live in lander.c; flame.c recolours its two pens per
# frame through the SCB penpal to make the exhaust flicker.

import math
from PIL import Image

FRAMES = 24
CELL   = 24
CX = CY = (CELL - 1) / 2.0     # rotation centre in pixel space
SS = 4                         # supersample factor per axis

# ---- upright templates, local coords: x right, y DOWN, origin at centre ----
# Tuned so the whole ship (legs + a bit of plume clearance) fits the 24px cell
# at every rotation (max radius < 12).

def seg_dist(px, py, ax, ay, bx, by):
    """distance from point p to segment a-b"""
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    if L2 == 0.0:
        return math.hypot(px - ax, py - ay)
    t = ((px - ax) * dx + (py - ay) * dy) / L2
    t = max(0.0, min(1.0, t))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))

def lander_pen(lx, ly):
    """pen for the ship body at local (lx,ly); 0 = empty."""
    # landing legs (thin diagonal struts + foot pads), drawn under the hull
    leg = min(seg_dist(lx, ly, -3.2, 3.0, -7.0, 7.5),
              seg_dist(lx, ly,  3.2, 3.0,  7.0, 7.5))
    foot = min(seg_dist(lx, ly, -7.0, 7.5, -8.6, 7.5),
               seg_dist(lx, ly,  7.0, 7.5,  8.6, 7.5))
    is_leg = leg < 0.9 or foot < 0.9

    # descent stage: trapezoid, wider at the bottom
    is_desc = (0.0 <= ly <= 4.0) and (abs(lx) <= 3.2 + (ly / 4.0) * 2.0)
    # nozzle bell under the descent stage
    is_noz = (4.0 <= ly <= 5.8) and (abs(lx) <= 1.6 - (ly - 4.0) * 0.35)
    # ascent stage / crew module: rounded top capsule
    is_asc = ((lx / 3.4) ** 2 + ((ly + 3.0) / 3.6) ** 2) <= 1.0
    # window
    is_win = ((lx / 1.15) ** 2 + ((ly + 4.2) / 1.15) ** 2) <= 1.0

    if is_win:
        return 3
    if is_asc or is_desc:
        # dark rim on the lower/right descent edge for a little shading
        if is_desc and (ly > 3.1 or abs(lx) > 2.2 + (ly / 4.0) * 2.0):
            return 2
        return 1
    if is_noz or is_leg:
        return 2
    return 0

def flame_pen(lx, ly):
    """pen for the exhaust plume; 0 = empty. Plume points DOWN (+y)."""
    top, bot = 5.6, 11.0
    if not (top <= ly <= bot):
        return 0
    t = (ly - top) / (bot - top)          # 0 at nozzle .. 1 at tip
    outer = 2.3 * (1.0 - t)               # plume half-width tapering to a point
    inner = 1.1 * (1.0 - t)
    if abs(lx) <= inner and ly <= bot - 1.2:
        return 1
    if abs(lx) <= outer:
        return 2
    return 0

def render(fn, penfn, npens):
    strip = Image.new("P", (CELL * FRAMES, CELL), 0)
    # cosmetic preview palette
    pal = [0, 0, 0]                                   # 0 transparent (black)
    if npens == 4:                                    # lander
        pal += [200, 205, 215,   90, 95, 110,   80, 220, 255]
    else:                                             # flame
        pal += [255, 245, 120,   250, 130, 30]
    pal += [0, 0, 0] * (256 - npens)
    strip.putpalette(pal)
    px = strip.load()

    for f in range(FRAMES):
        ang = math.radians(f * (360.0 / FRAMES))      # clockwise
        ca, sa = math.cos(ang), math.sin(ang)
        ox = f * CELL
        for yy in range(CELL):
            for xx in range(CELL):
                votes = {}
                for sy in range(SS):
                    for sx in range(SS):
                        dx = (xx + (sx + 0.5) / SS) - 0.5 - CX
                        dy = (yy + (sy + 0.5) / SS) - 0.5 - CY
                        # rotate pixel by -ang into upright template space
                        lx =  dx * ca + dy * sa
                        ly = -dx * sa + dy * ca
                        p = penfn(lx, ly)
                        votes[p] = votes.get(p, 0) + 1
                # choose the winning non-zero pen if it has enough coverage
                best, bestn = 0, votes.get(0, 0)
                for p, n in votes.items():
                    if p != 0 and n > bestn - 0.0 and (best == 0 or n > votes.get(best, 0)):
                        pass
                # pick most-covered pen overall, but let a solid non-zero pen
                # with >=40% coverage beat the transparent background
                nz = [(n, p) for p, n in votes.items() if p != 0]
                if nz:
                    n, p = max(nz)
                    if n >= SS * SS * 0.40:
                        px[ox + xx, yy] = p
                        continue
                px[ox + xx, yy] = 0
    strip.save(fn)
    print("wrote", fn, "(%d frames of %dx%d)" % (FRAMES, CELL, CELL))

render("lander.pcx", lander_pen, 4)
render("flame.pcx",  flame_pen,  3)

# A plain 8x8 pen-1 square. lander.c stretches it through the SCB h/v size to
# paint the terrain columns, landing pads and starfield dots, recolouring the
# single lit pen per draw through the SCB penpal (so one sprite serves them all).
sq = Image.new("P", (8, 8), 1)
sq.putpalette([0, 0, 0, 210, 210, 210] + [0, 0, 0] * 254)
sq.save("solidsq.pcx")
print("wrote solidsq.pcx (8x8 solid)")
