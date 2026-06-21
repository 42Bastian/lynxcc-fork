;
; Lynx static TGI: compact 5x5 bitmap text builder.
;
; build5x5 is an alternate body for tgi_outtext, reached through tgi_buildptr
; when the program has called tgi_setfont(TGI_FONT_COMPACT). It shares the
; prologue (string pointer, start position, capped length) and the epilogue
; (draw + cursor advance) with the 8x8 builder in tgi-text.s.
;
; Unlike the 8x8 font, glyphs are packed at a 6-px pitch (5 ink + 1 gap), so
; they are not byte-aligned: each glyph's 5-bit rows are shifted into a
; variable-width literal sprite. The background is transparent and the
; foreground is the current pen:
;
;   sprite type = $04 (normal) -> pixel value 0 maps to pen 0, which a
;   normal sprite does not draw (transparent).
;   pen byte = tgi_drawindex    -> high nibble 0 (pixel value 0 -> pen 0,
;   transparent), low nibble = current pen (pixel value 1 -> foreground).
;
; The font stores foreground as bit value 1 (5 ink bits in bits 7..3), so a
; glyph row ORs straight into a zero-initialised (= transparent) strip with
; no inversion. See design/LYNX_TGI_FONT5X5_DESIGN.md sec. 3-5.
;

        .include        "zeropage.inc"
        .include "lynx/lynx.inc"

        .import         tgi_drawindex
        .import         tgi_font5x5
        .import         text_sprite
        .import         text_c
        .import         text_bitmap
        .import         draw_and_advance

        .export         build5x5

PITCH           = 6

; Zero page aliases (zp slots are global; STRPTR/STRLEN match tgi-text.s).

STRPTR          := ptr3
STRLEN          := tmp4
DST             := ptr1
GP              := ptr2

.bss

b5_w:           .res    1       ; pixel bytes per row (W)
b5_stride:      .res    1       ; 1 + W (and the per-row offset byte value)
b5_shift:       .res    1       ; bit shift for the current glyph
b5_byteidx:     .res    1       ; starting byte within a row
b5_shi:         .res    1       ; shifted glyph row, high byte
b5_slo:         .res    1       ; shifted glyph row, carry into next byte
b5_idx:         .res    1       ; current character index

.code

build5x5:
        lda     #$04            ; normal sprite -> pen 0 transparent
        sta     text_sprite
        lda     tgi_drawindex   ; pen byte = (0 << 4) | drawindex
        sta     text_c

; W = (N * 6 + 7) >> 3 ; N = STRLEN (1..20) -> W in 1..15

        lda     STRLEN
        asl     a               ; N*2
        sta     tmp1
        asl     a               ; N*4
        clc
        adc     tmp1            ; N*6
        adc     #7
        lsr     a
        lsr     a
        lsr     a
        sta     b5_w            ; W
        clc
        adc     #1
        sta     b5_stride       ; 1 + W = per-row stride = offset byte value

; Zero the working area (5 rows * up to 16 + terminator = 81 bytes). The
; strip is shorter for short strings; zeroing a fixed safe span is simplest
; and the buffer is 169 bytes.

        ldx     #0
        lda     #0
@zero:  sta     text_bitmap,x
        inx
        cpx     #81
        bne     @zero

; Write the leading offset byte (W+1) of each of the 5 rows.

        ldx     #0
        ldy     #5
@off:   lda     b5_stride
        sta     text_bitmap,x
        txa
        clc
        adc     b5_stride
        tax
        dey
        bne     @off
        ; terminator at 5*stride is already zero

; Pack each character. idx = 0..N-1.

        lda     #0
        sta     b5_idx

@char:
        ; bit = idx * 6 ; byteidx = bit >> 3 ; shift = bit & 7
        lda     b5_idx
        asl     a               ; idx*2
        sta     tmp1
        asl     a               ; idx*4
        clc
        adc     tmp1            ; idx*6 (<= 114)
        tax                     ; X = bit
        and     #7
        sta     b5_shift
        txa
        lsr     a
        lsr     a
        lsr     a
        sta     b5_byteidx

        ; DST = text_bitmap + 1 + byteidx  (row 0 pixel base + byteidx)
        clc
        lda     #<text_bitmap
        adc     #1
        adc     b5_byteidx
        sta     DST
        lda     #>text_bitmap
        adc     #0
        sta     DST+1

        ; GP = tgi_font5x5 + t * 5, where t is the glyph index after folding
        ; lower-case a-z onto A-Z and splicing the freed a-z slots out of the
        ; table - so those 26 duplicate glyphs are never stored. See the table
        ; layout note in tgi-font5x5.s.
        ldy     b5_idx
        lda     (STRPTR),y
        cmp     #'a'            ; fold a-z -> A-Z (clear bit 5)
        bcc     @nofold
        cmp     #'z'+1
        bcs     @nofold
        and     #$DF
@nofold:
        sec
        sbc     #32             ; A = ch - 32 (preserved across the asl/rol)
        cmp     #65             ; indices >= 65 are only { | } ~ (123-126):
        bcc     @noslice        ;   close the 26-slot gap left by folded a-z
        sbc     #26             ; (carry set) A -= 26  -> 65..68
@noslice:
        sta     GP              ; GP = t
        stz     GP+1
        asl     GP
        rol     GP+1            ; GP = 2t
        asl     GP
        rol     GP+1            ; GP = 4t
        clc
        adc     GP              ; A(=t) + 4t low -> 5t low
        sta     GP
        bcc     @nc
        inc     GP+1
@nc:    clc
        lda     GP
        adc     #<tgi_font5x5
        sta     GP
        lda     GP+1
        adc     #>tgi_font5x5
        sta     GP+1

        ; For each of the 5 rows: shift the glyph row right by b5_shift and
        ; OR the two resulting bytes into the strip; step DST by one stride.
        lda     #5
        sta     tmp2            ; rows remaining
        ldy     #0              ; Y = row index for (GP),y
@row:
        lda     (GP),y          ; src: 5 ink bits in bits 7..3
        sta     b5_shi
        stz     b5_slo
        ldx     b5_shift
        beq     @noshift
@sh:    lsr     b5_shi
        ror     b5_slo
        dex
        bne     @sh
@noshift:
        sty     tmp1            ; save row index
        ldy     #0
        lda     (DST),y
        ora     b5_shi
        sta     (DST),y
        iny
        lda     (DST),y
        ora     b5_slo          ; carry into next byte (0 when none)
        sta     (DST),y

        lda     DST             ; DST += stride (next row)
        clc
        adc     b5_stride
        sta     DST
        bcc     @nd
        inc     DST+1
@nd:    ldy     tmp1            ; restore row index
        iny
        dec     tmp2
        bne     @row

        inc     b5_idx
        lda     b5_idx
        cmp     STRLEN
        beq     @done
        jmp     @char           ; (out of bne range; loop body > 128 bytes)
@done:
        jmp     draw_and_advance
