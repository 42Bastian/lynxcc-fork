/*****************************************************************************/
/*                                                                           */
/*                                  sfx.h                                     */
/*                                                                           */
/*            Reusable sound-effect library for the Atari Lynx               */
/*                                                                           */
/*                                                                           */
/* Part of the Lynx Game Development SDK (lynxcc).                            */
/* SPDX-License-Identifier: MPL-2.0                                          */
/* This Source Code Form is subject to the terms of the Mozilla Public       */
/* License, v. 2.0. If a copy of the MPL was not distributed with this file, */
/* you can obtain one at https://mozilla.org/MPL/2.0/.                        */
/* See doc/licenses.html.                                                    */
/*                                                                           */
/*****************************************************************************/



#ifndef _LYNX_SFX_H
#define _LYNX_SFX_H

#if !defined(__LYNX__)
#  error This module may only be used when compiling for the Lynx game console!
#endif

/* A batteries-included pack of ready-made sound effects for the snd engine.
** Each effect is a pre-built compiled event stream played through snd_play();
** the sfx_<name>(ch) macro fires one on Mikey channel ch (0-3).  Only the
** effects a game actually references are linked into its cart (one member per
** effect in lib/lynx-audio.lib).  Author your own with the SFX_* ca65 macros in
** asminc/lynx/sfx.inc.  See doc/sound.html and design/LYNX_SFX_LIBRARY_DESIGN.md.
**
** Convention: reserve one channel for effects (channel 3 recommended) so an
** effect ducks only itself, never the music on channels 0-2 (design sec. 2.2).
** Most effects are one-shot (their stream ends on its own); the looping ambient
** effects (alarm, ambient_hum, engine, heartbeat) run until snd_stop_channel().
*/

/* Pickups, power and progression */
extern const unsigned char sfx_coin_data[];
extern const unsigned char sfx_extra_life_data[];
extern const unsigned char sfx_power_up_data[];
extern const unsigned char sfx_power_down_data[];
extern const unsigned char sfx_confirm_data[];
extern const unsigned char sfx_level_complete_data[];
extern const unsigned char sfx_checkpoint_data[];
extern const unsigned char sfx_secret_found_data[];
extern const unsigned char sfx_recharge_data[];
extern const unsigned char sfx_charge_up_data[];
#define sfx_coin(ch)  snd_play((unsigned char)(ch), sfx_coin_data)
#define sfx_extra_life(ch)  snd_play((unsigned char)(ch), sfx_extra_life_data)
#define sfx_power_up(ch)  snd_play((unsigned char)(ch), sfx_power_up_data)
#define sfx_power_down(ch)  snd_play((unsigned char)(ch), sfx_power_down_data)
#define sfx_confirm(ch)  snd_play((unsigned char)(ch), sfx_confirm_data)
#define sfx_level_complete(ch)  snd_play((unsigned char)(ch), sfx_level_complete_data)
#define sfx_checkpoint(ch)  snd_play((unsigned char)(ch), sfx_checkpoint_data)
#define sfx_secret_found(ch)  snd_play((unsigned char)(ch), sfx_secret_found_data)
#define sfx_recharge(ch)  snd_play((unsigned char)(ch), sfx_recharge_data)
#define sfx_charge_up(ch)  snd_play((unsigned char)(ch), sfx_charge_up_data)

/* Movement and platforming */
extern const unsigned char sfx_jump_data[];
extern const unsigned char sfx_double_jump_data[];
extern const unsigned char sfx_land_data[];
extern const unsigned char sfx_footstep_data[];
extern const unsigned char sfx_bounce_data[];
extern const unsigned char sfx_bounce_wall_data[];
extern const unsigned char sfx_whoosh_data[];
extern const unsigned char sfx_cursor_move_data[];
#define sfx_jump(ch)  snd_play((unsigned char)(ch), sfx_jump_data)
#define sfx_double_jump(ch)  snd_play((unsigned char)(ch), sfx_double_jump_data)
#define sfx_land(ch)  snd_play((unsigned char)(ch), sfx_land_data)
#define sfx_footstep(ch)  snd_play((unsigned char)(ch), sfx_footstep_data)
#define sfx_bounce(ch)  snd_play((unsigned char)(ch), sfx_bounce_data)
#define sfx_bounce_wall(ch)  snd_play((unsigned char)(ch), sfx_bounce_wall_data)
#define sfx_whoosh(ch)  snd_play((unsigned char)(ch), sfx_whoosh_data)
#define sfx_cursor_move(ch)  snd_play((unsigned char)(ch), sfx_cursor_move_data)

/* Combat and damage */
extern const unsigned char sfx_attack_data[];
extern const unsigned char sfx_laser_data[];
extern const unsigned char sfx_explosion_small_data[];
extern const unsigned char sfx_explosion_large_data[];
extern const unsigned char sfx_hit_data[];
extern const unsigned char sfx_player_death_data[];
extern const unsigned char sfx_enemy_death_data[];
extern const unsigned char sfx_magic_cast_data[];
extern const unsigned char sfx_magic_impact_data[];
extern const unsigned char sfx_shield_data[];
#define sfx_attack(ch)  snd_play((unsigned char)(ch), sfx_attack_data)
#define sfx_laser(ch)  snd_play((unsigned char)(ch), sfx_laser_data)
#define sfx_explosion_small(ch)  snd_play((unsigned char)(ch), sfx_explosion_small_data)
#define sfx_explosion_large(ch)  snd_play((unsigned char)(ch), sfx_explosion_large_data)
#define sfx_hit(ch)  snd_play((unsigned char)(ch), sfx_hit_data)
#define sfx_player_death(ch)  snd_play((unsigned char)(ch), sfx_player_death_data)
#define sfx_enemy_death(ch)  snd_play((unsigned char)(ch), sfx_enemy_death_data)
#define sfx_magic_cast(ch)  snd_play((unsigned char)(ch), sfx_magic_cast_data)
#define sfx_magic_impact(ch)  snd_play((unsigned char)(ch), sfx_magic_impact_data)
#define sfx_shield(ch)  snd_play((unsigned char)(ch), sfx_shield_data)

/* Spawning, teleport and environment */
extern const unsigned char sfx_spawn_data[];
extern const unsigned char sfx_teleport_data[];
extern const unsigned char sfx_fire_data[];
extern const unsigned char sfx_wind_data[];
extern const unsigned char sfx_thunder_data[];
extern const unsigned char sfx_water_drop_data[];
extern const unsigned char sfx_splash_data[];
extern const unsigned char sfx_bubble_data[];
#define sfx_spawn(ch)  snd_play((unsigned char)(ch), sfx_spawn_data)
#define sfx_teleport(ch)  snd_play((unsigned char)(ch), sfx_teleport_data)
#define sfx_fire(ch)  snd_play((unsigned char)(ch), sfx_fire_data)
#define sfx_wind(ch)  snd_play((unsigned char)(ch), sfx_wind_data)
#define sfx_thunder(ch)  snd_play((unsigned char)(ch), sfx_thunder_data)
#define sfx_water_drop(ch)  snd_play((unsigned char)(ch), sfx_water_drop_data)
#define sfx_splash(ch)  snd_play((unsigned char)(ch), sfx_splash_data)
#define sfx_bubble(ch)  snd_play((unsigned char)(ch), sfx_bubble_data)

/* Objects, doors and items */
extern const unsigned char sfx_door_open_data[];
extern const unsigned char sfx_door_close_data[];
extern const unsigned char sfx_switch_data[];
extern const unsigned char sfx_button_data[];
extern const unsigned char sfx_chest_open_data[];
extern const unsigned char sfx_item_equip_data[];
extern const unsigned char sfx_key_pickup_data[];
extern const unsigned char sfx_unlock_data[];
extern const unsigned char sfx_crumble_data[];
extern const unsigned char sfx_falling_rock_data[];
#define sfx_door_open(ch)  snd_play((unsigned char)(ch), sfx_door_open_data)
#define sfx_door_close(ch)  snd_play((unsigned char)(ch), sfx_door_close_data)
#define sfx_switch(ch)  snd_play((unsigned char)(ch), sfx_switch_data)
#define sfx_button(ch)  snd_play((unsigned char)(ch), sfx_button_data)
#define sfx_chest_open(ch)  snd_play((unsigned char)(ch), sfx_chest_open_data)
#define sfx_item_equip(ch)  snd_play((unsigned char)(ch), sfx_item_equip_data)
#define sfx_key_pickup(ch)  snd_play((unsigned char)(ch), sfx_key_pickup_data)
#define sfx_unlock(ch)  snd_play((unsigned char)(ch), sfx_unlock_data)
#define sfx_crumble(ch)  snd_play((unsigned char)(ch), sfx_crumble_data)
#define sfx_falling_rock(ch)  snd_play((unsigned char)(ch), sfx_falling_rock_data)

/* Menus and UI */
extern const unsigned char sfx_menu_select_data[];
extern const unsigned char sfx_menu_back_data[];
extern const unsigned char sfx_error_data[];
extern const unsigned char sfx_typing_data[];
extern const unsigned char sfx_countdown_data[];
#define sfx_menu_select(ch)  snd_play((unsigned char)(ch), sfx_menu_select_data)
#define sfx_menu_back(ch)  snd_play((unsigned char)(ch), sfx_menu_back_data)
#define sfx_error(ch)  snd_play((unsigned char)(ch), sfx_error_data)
#define sfx_typing(ch)  snd_play((unsigned char)(ch), sfx_typing_data)
#define sfx_countdown(ch)  snd_play((unsigned char)(ch), sfx_countdown_data)

/* Continuous / ambient (looping - stop with snd_stop_channel) */
extern const unsigned char sfx_alarm_data[];
extern const unsigned char sfx_ambient_hum_data[];
extern const unsigned char sfx_engine_data[];
extern const unsigned char sfx_heartbeat_data[];
extern const unsigned char sfx_game_over_data[];
#define sfx_alarm(ch)  snd_play((unsigned char)(ch), sfx_alarm_data)
#define sfx_ambient_hum(ch)  snd_play((unsigned char)(ch), sfx_ambient_hum_data)
#define sfx_engine(ch)  snd_play((unsigned char)(ch), sfx_engine_data)
#define sfx_heartbeat(ch)  snd_play((unsigned char)(ch), sfx_heartbeat_data)
#define sfx_game_over(ch)  snd_play((unsigned char)(ch), sfx_game_over_data)

/* End of sfx.h */
#endif
