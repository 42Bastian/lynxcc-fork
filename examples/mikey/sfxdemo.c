/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** sfx library demo: a scrolling "sound test" for the reusable sound-effect pack
** (<lynx/sfx.h>, design/LYNX_SFX_LIBRARY_DESIGN.md, doc/sound.html section 6).
**
** Each effect is a ready-made compiled stream in lib/lynx-audio.lib; the
** sfx_<name>(ch) macro fires one on a Mikey channel.  This demo plays everything
** on channel 3 -- the convention is to reserve one channel for effects so they
** duck only themselves, never music on channels 0-2.  Only the effects a file
** references are linked into the cart (member-granularity linkage); this demo
** references all 56 effects in the pack, grouped the same way as <lynx/sfx.h>,
** so it doubles as a browser for the whole catalogue (and, as a side effect,
** pulls the entire audio library into its own cart).
**
** Controls:
**   Up / Down   move the selection (the list scrolls if it overflows)
**   A           play the selected effect on channel 3
**   B           stop channel 3 (silences a looping effect)
**
** The looping effects (alarm, ambient_hum, engine, heartbeat) run until you press
** B or pick another effect; the one-shots end on their own.  snd_active() drives
** the readout.
**
** Build:  cl65 -Ors -o sfxdemo.lnx sfxdemo.c
** (no extra .s files -- the effects come from the audio library automatically.)
*/

#include <lynx/lynx.h>          /* pulls in <lynx/sfx.h> */
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

/* One menu row per effect: a label and the effect's stream symbol.  The stream
** symbols (sfx_<name>_data) are declared by <lynx/sfx.h>; referencing one here
** is what drags that effect's object out of the audio library. */
typedef struct {
    const char          *name;
    const unsigned char *stream;
    unsigned char        loops;     /* non-zero: needs B to stop */
} SfxEntry;

static const SfxEntry sfx_list[] = {
    /* Pickups, power and progression */
    { "Coin",              sfx_coin_data,              0 },
    { "Extra life",        sfx_extra_life_data,        0 },
    { "Power up",          sfx_power_up_data,          0 },
    { "Power down",        sfx_power_down_data,        0 },
    { "Confirm",           sfx_confirm_data,           0 },
    { "Level complete",    sfx_level_complete_data,    0 },
    { "Checkpoint",        sfx_checkpoint_data,        0 },
    { "Secret found",      sfx_secret_found_data,      0 },
    { "Recharge",          sfx_recharge_data,          0 },
    { "Charge up",         sfx_charge_up_data,         0 },

    /* Movement and platforming */
    { "Jump",              sfx_jump_data,              0 },
    { "Double jump",       sfx_double_jump_data,       0 },
    { "Land",              sfx_land_data,              0 },
    { "Footstep",          sfx_footstep_data,          0 },
    { "Bounce",            sfx_bounce_data,            0 },
    { "Bounce wall",       sfx_bounce_wall_data,       0 },
    { "Whoosh",            sfx_whoosh_data,            0 },
    { "Cursor move",       sfx_cursor_move_data,       0 },

    /* Combat and damage */
    { "Attack",            sfx_attack_data,            0 },
    { "Laser",             sfx_laser_data,             0 },
    { "Explosion small",   sfx_explosion_small_data,   0 },
    { "Explosion large",   sfx_explosion_large_data,   0 },
    { "Hit",               sfx_hit_data,               0 },
    { "Player death",      sfx_player_death_data,      0 },
    { "Enemy death",       sfx_enemy_death_data,       0 },
    { "Magic cast",        sfx_magic_cast_data,        0 },
    { "Magic impact",      sfx_magic_impact_data,      0 },
    { "Shield",            sfx_shield_data,            0 },

    /* Spawning, teleport and environment */
    { "Spawn",             sfx_spawn_data,             0 },
    { "Teleport",          sfx_teleport_data,          0 },
    { "Fire",              sfx_fire_data,              0 },
    { "Wind",              sfx_wind_data,              0 },
    { "Thunder",           sfx_thunder_data,           0 },
    { "Water drop",        sfx_water_drop_data,        0 },
    { "Splash",            sfx_splash_data,            0 },
    { "Bubble",            sfx_bubble_data,            0 },

    /* Objects, doors and items */
    { "Door open",         sfx_door_open_data,         0 },
    { "Door close",        sfx_door_close_data,        0 },
    { "Switch",            sfx_switch_data,            0 },
    { "Button",            sfx_button_data,            0 },
    { "Chest open",        sfx_chest_open_data,        0 },
    { "Item equip",        sfx_item_equip_data,        0 },
    { "Key pickup",        sfx_key_pickup_data,        0 },
    { "Unlock",            sfx_unlock_data,            0 },
    { "Crumble",           sfx_crumble_data,           0 },
    { "Falling rock",      sfx_falling_rock_data,      0 },

    /* Menus and UI */
    { "Menu select",       sfx_menu_select_data,       0 },
    { "Menu back",         sfx_menu_back_data,         0 },
    { "Error",             sfx_error_data,             0 },
    { "Typing",            sfx_typing_data,            0 },
    { "Countdown",         sfx_countdown_data,         0 },

    /* Continuous / ambient (looping effects need B to stop) */
    { "Alarm (loop)",      sfx_alarm_data,             1 },
    { "Ambient hum (loop)",sfx_ambient_hum_data,       1 },
    { "Engine (loop)",     sfx_engine_data,            1 },
    { "Heartbeat (loop)",  sfx_heartbeat_data,         1 },
    { "Game over",         sfx_game_over_data,         0 },
};
#define SFX_COUNT   (sizeof (sfx_list) / sizeof (sfx_list[0]))

#define SFX_CHANNEL 3           /* effects play here (music would use 0-2) */

#define LIST_X      14
#define CURSOR_X    4
#define LIST_Y      22
#define ROW_H       11
#define VISIBLE     6

void main (void)
{
    unsigned char joy, prev = 0, pressed;
    unsigned char sel = 0;
    unsigned char top = 0;
    int           cur = -1;
    unsigned char i, last;
    int           y;

    gfx_init ();
    CLI ();                     /* let the VBL + sound-timer IRQs run */
    while (gfx_busy ()) {}
    snd_init ();

    for (;;) {
        joy     = (unsigned char)joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_UP_MASK)
            sel = (sel == 0) ? (unsigned char)(SFX_COUNT - 1) : (sel - 1);
        if (pressed & JOY_DOWN_MASK)
            sel = (sel + 1 >= SFX_COUNT) ? 0 : (sel + 1);

        if (pressed & JOY_BTN_A_MASK) {
            snd_play (SFX_CHANNEL, sfx_list[sel].stream);
            cur = sel;
        }
        if (pressed & JOY_BTN_B_MASK) {
            snd_stop_channel (SFX_CHANNEL);
            cur = -1;
        }

        if (sel < top)
            top = sel;
        else if (sel >= top + VISIBLE)
            top = (unsigned char)(sel - VISIBLE + 1);

        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();
        gfx_setfont (GFX_FONT_COMPACT);

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (CURSOR_X, 4, "lynxcc sfx pack");

        last = (unsigned char)(top + VISIBLE);
        if (last > SFX_COUNT)
            last = (unsigned char)SFX_COUNT;

        for (i = top; i < last; ++i) {
            y = LIST_Y + (int)(i - top) * ROW_H;

            if (i == sel) {
                gfx_setcolor (COLOR_GREEN);
                gfx_outtextxy (CURSOR_X, y, ">");
            }
            if ((int)i == cur && snd_active ())
                gfx_setcolor (COLOR_YELLOW);
            else if (i == sel)
                gfx_setcolor (COLOR_GREEN);
            else
                gfx_setcolor (COLOR_GREY);

            gfx_outtextxy (LIST_X, y, sfx_list[i].name);
        }

        gfx_setcolor (COLOR_WHITE);
        if (cur >= 0 && snd_active ())
            gfx_outtextxy (CURSOR_X, 86, sfx_list[cur].name);
        else
            gfx_outtextxy (CURSOR_X, 86, "idle");

        gfx_setcolor (COLOR_GREY);
        gfx_outtextxy (CURSOR_X, 96, "Up/Dn A:play B:stop");

        gfx_updatedisplay ();
    }
}
