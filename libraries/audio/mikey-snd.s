; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Lynx sound engine (snd): direct Mikey channel-register helpers.
;
; These poke a single register of one Mikey audio channel and are independent
; of the compiled-stream engine in lynx-snd.s -- useful for sound effects or
; hand-built instruments.  Each Mikey channel occupies 8 bytes, so the per-
; channel register address is base + (chan << 3).  See
; design/LYNX_SND_ENGINE_DESIGN.md sec. 1.2.
;

        .include "lynx/lynx.inc"

        .export         _mikey_snd_octave
        .export         _mikey_snd_pitch
        .export         _mikey_snd_taps
        .export         _mikey_snd_integrate
        .export         _mikey_snd_volume
        .import         popa
        .importzp       tmp1
;----------------------------------------------------------------------------

        .code

;----------------------------------------------------------------------------
; void __fastcall__ mikey_snd_volume (unsigned char chan, unsigned char val);
;
; Write the signed output volume ($FD20 + 8c).  val in A, chan on the C stack.
;
_mikey_snd_volume:
        jsr     get_channel     ; A = chan
        sta     AUD0VOL,x       ; $FD20 + 8c
        rts

;----------------------------------------------------------------------------
; void __fastcall__ mikey_snd_pitch (unsigned char chan, unsigned char val);
;
; Write the timer reload / pitch within the octave (BACKUP, $FD24 + 8c).
;
_mikey_snd_pitch:
        jsr     get_channel
        sta     AUD0BKUP,x      ; $FD24 + 8c
        rts

;----------------------------------------------------------------------------
; void __fastcall__ mikey_snd_octave (unsigned char chan, unsigned char val);
;
; Set the clock-divider band (CONTROL bits 0-2, $FD25 + 8c) AND enable the
; channel's audio timer (reload+count, bits 3-4 = $18) so the poly counter is
; actually clocked -- a channel with the enable bits clear produces no waveform
; no matter what volume/pitch/taps are set.  The same $18 the stream engine ORs
; into CONTROL (see SndSetValues).  Integrate (bit 5) and the 9th tap (bit 7)
; are preserved.  A channel still needs a non-zero feedback tap (mikey_snd_taps)
; to oscillate; with no taps the output is a DC level, i.e. silence.
;
_mikey_snd_octave:
        jsr     get_channel
        and     #$07            ; octave band -> bits 0-2
        sta     tmp1
        lda     AUD0CTLA,x      ; $FD25 + 8c
        and     #$F8            ; keep bits 3-7
        ora     #$18            ; enable reload+count so the channel runs
        ora     tmp1
        sta     AUD0CTLA,x
        rts

;----------------------------------------------------------------------------
; void __fastcall__ mikey_snd_integrate (unsigned char chan, unsigned char val);
;
; Set or clear the integrate bit (CONTROL bit 5, $FD25 + 8c).  val 0 = square,
; non-zero = integrated waveform.  Other control bits are preserved.
;
_mikey_snd_integrate:
        lsr     a               ; val bit0 -> carry
        php
        jsr     get_channel

	plp
        lda     AUD0CTLA,x      ; $FD25 + 8c
        and     #$DF            ; clear bit 5
        bcc     @set
        ora     #$20            ; integrate on
@set:   sta     AUD0CTLA,x
        rts

;----------------------------------------------------------------------------
; void __fastcall__ mikey_snd_taps (unsigned char chan, unsigned int val);
;
; Select the LFSR feedback taps (timbre / noise colour).  The low 8 taps go to
; FEEDBACK ($FD21 + 8c); the 9th tap is CONTROL bit 7 ($FD25 + 8c).  val (0..511)
; arrives with the low byte in A and the high byte in X.
;
_mikey_snd_taps:
        sta     tmp1            ; low 8 taps
        txa                     ; high byte (bit 0 = 9th tap)
        lsr     a               ; tap 8 -> carry
        php                     ; remember 9th tap
        jsr     get_channel     ; X = chan << 3 (A = val, overwritten below)
        lda     tmp1
        sta     AUD0FEED,x      ; $FD21 + 8c : taps 0-7
        lda     AUD0CTLA,x      ; $FD25 + 8c
        and     #$7F            ; clear bit 7 (9th tap)
        plp
        bcc     @set
        ora     #$80            ; 9th tap on
@set:   sta     AUD0CTLA,x
        rts

;----------------------------------------------------------------------------
; get_channel: shared prologue for the per-channel helpers.
;
; In:   A   = value to preserve for the caller
;       C stack top = channel number
; Out:  X   = chan << 3 (per-channel register offset)
;       A   = the value passed in (unchanged)
;       Y   = clobbered
;
; The incoming value is stashed in Y across popa/shift, which is only safe
; because this SDK is hardwired to the 65SC02, where popa is `lda (sp)` and
; leaves Y untouched.  On a plain 6502 popa does `ldy #0` and would destroy
; the stashed value.
;
get_channel:
        tay                     ; stash val (65SC02 popa preserves Y)
        jsr     popa            ; A = chan
        asl     a
        asl     a
        asl     a               ; chan << 3
        tax
        tya                     ; restore val
        rts
