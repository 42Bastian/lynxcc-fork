/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** textdir.c - Demonstrates the two text directions of the Lynx graphics text
** API on the Atari Lynx.
**
** gfx_settextdir selects how the text cursor ADVANCES after each gfx_outtext
** call. The glyphs themselves are never rotated and each individual string is
** always drawn as a normal left-to-right strip (see include/lynx/gfx.h and
** libraries/graphics/gfx-text.s); only the post-draw cursor move changes:
**
**   GFX_TEXT_HORIZONTAL  cursor moves RIGHT by the string width -> successive
**                        gfx_outtext calls run on, left to right.
**   GFX_TEXT_VERTICAL    cursor moves DOWN by one text line (the scaled font
**                        height) -> successive gfx_outtext calls stack into a
**                        column, each new string below the previous one.
**
** The vertical step is the line height, so with the 8x8 font the column below
** descends in even 8-px steps from its TOP token, exactly as a stack of lines
** would. (The cursor advance gives this for free; gfx_gotoxy plus repeated
** gfx_outtext is all the example does.)
**
** Both panels draw the identical token sequence "01" "02" "03" with gotoxy
** once followed by three cursorless gfx_outtext calls, so the only difference
** on screen is the direction the tokens march.
**
** The A button swaps which panel is highlighted (recoloured) so the
** correspondence between the two layouts is easy to follow; the layout itself
** is fixed.
**
** Build:  cl65 -Ors -o textdir.lnx textdir.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

/* Draw "01" "02" "03" from (x, y) using three cursorless gfx_outtext calls,
** so the active text direction alone decides how they are laid out. */
static void draw_tokens (int x, int y)
{
    gfx_gotoxy (x, y);
    gfx_outtext ("01");
    gfx_outtext ("02");
    gfx_outtext ("03");
}

void main (void)
{
    unsigned char hot = 0;              /* 0 = highlight H panel, 1 = V     */
    unsigned char joy, prev = 0, pressed;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setdefpalette ();
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);

    for (;;) {
        joy     = joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_BTN_1_MASK) {
            hot = !hot;
        }

        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLUE);
        gfx_clear ();

        /* Title (drawn horizontally, the default direction). */
        gfx_settextdir (GFX_TEXT_HORIZONTAL);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 2, "TEXT DIRECTION");

        /* Left panel: HORIZONTAL - tokens run to the right. */
        gfx_settextdir (GFX_TEXT_HORIZONTAL);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 22, "HORIZONTAL");
        gfx_setcolor (hot == 0 ? COLOR_GREEN : COLOR_GREY);
        draw_tokens (8, 34);

        /* Right panel: VERTICAL - same calls, tokens stack downward from the
        ** top; "01" is the top token and the column descends by line height. */
        gfx_settextdir (GFX_TEXT_HORIZONTAL);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (92, 22, "VERTICAL");
        gfx_settextdir (GFX_TEXT_VERTICAL);
        gfx_setcolor (hot == 1 ? COLOR_GREEN : COLOR_GREY);
        draw_tokens (116, 34);

        /* Footer hint. Reset to horizontal for normal text. */
        gfx_settextdir (GFX_TEXT_HORIZONTAL);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 92, "A = SWAP HIGHLIGHT");

        gfx_updatedisplay ();
    }
}
