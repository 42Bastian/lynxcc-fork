;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; asminc/lynx/sfx.inc - authoring macros for the reusable sound-effect library.
;
; These macros hide the raw snd-engine opcode bytes (design/LYNX_SND_ENGINE_DESIGN.md,
; libraries/audio/lynx-snd.s) behind a small sound-designer vocabulary so each
; effect in libraries/audio/sfx/<name>.s reads declaratively.  They emit only
; existing engine opcodes, so every effect replays on the shipped player with no
; stream-format bump (design/LYNX_SFX_LIBRARY_DESIGN.md).
;
; One effect per source file.  A file looks like:
;
;       .include "lynx/sfx.inc"
;
;       SFX_BEGIN coin          ; -> exports _sfx_coin_data
;       SFX_WAVE  SQ            ; square timbre
;       SFX_VOL   90           ; base loudness
;       SFX_DUR   3            ; compact notes, 3 frames each
;       SFX_DECAY 6, 8         ; short volume fade to silence
;       SFX_ARP   72, 79       ; two rising notes
;       SFX_END
;
; The stream is laid out linearly in RODATA; SFX_END terminates it with END
; ($00) and then emits any envelope tables the effect referenced, so those data
; bytes sit after the terminator and are never executed by the player.
;

; ---- engine opcode bytes (mirror design sec. 6.2) -------------------------
SFX_OP_END      = $00
SFX_OP_LOOP     = $80           ; [op][count]   count 0 = 256 reps (very long)
SFX_OP_DO       = $81           ; loop back if count remains
SFX_OP_PAUSE    = $82           ; [op][frames]  silence
SFX_OP_NOTEOFF  = $83
SFX_OP_SETINSTR = $84           ; [op][shiftLo][shiftHi][feedback][vol][maxVol]
SFX_OP_DEFVOL   = $88           ; [op][env#][ptrLo][ptrHi]
SFX_OP_SETVOL   = $89           ; [op][env#]
SFX_OP_DEFFRQ   = $8A
SFX_OP_SETFRQ   = $8B
SFX_OP_DEFWAVE  = $8C
SFX_OP_SETWAVE  = $8D
SFX_OP_STEREO   = $8E           ; [op][MSTEREO]
SFX_OP_CHNATTEN = $90           ; [op][atten]
SFX_OP_MODE     = $93           ; [op][flags]  bit0 = compact notes
SFX_OP_SETDUR   = $94           ; [op][frames]
SFX_OP_SETINTEG = $95           ; [op][value]  bit5 integrate, bit7 upper tap

; ---- fixed per-effect envelope slots --------------------------------------
; Each effect (re)defines its own envelopes at play time, so a small fixed set
; of slot numbers is enough; the engine just stores a pointer per slot.  Two
; effects sounding at once on different channels must not share a slot with
; different shapes - the "effects on channel 3" convention (design sec. 2.2)
; keeps them serialised, so a fixed allocation is safe in practice.
SFX_ENV_VOL     = 1
SFX_ENV_FRQ     = 2
SFX_ENV_WAVE    = 3

; ---- integrate / tap presets (SetInteg value) -----------------------------
SFX_INTEG_NONE  = $00           ; plain square
SFX_INTEG_SOFT  = $20           ; bit5 = integrate (soft/triangle-ish)
SFX_INTEG_TAP9  = $80           ; bit7 = 9th LFSR tap (wider noise)

; ---- feedback (LFSR tap) presets ------------------------------------------
SFX_FB_TONE     = $01           ; pure narrow tone
SFX_FB_SQ       = $03           ; clean square-ish tone
SFX_FB_METAL    = $05           ; bright tonal-with-edge
SFX_FB_NOISE    = $FF           ; broadband noise

; sweep direction tokens
down            = 1
up              = 0

; ---------------------------------------------------------------------------
; Internal state (module-scope .set symbols; reset by SFX_BEGIN).  One effect
; per file, so a single set of file-local labels/vars is sufficient.
; ---------------------------------------------------------------------------

.macro  _SFX_RESET
        _sfx_fb      .set SFX_FB_SQ
        _sfx_integ   .set SFX_INTEG_NONE
        _sfx_vol     .set 100
        _sfx_maxvol  .set 127
        _sfx_dirty   .set 1             ; instrument needs (re)emitting before a note
        _sfx_hasvol  .set 0
        _sfx_volloop .set 0
        _sfx_volcnt  .set 1
        _sfx_volinc  .set 0
        _sfx_hasfrq  .set 0
        _sfx_frqloop .set 0
        _sfx_frqcnt  .set 1
        _sfx_frqinc  .set 0
        _sfx_haswav  .set 0
        _sfx_wavcnt  .set 8
        _sfx_wavfb1  .set SFX_FB_SQ
        _sfx_wavfb2  .set SFX_FB_METAL
.endmacro

; Emit the pending SetInteg + SetInstr right before a note, if the voice changed.
.macro  _SFX_FLUSH
        .if _sfx_dirty
                .byte SFX_OP_SETINTEG, _sfx_integ
                .byte SFX_OP_SETINSTR, $00, $00, _sfx_fb, _sfx_vol, _sfx_maxvol
                _sfx_dirty .set 0
        .endif
.endmacro

; ---------------------------------------------------------------------------
; Public macros
; ---------------------------------------------------------------------------

; Start an effect: export + label _sfx_<name>_data, reset state.
.macro  SFX_BEGIN name
        .rodata
        _SFX_RESET
        .export .ident (.sprintf ("_sfx_%s_data", .string (name)))
        .ident (.sprintf ("_sfx_%s_data", .string (name))):
.endmacro

; Pick the base timbre.  kind = SQ | TRI | NOISE | METAL.
.macro  SFX_WAVE kind
        .if .xmatch ({kind}, SQ)
                _sfx_fb    .set SFX_FB_SQ
                _sfx_integ .set SFX_INTEG_NONE
        .elseif .xmatch ({kind}, TRI)
                _sfx_fb    .set SFX_FB_SQ
                _sfx_integ .set SFX_INTEG_SOFT
        .elseif .xmatch ({kind}, NOISE)
                _sfx_fb    .set SFX_FB_NOISE
                _sfx_integ .set SFX_INTEG_NONE
        .elseif .xmatch ({kind}, METAL)
                _sfx_fb    .set SFX_FB_METAL
                _sfx_integ .set SFX_INTEG_NONE
        .else
                .error "SFX_WAVE: kind must be SQ, TRI, NOISE or METAL"
        .endif
        _sfx_wavfb1 .set _sfx_fb
        _sfx_dirty  .set 1
.endmacro

; Set base loudness (0..127) and the envelope ceiling.
.macro  SFX_VOL v
        _sfx_vol    .set v
        _sfx_maxvol .set v
        _sfx_dirty  .set 1
.endmacro

; Set an explicit feedback (LFSR taps) value for fine timbre control.
.macro  SFX_FEEDBACK f
        _sfx_fb    .set f
        _sfx_dirty .set 1
.endmacro

; Enable compact 1-byte notes and set their default length in frames.
.macro  SFX_DUR frames
        .byte SFX_OP_MODE, $01
        .byte SFX_OP_SETDUR, frames
.endmacro

; Volume envelope: fade from the base volume to silence over `len` frames,
; losing `rate` per frame.  One-shot (holds silence, frees the note).
.macro  SFX_DECAY rate, len
        _sfx_hasvol  .set 1
        _sfx_volloop .set 0
        _sfx_volcnt  .set len
        _sfx_volinc  .set (256 - (rate)) & $ff
        .byte SFX_OP_DEFVOL, SFX_ENV_VOL, <sfx_volenv, >sfx_volenv
        .byte SFX_OP_SETVOL, SFX_ENV_VOL
.endmacro

; Volume envelope: swell up by `rate` per frame over `len` frames, then hold.
.macro  SFX_SWELL rate, len
        _sfx_hasvol  .set 1
        _sfx_volloop .set 0
        _sfx_volcnt  .set len
        _sfx_volinc  .set (rate) & $ff
        .byte SFX_OP_DEFVOL, SFX_ENV_VOL, <sfx_volenv, >sfx_volenv
        .byte SFX_OP_SETVOL, SFX_ENV_VOL
.endmacro

; Frequency envelope: slide the pitch `dir` (up|down) by `rate` per frame over
; `len` frames, then hold.  One-shot.
.macro  SFX_SWEEP dir, rate, len
        _sfx_hasfrq  .set 1
        _sfx_frqloop .set 0
        _sfx_frqcnt  .set len
        .if (dir) = down
                _sfx_frqinc .set (256 - (rate)) & $ff
        .else
                _sfx_frqinc .set (rate) & $ff
        .endif
        .byte SFX_OP_DEFFRQ, SFX_ENV_FRQ, <sfx_frqenv, >sfx_frqenv
        .byte SFX_OP_SETFRQ, SFX_ENV_FRQ
.endmacro

; Waveform (timbre) wobble: alternate the feedback taps every `rate` frames,
; looping, for a shimmering / magical colour.
.macro  SFX_SHIMMER rate, len
        _sfx_haswav  .set 1
        _sfx_wavcnt  .set rate
        _sfx_wavfb1  .set _sfx_fb
        _sfx_wavfb2  .set SFX_FB_METAL
        .byte SFX_OP_DEFWAVE, SFX_ENV_WAVE, <sfx_wavenv, >sfx_wavenv
        .byte SFX_OP_SETWAVE, SFX_ENV_WAVE
.endmacro

; A single compact note (chromatic index 0..127).  Requires SFX_DUR first.
.macro  SFX_NOTE pitch
        _SFX_FLUSH
        .byte pitch
.endmacro

; A quick run of compact notes (arpeggio / jingle).  Requires SFX_DUR first.
.macro  SFX_ARP p0, p1, p2, p3, p4, p5, p6, p7
        _SFX_FLUSH
        .byte p0
        .ifnblank p1
                .byte p1
        .endif
        .ifnblank p2
                .byte p2
        .endif
        .ifnblank p3
                .byte p3
        .endif
        .ifnblank p4
                .byte p4
        .endif
        .ifnblank p5
                .byte p5
        .endif
        .ifnblank p6
                .byte p6
        .endif
        .ifnblank p7
                .byte p7
        .endif
.endmacro

; Silence for `frames` frames (gap between notes).
.macro  SFX_REST frames
        .byte SFX_OP_PAUSE, frames
.endmacro

; Counted / infinite loop wrappers for continuous effects.
.macro  SFX_LOOP count
        .byte SFX_OP_LOOP, count
.endmacro
.macro  SFX_LOOP_FOREVER
        .byte SFX_OP_LOOP, 0
.endmacro
.macro  SFX_LOOP_END
        .byte SFX_OP_DO
.endmacro

; Finish a (one-shot) effect: release the note, END the stream, then emit any
; envelope tables it referenced (safely past the terminator).
.macro  SFX_END
        .byte SFX_OP_NOTEOFF
        .byte SFX_OP_END
        .if _sfx_hasvol
sfx_volenv:     .byte _sfx_volloop, 1, _sfx_volcnt, _sfx_volinc
        .endif
        .if _sfx_hasfrq
sfx_frqenv:     .byte _sfx_frqloop, 1, _sfx_frqcnt, _sfx_frqinc
        .endif
        .if _sfx_haswav
sfx_wavenv:     .byte 1, 2
                .byte _sfx_wavcnt, $00, $00, _sfx_wavfb1
                .byte _sfx_wavcnt, $00, $00, _sfx_wavfb2
        .endif
.endmacro
