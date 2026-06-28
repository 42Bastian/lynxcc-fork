<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Lynx reusable sound-effect library (`sfx`) — design

Design for a **starter pack** of ready-made, linkable sound effects for **lynxcc**
games. Each effect is a small, pre-built compiled event stream that plays through
the existing `snd` engine, exposed to game code as a `sfx_`-prefixed macro. A game
calls `sfx_coin(ch)` and the engine does the rest; only the effects a game actually
references are linked into its cart.

This layer adds **no new runtime engine** and **no new stream opcodes** — it is
content (data) plus a thin macro veneer over the `snd` player documented in
`design/LYNX_SND_ENGINE_DESIGN.md`. SFX are realised entirely with features the
engine already has: frequency envelopes for pitch sweeps, volume envelopes for
decay, the LFSR taps / integrate bits for noise and timbre, and multi-note runs for
arpeggios.

- Runtime: the `snd` compiled-stream player (Kaksonen / Spruck lineage), unchanged.
- New material: per-effect `RODATA` streams + `include/lynx/sfx.h` + the ca65
  authoring macros in `asminc/lynx/sfx.inc`.
- Target: cc65 / ca65, Atari Lynx (Mikey audio).

---

## 1. Where `sfx` sits in the audio stack

Three layers, lowest to highest:

```
  mikey_snd_*   direct Mikey register pokes (octave/pitch/taps/integrate/volume)
                — one register write, no timing, fully manual. (mikey-snd.s)

  snd_*         compiled-stream player: streams advance under the sound IRQ with
                instruments + looping envelopes. Music is authored as ABC and
                compiled offline by abccc. (lynx-snd.s)

  sfx_*  <-- THIS DESIGN
                a curated set of named one-shot streams, each its own linkable
                object, played by handing the stream to snd_play() via a macro.
```

`sfx` is to sound effects what the **lynxcc** standard graphics font is to text: a
batteries-included asset set that any game can drop in, while power users can still
drop down a level (write their own stream for `snd_play`, or poke registers with
`mikey_snd_*`) whenever they want something bespoke.

The reason SFX are **streams** rather than hand-written C routines that busy-poke
registers is that the engine already owns the audio IRQ and the per-frame envelope
machinery. A C routine driving an effect frame-by-frame would have to duplicate that
timer and would block the game loop; a stream is fire-and-forget — `snd_play`
returns immediately and the IRQ renders the effect.

---

## 2. Public API

### 2.1 Header and naming

Declared in a new `include/lynx/sfx.h` (also pulled in by `lynx/lynx.h` so a single
`#include <lynx/lynx.h>` exposes everything):

```c
#include <lynx/sfx.h>

/* Each effect is a const stream symbol... */
extern const unsigned char sfx_coin_data[];
extern const unsigned char sfx_jump_data[];
/* ...one per catalogue entry (§7). */

/* ...wrapped by a function-like play macro named for the effect: */
#define sfx_coin(ch)  snd_play((unsigned char)(ch), sfx_coin_data)
#define sfx_jump(ch)  snd_play((unsigned char)(ch), sfx_jump_data)
/* ...one per catalogue entry. */
```

The public surface a game sees is therefore just `sfx_<name>(ch)`. The `_data`
stream symbols are the linkable units (§6) and are also usable directly — anything
that takes a stream pointer (e.g. `snd_play`) accepts `sfx_coin_data`.

Naming rules:

- Effect macro: `sfx_<snake_name>(ch)` — `ch` is a Mikey channel 0..3.
- Effect data: `sfx_<snake_name>_data[]` — the compiled stream.
- Names come straight from the catalogue (§7): `sfx_coin`, `sfx_power_up`,
  `sfx_laser`, `sfx_explosion_small`, `sfx_menu_select`, …

The project name **lynxcc** is bolded in prose per house style; the `sfx_` prefix
keeps the namespace clear of the `snd_`/`mikey_snd_` engine names.

### 2.2 Channel policy

SFX share Mikey's four channels with music. `snd_play(ch, …)` already replaces
whatever was on `ch`, so the convention is:

- **Reserve one channel for effects** (channel 3 by recommendation) and play all
  `sfx_*` there. Music uses 0–2. An effect then "ducks" only itself, never the
  tune. This is a game-side convention, not enforced by the library.
- A second SFX channel (2) can be reserved for games that need two simultaneous
  effects (e.g. a continuous engine drone under one-shot pickups).

No new priority engine is introduced: the policy is "last `sfx_*` on a channel
wins," which is the existing `snd_play` behaviour and is what most retro games do.

### 2.3 Stopping and one-shot vs looping

Most effects are **one-shot**: their stream ends in the `END` opcode (`$00`), the
channel goes idle on its own, and `snd_active()` reflects it. Nothing needs to be
stopped.

A handful are **continuous** by nature (ambient hum, engine, alarm, heartbeat — see
§8). Those are authored as infinite-loop streams (`Loop` count 0) and must be
stopped explicitly:

```c
sfx_alarm(3);              /* starts looping on channel 3 */
/* ...later... */
snd_stop_channel(3);       /* silence it */
```

Pausing/resuming all audio for a game-pause still uses `snd_pause()` /
`snd_continue()` and covers SFX channels too.

---

## 3. Realising effects on the engine

Every catalogue recipe (§7) is expressed with opcodes the player already
interprets. The mapping from "sound-designer intent" to stream is fixed:

| Intent | Engine feature | Opcode(s) |
|--------|----------------|-----------|
| Base timbre (square / soft / noise) | instrument: feedback taps + integrate bit | `SetInstr` `$84`, `SetInteg` `$95` |
| Pitch sweep (pew, boing, fall) | **frequency envelope** | `DefEnvFrq` `$8A` + `SetEnvFrq` `$8B` |
| Volume decay / swell | **volume envelope** | `DefEnvVol` `$88` + `SetEnvVol` `$89` |
| Timbre sweep (shimmer, charge) | **waveform/shift envelope** | `DefEnvWave` `$8C` + `SetEnvWave` `$8D` |
| Arpeggio / multi-note jingle | several compact notes | `Mode` `$93` + `SetDur` `$94` + note bytes |
| Sustain length | note duration | `SetDur` `$94` / per-note length |
| Loop (continuous SFX) | counted loop, count 0 = forever | `Loop` `$80` + `Do` `$81` |
| Stereo placement | stereo / attenuation | `SetStereo` `$8E`, `SetChnAttenuation` `$90` |
| End one-shot | end of stream | `END` `$00` |

Because this stays inside the existing opcode set, every `sfx_*` stream replays
byte-identically on the shipped player; no format-version bump (§6.4 of the engine
design) is required.

### 3.1 Envelope record format (used by the sweep/decay macros)

The engine's `Def*Env` opcodes point at an envelope record; the SFX macros emit
these records. The layout (as read by `SndSetEnvVol1` / the env-advance code in
`lynx-snd.s`) is:

```
offset  size  field
  0      1     loop      ; byte offset to jump back to when the list is exhausted;
                         ;   0 = no loop -> hold the final segment (silence/last value)
  1      1     parts     ; number of [count, increment] segments
  2..    2*n   segments  ; parts x [count, increment]
```

For a one-shot effect, `loop = 0` so the envelope runs once and **holds** its last
segment — a volume envelope that ramps to 0 then stays at 0 gives a clean decay with
no retrigger. For a continuous effect, `loop` points back into the segment list so
the shape repeats (tremolo/vibrato).

A pitch sweep is a frequency envelope whose segments add a per-frame increment to
the running reload/prescale (negative increment = pitch rises in Mikey terms /
positive = falls, exact sign tuned in implementation). A "boing" is two segments —
fast up, slower settle.

### 3.2 Noise and timbre

Tonal effects use a feedback value that produces a long LFSR period (musical pitch);
noise effects (explosions, wind, hits, splashes) use the wide-band feedback taps so
the channel emits white noise. `SetInstr`'s `feedback` byte plus `SetInteg`'s bit-7
tap select the LFSR; `SetInteg`'s integrate bit softens the timbre (the engine's `I`
/ `X` round-trip, §6.4 of the engine design). The catalogue's "noise" column maps to
a small set of named feedback presets defined once in `sfx.inc`.

---

## 4. The ca65 authoring layer (`asminc/lynx/sfx.inc`)

Effects are **hand-authored and hand-tuned** as small ca65 sources, one per effect,
using a macro vocabulary that hides the raw opcode bytes. This keeps each effect
file short and legible for a sound designer, needs no new host tool, and emits the
exact engine bytes (§3). The macros:

| Macro | Expands to | Purpose |
|-------|-----------|---------|
| `SFX_BEGIN name` | export label `sfx_<name>_data:` + segment | start an effect |
| `SFX_END` | `.byte $00` (END) | finish a one-shot effect |
| `SFX_LOOP_FOREVER` / `SFX_LOOP_END` | `Loop 0` … `Do` | wrap a continuous effect |
| `SFX_WAVE kind` | `SetInstr` + `SetInteg` preset | pick `SQ`, `TRI`, `NOISE`, `METAL` timbre |
| `SFX_VOL v` | `SetInstr` volume field | base loudness (0..127) |
| `SFX_DUR frames` | `Mode 1` + `SetDur frames` | enable compact notes, set default length |
| `SFX_SWEEP up\|down, rate, len` | `DefEnvFrq`+`SetEnvFrq` record | pitch slide |
| `SFX_DECAY rate, len` | `DefEnvVol`+`SetEnvVol`, loop=0 | fade to silence |
| `SFX_SWELL rate, len` | `DefEnvVol`+`SetEnvVol`, rising | fade in / charge |
| `SFX_SHIMMER rate, len` | `DefEnvWave`+`SetEnvWave` | timbre wobble (magic) |
| `SFX_NOTE pitch` | one compact note byte | a single pitch (chromatic index) |
| `SFX_ARP p0,p1,...` | several compact notes | a quick jingle/arpeggio |
| `SFX_REST frames` | `Pause` `$82` | gap between notes |

A whole effect then reads declaratively, e.g. a coin pickup (bright ascending
two-note bling):

```asm
        .include "lynx/sfx.inc"

        SFX_BEGIN coin
        SFX_WAVE  SQ
        SFX_VOL   90
        SFX_DUR   3            ; ~3 frames/note at the player IRQ
        SFX_DECAY 6, 8         ; short metallic fade
        SFX_ARP   72, 79       ; two rising notes  (C5, G5-ish)
        SFX_END
```

and a laser "pew" (sharp descending chirp) is:

```asm
        SFX_BEGIN laser
        SFX_WAVE  SQ
        SFX_VOL   80
        SFX_SWEEP down, 12, 10 ; fast falling pitch
        SFX_DUR   10
        SFX_NOTE  96           ; start high
        SFX_END
```

These two examples are illustrative; the precise pitch indices, rates and durations
are tuned on emulator/hardware during implementation (§9), since "sounds right" is
the acceptance test, not byte counts.

---

## 5. Where the catalogue values come from

The recipes in §7 follow the user-supplied retro design guidelines:

- **Duration** 50–300 ms for gameplay one-shots (≈ 12–72 player-IRQ frames at the
  engine's ~240 Hz default; `abccc`/SFX durations are in frames).
- **Waveforms** limited to what Mikey gives: square, the integrate ("soft/triangle")
  shape, and LFSR noise. No external samples.
- **Pitch bends** (frequency envelope) make the classic pew / boing / bling.
- **Noise** (wide LFSR taps) drives explosions, wind, splashes, impacts.
- **Arpeggios** (fast multi-note runs) stand in for chords — coins, fanfares,
  secret-found stings.
- **No reverb / minimal sustain** — effects are short and punchy; "echo"/"shimmer"
  hints in the catalogue are faked with a quick decay or a waveform wobble, not a
  delay line (the Lynx has none).

---

## 6. Linkability and library layout

The requirement is that a game pays only for the effects it uses. This is achieved
with **library-member granularity**:

- One effect per source file: `libraries/audio/sfx/coin.s`, `…/jump.s`, etc., each
  emitting exactly one `sfx_<name>_data` symbol.
- All effect objects are archived into the existing **audio** library partition,
  `lib/lynx-audio.lib` (the partition created by the phase-5 library split), as
  separate members.
- ld65 pulls a library member **only if one of its symbols is referenced**. A game
  that calls `sfx_coin(3)` references `sfx_coin_data`, which drags in `coin.o` and
  nothing else. Unused effects never reach the cart. (This is the key difference
  from bundling all effects into one `.o`, which would link the whole set.)

Because the effects live in `lynx-audio.lib`, the cl65 auto-libs manifest (phase 6,
`lib/lynx-sdklibs.list`) already lists the audio library, so no build-flag change is
needed — `#include <lynx/sfx.h>` and call `sfx_*`; the linker resolves the data from
the audio lib automatically.

Build wiring: `libraries/audio/sfx/*.s` are added to the audio library's object list
in `libraries.mk`; the header goes in `include/lynx/sfx.h`; the macros in
`asminc/lynx/sfx.inc`. No example or game `Makefile` needs SFX-specific rules.

### 6.1 Footprint

Each one-shot effect stream is on the order of 10–30 bytes of `RODATA`, so the whole
~55-effect pack is well under 2 KB if every effect were linked — but the point is
that a typical game links a dozen and spends a few hundred bytes. The runtime cost
is zero beyond the `snd` engine the game already links for music.

---

## 7. The effect catalogue

Every entry below maps a named effect to a synthesis recipe expressed in the §4
macros. Columns: **Wave** (`SQ` square, `TRI` integrate/soft, `NOISE` LFSR noise,
`METAL` bright tonal-with-edge); **Pitch / sweep** (chromatic direction); **Dur**
(approx ms, one-shot unless noted); **Shape** (envelope / arpeggio). Exact indices
and rates are tuned in implementation.

### 7.1 Pickups, power, progression

| Effect | Macro | Wave | Pitch / sweep | Dur | Shape |
|--------|-------|------|---------------|-----|-------|
| Coin / Pickup | `sfx_coin` | SQ | up, 2-note | 80 | bright bling, short decay |
| Extra Life | `sfx_extra_life` | SQ | ascending 4–6 notes | 600 | arpeggio ending on sustained high note |
| Power-Up | `sfx_power_up` | SQ | rising sweep / arpeggio | 250 | energetic rise, light decay |
| Power-Down | `sfx_power_down` | TRI | falling, wobbling | 300 | descending sweep, slight vibrato |
| Confirm / Success | `sfx_confirm` | SQ | up, 2-note | 180 | bright ascending chime |
| Level Complete | `sfx_level_complete` | SQ | ascending fanfare | 1000 | multi-note triumphant run |
| Checkpoint | `sfx_checkpoint` | TRI | up, 2-note | 350 | warm chime, lingering decay |
| Secret Found | `sfx_secret_found` | SQ | unusual intervals | 500 | mysterious arpeggio |
| Recharge | `sfx_recharge` | SQ | ascending pulses | 400 | series of rising ticks |
| Charge Up | `sfx_charge_up` | SQ | sustained rise | continuous* | rising sweep + brightening wave env; stop on release |

### 7.2 Movement and platforming

| Effect | Macro | Wave | Pitch / sweep | Dur | Shape |
|--------|-------|------|---------------|-----|-------|
| Jump | `sfx_jump` | SQ | up chirp | 120 | quick rising "boip" |
| Double Jump | `sfx_double_jump` | SQ | up, higher/sharper | 100 | brighter, faster jump |
| Land | `sfx_land` | NOISE | low | 60 | short muted thud |
| Footstep | `sfx_footstep` | NOISE | low tick | 40 | tiny tap, near-instant decay |
| Bounce | `sfx_bounce` | SQ | up bend | 150 | elastic "boing" |
| Bounce Off Wall | `sfx_bounce_wall` | METAL | up bend | 120 | harder "boink", metallic edge |
| Whoosh | `sfx_whoosh` | NOISE | swept band | 150 | fast filtered-noise burst |
| Cursor Move | `sfx_cursor_move` | SQ | flat blip | 30 | very short soft blip |

### 7.3 Combat and damage

| Effect | Macro | Wave | Pitch / sweep | Dur | Shape |
|--------|-------|------|---------------|-----|-------|
| Attack / Swing | `sfx_attack` | NOISE | down | 120 | whoosh + slight pitch fall |
| Laser Shot | `sfx_laser` | SQ | fast down sweep | 150 | sharp "pew" |
| Explosion (Small) | `sfx_explosion_small` | NOISE | low | 250 | noise burst + low pop, quick decay |
| Explosion (Large) | `sfx_explosion_large` | NOISE | very low | 600 | big noise burst, long rumbling decay |
| Hit / Damage | `sfx_hit` | NOISE | mid | 80 | sharp click + noise crack |
| Player Death | `sfx_player_death` | SQ | descending scale | 800 | dramatic falling tone to silence |
| Enemy Death | `sfx_enemy_death` | SQ | down chirp | 180 | quick pop/crackle |
| Magic Cast | `sfx_magic_cast` | TRI | sparkling arpeggio | 500 | shimmering wave env + vibrato |
| Magic Impact | `sfx_magic_impact` | METAL | bright burst | 200 | flash + crystalline pop |
| Shield Activate | `sfx_shield` | TRI | rising shimmer | 500 | shimmer rise to sustained hum |

### 7.4 Spawning, teleport, environment

| Effect | Macro | Wave | Pitch / sweep | Dur | Shape |
|--------|-------|------|---------------|-----|-------|
| Spawn / Appear | `sfx_spawn` | SQ | up shimmer | 250 | rising sweep + wave wobble |
| Teleport | `sfx_teleport` | TRI | up-then-down | 400 | rise/fall sweep + shimmer |
| Fire / Ignite | `sfx_fire` | NOISE | mid band | 300 | soft "foosh" + crackle |
| Wind Gust | `sfx_wind` | NOISE | swept band | 600 | broadband noise, volume swell/fall |
| Thunder | `sfx_thunder` | NOISE | very low | 900 | sharp crack then fading rumble |
| Water Drop | `sfx_water_drop` | TRI | up plink | 120 | rounded attack, brief decay |
| Splash | `sfx_splash` | NOISE | low tail | 300 | noise burst + soft low tail |
| Bubble | `sfx_bubble` | TRI | up bloop | 150 | rounded rising "bloop" |

### 7.5 Objects, doors, items

| Effect | Macro | Wave | Pitch / sweep | Dur | Shape |
|--------|-------|------|---------------|-----|-------|
| Door Open | `sfx_door_open` | NOISE+SQ | up slide | 350 | click + short sliding rise |
| Door Close | `sfx_door_close` | NOISE | low | 250 | solid low thunk |
| Switch / Lever | `sfx_switch` | METAL | flat | 80 | crisp mechanical click |
| Button Click | `sfx_button` | SQ | high flat | 30 | very short high tick |
| Chest Open | `sfx_chest_open` | NOISE+SQ | up after thunk | 500 | wooden clunk + bright sparkle |
| Item Equip | `sfx_item_equip` | METAL | 2-note | 150 | clean metallic confirm |
| Key Pickup | `sfx_key_pickup` | METAL | up jingle | 180 | tiny metallic jingle |
| Unlock | `sfx_unlock` | NOISE+METAL | click+clunk | 300 | lock-release click then clunk |
| Crumble | `sfx_crumble` | NOISE | descending | 400 | crackle series ending in thud |
| Falling Rock | `sfx_falling_rock` | NOISE | low repeats | 600 | multiple low impacts + rattle |

### 7.6 Menus and UI

| Effect | Macro | Wave | Pitch / sweep | Dur | Shape |
|--------|-------|------|---------------|-----|-------|
| Menu Select | `sfx_menu_select` | SQ | up blip | 80 | pleasant confirming chirp |
| Menu Back / Cancel | `sfx_menu_back` | SQ | down blip | 80 | lower-pitched blip |
| Error / Invalid | `sfx_error` | SQ | descending beeps | 250 | 2–3 harsh falling beeps / buzz |
| Typing Beep | `sfx_typing` | SQ | high flat | 25 | tiny square click per char |
| Countdown Beep | `sfx_countdown` | SQ | flat | 100 | single urgent square beep |

### 7.7 Continuous / ambient (looping — stop with `snd_stop_channel`)

| Effect | Macro | Wave | Pitch / sweep | Loop | Shape |
|--------|-------|------|---------------|------|-------|
| Alarm | `sfx_alarm` | SQ | alternating hi/lo | yes | pulsing two-tone siren |
| Ambient Hum | `sfx_ambient_hum` | TRI | low flat | yes | low drone + subtle wave wobble |
| Engine | `sfx_engine` | NOISE+SQ | low buzz | yes | looping buzz; pitch by speed† |
| Heartbeat | `sfx_heartbeat` | TRI | two low thumps | yes | "dum-dum" rhythm |
| Game Over | `sfx_game_over` | SQ | descending minor | once | slow falling minor arpeggio |

\* **Charge Up** loops while held and is stopped (or capped) by the game on release.
† **Engine** pitch tracks speed at runtime: poke `mikey_snd_pitch(ch, …)` on the
engine channel between frames, or re-trigger the effect at the new pitch. Effects
whose parameters change every frame (engine RPM, a continuously aimed charge) are
the one place where dropping to the `mikey_snd_*` layer (§1) is the better tool; the
streamed `sfx_*` form gives the baseline loop.

That is the full ~55-effect starter pack covering pickups, movement, combat,
environment, objects, UI, and ambience.

---

## 8. Worked examples

### 8.1 A jump on the SFX channel

```c
#include <lynx/lynx.h>           /* pulls in sfx.h */

#define SFX_CH 3

void main (void) {
    snd_init();
    for (;;) {
        if (player_pressed_jump())
            sfx_jump(SFX_CH);    /* fire-and-forget; IRQ renders it */
        game_tick();
    }
}
```

### 8.2 Music on 0–2, effects on 3

```c
snd_init();
snd_play(0, level_music);        /* abccc-compiled tune (engine design §7) */
...
sfx_coin(3);                     /* ducks only channel 3, music keeps playing */
sfx_explosion_small(3);          /* replaces the coin if it's still ringing */
```

### 8.3 A looping alarm you turn off

```c
sfx_alarm(3);                    /* starts the siren */
...
if (threat_cleared())
    snd_stop_channel(3);         /* stop it */
```

---

## 9. Documentation and work items (project doc-sync rule)

Per `CLAUDE.md`, code and docs change in the same pass. Implementing this design
touches:

- `include/lynx/sfx.h` — the `sfx_<name>_data` externs + `sfx_<name>(ch)` macros;
  `include/lynx/lynx.h` includes it.
- `asminc/lynx/sfx.inc` — the `SFX_*` authoring macros (§4).
- `libraries/audio/sfx/*.s` — one source per effect (§6); added to the audio
  library object list in `libraries.mk` so they archive into `lib/lynx-audio.lib`.
- `doc/sound.html` — a new "Sound effects" section: the `sfx_*` API, the channel
  convention, the one-shot-vs-looping distinction, the catalogue table, and the
  drop-down-a-layer guidance; plus at least one SVG (the three-layer audio stack of
  §1) following `design/DOC_SVG_STYLE_DESIGN.md`.
- `doc/funcref.html` — list the `sfx_*` macros (or reference the catalogue) under
  the audio section.
- `doc/lynx.html` §8.4 — a pointer to the SFX pack.
- This design doc — source of truth.
- `examples/mikey/` — a small example (e.g. `sfxdemo.c`) that fires several effects
  on input, with a GearLynx golden; wired into the examples Makefile and
  `samples.html`.

Acceptance is **audible**, not just build-and-boot: per the project's standing
feedback, each effect must actually be heard (verify Mikey registers on GearLynx and
listen), since "sounds right" is the only real test for an effect.

---

## 10. Quirks and limitations

1. **Shared channels.** SFX and music compete for Mikey's four channels; the library
   ships no mixer/priority engine. The "reserve channel 3" convention (§2.2) is the
   intended pattern.
2. **Frames, not milliseconds.** Durations are in player-IRQ frames; the ms figures
   in §7 assume the engine's ~240 Hz default and shift if `PlayerFreq` changes.
3. **No samples / no reverb.** Effects are pure Mikey synthesis; "echo"/"shimmer"
   are faked with envelopes (§5).
4. **Runtime-variable effects** (engine RPM, continuous charge) are better driven via
   `mikey_snd_*`; the streamed form is only the baseline loop (§7.7).
5. **Linkage depends on per-file objects.** The "pay only for what you use" property
   relies on one effect per `.o` inside `lynx-audio.lib`; collapsing them into one
   object would link the whole pack (§6).
6. **Byte-compatible.** SFX use only existing opcodes, so the player and stream
   format version are unchanged (engine design §6.4 / §9.3).

---

## 11. Quick reference

```
one-shot:    sfx_coin(3);  sfx_jump(3);  sfx_laser(3);  sfx_explosion_small(3);
looping:     sfx_alarm(3);   ... ;  snd_stop_channel(3);
custom:      snd_play(3, sfx_coin_data);     // stream symbol is public
pause all:   snd_pause(); / snd_continue();

author:      libraries/audio/sfx/<name>.s using SFX_* macros (asminc/lynx/sfx.inc)
convention:  music on channels 0-2, effects on channel 3
```
