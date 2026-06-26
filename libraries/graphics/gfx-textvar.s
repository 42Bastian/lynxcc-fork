; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Lynx graphics: proportional (variable-width) text builder.
;
; buildvar is an alternate body for gfx_outtext, reached through gfx_buildptr
; when the program has called gfx_setfont(GFX_FONT_VARIABLE). It shares the
; prologue (string pointer, start position, capped length) and the epilogue
; (draw + cursor advance) with the 8x8 and 5x5 builders in gfx-text.s.
;
; It is a near-clone of build5x5 (gfx-text5x5.s) and uses the same storage
; convention: a bit-shifted literal 1-bpp sprite, transparent background
; (TYPE_NORMAL, pixel value 0 -> pen 0), foreground in the current pen, glyph
; rows stored fg = bit value 1 and ink left-aligned in the high bits so each
; row ORs straight into a zero-filled strip with no inversion. The two builders
; differ in only one thing: build5x5 packs every glyph at a fixed 6-px pitch
; (bit = idx*6), while buildvar keeps a running bit accumulator stepped by the
; per-glyph advance read from gfx_fontadv (ink width + 1-px gap). This is what
; makes the font proportional - an 'I' eats 2 px, an 'M' eats 6.
;
; Two passes over the (capped) string:
;   pass 1: total = sum of advances -> strip byte width W = (total + 7) >> 3.
;           The summation is the shared str_advance helper in gfx-text.s, which
;           is also what gfx_gettextwidth uses, so the drawn width and the
;           queried width can never disagree.
;   pass 2: pack each glyph at its accumulated bit position.
;
; The folding of lower-case a-z onto A-Z and the splicing-out of the freed
; slots are identical to the 5x5 font (see gfx-fontvar.s and
; design/LYNX_GFX_FONTVAR_DESIGN.md sec. 3-5).
;

        .include        "zeropage.inc"
        .include        "lynx/lynx.inc"

        .import         gfx_drawindex
        .import         gfx_fontvar
        .import         gfx_fontadv
        .import         text_sprite
        .import         text_c
        .import         text_bitmap
        .import         draw_and_advance
        .import         str_advance

        .export         buildvar

; Zero page aliases (zp slots are global; STRPTR/STRLEN match gfx-text.s).

STRPTR          := ptr3
STRLEN          := tmp4
DST             := ptr1
GP              := ptr2

.bss

bv_w:           .res    1       ; pixel bytes per row (W)
bv_stride:      .res    1       ; 1 + W (and the per-row offset byte value)
bv_shift:       .res    1       ; bit shift for the current glyph
bv_byteidx:     .res    1       ; starting byte within a row
bv_shi:         .res    1       ; shifted glyph row, high byte
bv_slo:         .res    1       ; shifted glyph row, carry into next byte
bv_idx:         .res    1       ; current character index
bv_bitpos:      .res    1       ; running pack bit position (sum of advances)
bv_t:           .res    1       ; folded+spliced glyph index of current char

.code

buildvar:
        lda     #$04            ; normal sprite -> pen 0 transparent
        sta     text_sprite
        lda     gfx_drawindex   ; pen byte = (0 << 4) | drawindex
        sta     text_c

; Pass 1: W = (total_advance + 7) >> 3 ; total <= 20*6 = 120, fits a byte.
; str_advance sums gfx_advtab[foldsplice(ch)] over the capped string already
; latched in STRPTR / STRLEN by the shared prologue.

        jsr     str_advance     ; A = total advance (<= 120)
        clc
        adc     #7
        lsr     a
        lsr     a
        lsr     a
        sta     bv_w            ; W (1..15)
        clc
        adc     #1
        sta     bv_stride       ; 1 + W = per-row stride = offset byte value

; Zero the working area (5 rows * up to 16 + terminator = 81 bytes). The strip
; is shorter for short strings; zeroing a fixed safe span is simplest and the
; shared text_bitmap buffer is large enough (see gfx-text.s).

        ldx     #0
        lda     #0
@zero:  sta     text_bitmap,x
        inx
        cpx     #81
        bne     @zero

; Write the leading offset byte (W+1) of each of the 5 rows.

        ldx     #0
        ldy     #5
@off:   lda     bv_stride
        sta     text_bitmap,x
        txa
        clc
        adc     bv_stride
        tax
        dey
        bne     @off
        ; terminator at 5*stride is already zero

; Pack each character. idx = 0..N-1 ; bitpos accumulates the advances.

        stz     bv_bitpos
        lda     #0
        sta     bv_idx

@char:
        ; byteidx = bitpos >> 3 ; shift = bitpos & 7
        lda     bv_bitpos
        and     #7
        sta     bv_shift
        lda     bv_bitpos
        lsr     a
        lsr     a
        lsr     a
        sta     bv_byteidx

        ; DST = text_bitmap + 1 + byteidx  (row 0 pixel base + byteidx)
        clc
        lda     #<text_bitmap
        adc     #1
        adc     bv_byteidx
        sta     DST
        lda     #>text_bitmap
        adc     #0
        sta     DST+1

        ; foldsplice(s[idx]) -> t : fold a-z onto A-Z, then drop the freed
        ; a-z slots so the 26 duplicate glyphs are never stored. Same math as
        ; build5x5 / str_advance.
        ldy     bv_idx
        lda     (STRPTR),y
        cmp     #'a'
        bcc     @nofold
        cmp     #'z'+1
        bcs     @nofold
        and     #$DF
@nofold:
        sec
        sbc     #32
        cmp     #65
        bcc     @noslice
        sbc     #26             ; (carry set) A -= 26 -> 65..68
@noslice:
        sta     bv_t            ; t, for the advance lookup below

        ; GP = gfx_fontvar + t*5  (A still = t across the asl/rol on GP)
        sta     GP
        stz     GP+1
        asl     GP
        rol     GP+1            ; 2t
        asl     GP
        rol     GP+1            ; 4t
        clc
        adc     GP              ; t + 4t low -> 5t low
        sta     GP
        bcc     @nc
        inc     GP+1
@nc:    clc
        lda     GP
        adc     #<gfx_fontvar
        sta     GP
        lda     GP+1
        adc     #>gfx_fontvar
        sta     GP+1

        ; For each of the 5 rows: shift the glyph row right by bv_shift and OR
        ; the two resulting bytes into the strip; step DST by one stride.
        lda     #5
        sta     tmp2            ; rows remaining
        ldy     #0              ; Y = row index for (GP),y
@row:
        lda     (GP),y          ; src: ink bits left-aligned from bit 7
        sta     bv_shi
        stz     bv_slo
        ldx     bv_shift
        beq     @noshift
@sh:    lsr     bv_shi
        ror     bv_slo
        dex
        bne     @sh
@noshift:
        sty     tmp1            ; save row index
        ldy     #0
        lda     (DST),y
        ora     bv_shi
        sta     (DST),y
        iny
        lda     (DST),y
        ora     bv_slo          ; carry into next byte (0 when none)
        sta     (DST),y

        lda     DST             ; DST += stride (next row)
        clc
        adc     bv_stride
        sta     DST
        bcc     @nd
        inc     DST+1
@nd:    ldy     tmp1            ; restore row index
        iny
        dec     tmp2
        bne     @row

        ; bitpos += advance(t)
        ldy     bv_t
        lda     gfx_fontadv,y
        clc
        adc     bv_bitpos
        sta     bv_bitpos

        inc     bv_idx
        lda     bv_idx
        cmp     STRLEN
        beq     @done
        jmp     @char           ; (out of bne range; loop body > 128 bytes)
@done:
        jmp     draw_and_advance
