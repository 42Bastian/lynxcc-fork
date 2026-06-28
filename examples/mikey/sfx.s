;
; SPDX-License-Identifier: MIT
;
; Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
; Provided under the MIT License; copy it into your own projects freely.
; See the LICENSE file in this directory.
;
; Space-Invaders-style sound effects, reproduced as snd-engine event streams.
;
; These six streams are a hand-translation of an old rsound .mac source whose
; macros map directly onto the compiled-stream opcodes implemented in
; libraries/audio/lynx-snd.s (see design/LYNX_SND_ENGINE_DESIGN.md, doc/sound.html):
;
;   rsound macro        engine opcode (byte)        operands
;   ------------        --------------------        --------
;   DEFVOL n,env        $88 DefEnvVol               n, <env, >env
;   DEFFRQ n,env        $8A DefEnvFrq               n, <env, >env
;   SETVOL n            $89 SetEnvVol               n
;   SETFRQ n            $8B SetEnvFrq               n
;   INSTR f,v,mv        $84 SetInstr                shiftLo=0, shiftHi=0, f, v, mv
;   PLAY note,dur       (legacy note)               note, dur          (2 raw bytes)
;   STOP                $83 NoteOff                  -
;   dc.b 0              $00 END                      -
;
; INSTR's three arguments are feedback (LFSR taps -> timbre), volume and
; max-volume; the shifter seed bytes the opcode also carries are left 0.  A
; feedback of $ff opens all the low taps -> broadband noise (explosions); small
; values give narrow, tonal timbres (shots, the UFO drone).
;
; In the original, the envelopes were defined once in a shared DefineENVs block
; and the effects only re-selected them.  Here every stream re-defines the
; envelopes it uses (the opcode just stores a pointer, so it is cheap and
; idempotent) which makes each effect a self-contained stream you can hand to
; snd_play in any order, with no init stream to run first.
;
; Envelope table layout (consumed by SndSetEnvVol1 / SndSetEnvFrq1):
;       .byte loopStart, numParts, (count, increment) ...
; loopStart == 0 makes the envelope one-shot (decays then frees the note);
; loopStart != 0 loops from that pair (a sustained slide / wobble).  Increments
; are signed; negatives are written as 256-n so ca65 accepts the byte.
;

        .export _sfx_shot
        .export _sfx_alien_expl
        .export _sfx_alien_move
        .export _sfx_nothit_expl
        .export _sfx_ship_expl
        .export _sfx_ufo

; ---- opcode equates -------------------------------------------------------
OP_NOTEOFF      = $83
OP_SETINSTR     = $84
OP_LOOP         = $80
OP_DO           = $81
OP_DEFVOL       = $88
OP_SETVOL       = $89
OP_DEFFRQ       = $8A
OP_SETFRQ       = $8B
END             = $00

        .rodata

; ---- envelope tables (from the original DefineENVs block) -----------------
;                     loop parts  (count, inc) ...
explenv1:   .byte  0, 1,  60, 256-2                 ; volume: 60-frame decay
explenv2:   .byte  1, 1,   1, 256-4                 ; freq:   looping pitch drop
Sexplenv1:  .byte  0, 1,  60, 256-1                 ; slower decay (ship)
Sexplenv2:  .byte  1, 1,   1, 256-2                 ; slower pitch drop
shotenv1:   .byte  2, 2,   2, 256-10,  1, 256-8     ; volume: two-stage decay
shotenv2:   .byte  1, 1,   4, 256-15                ; freq:   fast looping drop
gnurbsh1:   .byte  0, 1,  20, 256-10                ; volume: short blip decay
gnurbsh2:   .byte  1, 1,   1, 256-20                ; freq:   fast looping drop

; ---- 1. player shot -------------------------------------------------------
_sfx_shot:
        .byte   OP_DEFVOL, 15, <explenv1, >explenv1
        .byte   OP_DEFFRQ, 15, <explenv2, >explenv2
        .byte   OP_SETFRQ, 15
        .byte   OP_SETVOL, 15
        .byte   OP_SETINSTR, 0, 0, 3, 127, 127      ; INSTR 3,127,127 (max vol)
        .byte   100, 40                             ; PLAY 100,40
        .byte   OP_NOTEOFF
        .byte   END

; ---- 2. alien explosion ---------------------------------------------------
_sfx_alien_expl:
        .byte   OP_DEFVOL, 14, <explenv1, >explenv1
        .byte   OP_DEFFRQ, 14, <explenv2, >explenv2
        .byte   OP_SETVOL, 14
        .byte   OP_SETFRQ, 14
        .byte   OP_SETINSTR, 0, 0, $ff, 127, 127    ; INSTR $ff,127,127 (loud noise)
        .byte   60, 60                              ; PLAY 60,60
        .byte   OP_NOTEOFF
        .byte   END

; ---- 3. alien march step --------------------------------------------------
_sfx_alien_move:
        .byte   OP_DEFVOL, 11, <gnurbsh1, >gnurbsh1
        .byte   OP_DEFFRQ, 11, <gnurbsh2, >gnurbsh2
        .byte   OP_SETFRQ, 11
        .byte   OP_SETVOL, 11
        .byte   OP_SETINSTR, 0, 0, $31, 127, 127    ; INSTR $31,127,127 (was 20: silent)
        .byte   70, 10                              ; PLAY 70,10
        .byte   OP_SETINSTR, 0, 0, $30, 127, 127    ; INSTR $30,127,127 (was 20: silent)
        .byte   70, 10                              ; PLAY 70,10
        .byte   OP_NOTEOFF
        .byte   END

; ---- 4. shot hit nothing (miss explosion) ---------------------------------
_sfx_nothit_expl:
        .byte   OP_DEFVOL, 13, <shotenv1, >shotenv1
        .byte   OP_DEFFRQ, 13, <shotenv2, >shotenv2
        .byte   OP_SETFRQ, 13
        .byte   OP_SETVOL, 13
        .byte   OP_SETINSTR, 0, 0, $05, 127, 127    ; INSTR $5,127,127
        .byte   70, 40                              ; PLAY 70,40
        .byte   OP_NOTEOFF
        .byte   END

; ---- 5. player ship explosion ---------------------------------------------
_sfx_ship_expl:
        .byte   OP_DEFVOL, 12, <Sexplenv1, >Sexplenv1
        .byte   OP_DEFFRQ, 12, <Sexplenv2, >Sexplenv2
        .byte   OP_SETVOL, 12
        .byte   OP_SETFRQ, 12
        .byte   OP_SETINSTR, 0, 0, $ff, 127, 127    ; INSTR $ff,127,127 (loud noise)
        .byte   60, 60                              ; PLAY 60,60
        .byte   OP_NOTEOFF
        .byte   END

; ---- 6. UFO fly-by siren --------------------------------------------------
; Reworked from the original's single droning note.  Instead of one continuous
; high tone (piercing) we make a "beep-boop" two-tone siren: a clean tonal
; voice (feedback 3, no frequency envelope) alternating a higher "wee" note and
; a lower "woo" note a fifth below, looped.  Lower pitches (indices 40 / 33 sit
; an octave or so below the old index 50) and a softer volume (70) take the edge
; off, and 8 cycles x (30+30) frames = 480 frames is about half the old length.
_sfx_ufo:
        .byte   OP_SETINSTR, 0, 0, 3, 70, 70        ; tonal voice, moderate volume
        .byte   OP_LOOP, 8                           ; 8 wee-woo cycles
        .byte   40, 30                               ; "wee" (higher), 30 frames
        .byte   33, 30                               ; "woo" (lower),  30 frames
        .byte   OP_DO
        .byte   OP_NOTEOFF
        .byte   END
