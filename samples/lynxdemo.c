/*
** Minimal Atari Lynx sample for cc65.
**
** Initializes the TGI driver (statically linked), draws some
** graphics and text, and cycles colors while polling the joypad.
**
** Build:  cl65 -t lynx -Ors -o lynxdemo.lnx lynxdemo.c
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <6502.h>

void main (void)
{
    unsigned char color = COLOR_WHITE;
    unsigned char joy;

    /* Install and start the statically linked TGI and joystick drivers */
    tgi_install (tgi_static_stddrv);
    joy_install (joy_static_stddrv);
    tgi_init ();
    CLI ();

    /* Wait for the display to be ready */
    while (tgi_busy ()) {}

    /* Draw */
    tgi_setpalette (tgi_getdefpalette ());
    tgi_clear ();
    tgi_setcolor (COLOR_WHITE);
    tgi_outtextxy (20, 10, "Hello, Lynx!");
    tgi_setcolor (COLOR_GREEN);
    tgi_line (0, 101, 159, 30);
    tgi_setcolor (COLOR_RED);
    tgi_circle (80, 60, 25);
    tgi_updatedisplay ();

    /* Cycle the circle color with the A button; exit never */
    for (;;) {
        joy = joy_read (JOY_1);
        if (joy & JOY_BTN_1_MASK) {
            color = (color + 1) & 0x0F;
            tgi_setcolor (color);
            tgi_circle (80, 60, 25);
            tgi_updatedisplay ();
            while (joy_read (JOY_1) & JOY_BTN_1_MASK) {}
        }
    }
}
