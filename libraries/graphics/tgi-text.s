;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx static TGI: bitmap text output.
;
; void __fastcall__ tgi_gotoxy (int x, int y);
; void __fastcall__ tgi_outtext (const char* s);
; void __fastcall__ tgi_outtextxy (int x, int y, const char* s);
; void __fastcall__ tgi_settextstyle (unsigned width, unsigned height,
;                                     unsigned char dir, unsigned char font);
; void __fastcall__ tgi_settextscale (unsigned width, unsigned height);
; void __fastcall__ tgi_settextdir (unsigned char dir);
; unsigned __fastcall__ tgi_gettextwidth (const char* s);
; unsigned __fastcall__ tgi_gettextheight (const char* s);
;
; The string is built as one glyph-strip bitmap and drawn as a single
; sprite. At most 20 characters per call are drawn (pre-existing limit).
;
; Two bitmap fonts are supported. tgi_outtext dispatches through the
; indirect pointer tgi_buildptr to the active builder: build8x8 (the
; default 8x8 system font) or build5x5 (the compact transparent 5x5 font
; in tgi-text5x5.s). tgi_setfont (tgi-setfont.s) selects between them and
; sets tgi_pitch / tgi_fontheight, the per-font metrics read by the width
; and height queries. Programs that never call tgi_setfont(COMPACT) keep
; the default 8x8 path and do not link the compact builder or its font.
; See design/LYNX_TGI_FONT5X5_DESIGN.md.
;
; Text scaling is a true 8.8 fixed point word per axis, stored straight
; into the text sprite's sx/sy fields - Suzy scales sprites natively in
; 8.8, so fractional text scaling is free (design/LYNX_TGI_DESIGN.md sec. 2.3).
; The font argument of tgi_settextstyle is ignored (use tgi_setfont). The
; text direction only affects how the cursor advances; glyphs are never
; rotated (pre-existing behavior).
;
; tgi_gettextwidth = (strlen(s) * tgi_pitch * scalew) >> 8 and
; tgi_gettextheight = (tgi_fontheight * scaleh) >> 8, computed on Suzy's
; 16x16 multiply. Safe because all drawing is synchronous, so neither the
; sprite engine nor a competing math operation can be in flight (spec ch.
; 3.1.1; design/LYNX_CODEGEN_DESIGN.md sec. 2.6); the math-working bit is polled
; before results are read.
;
; Derived from the lynx-160-102-16 TGI driver by Karri Kaksonen, 2004.
;

        .include        "zeropage.inc"
        .include "lynx/lynx.inc"
        .include        "tgi-kernel.inc"

        .import         tgi_draw_sprite
        .import         tgi_drawindex
        .import         tgi_bgindex
        .import         tgi_font
        .import         _strlen
        .import         popa, popax, negax

        .export         _tgi_gotoxy
        .export         _tgi_outtext
        .export         _tgi_outtextxy
        .export         _tgi_settextstyle
        .export         _tgi_settextscale
        .export         _tgi_settextdir
        .export         _tgi_gettextwidth
        .export         _tgi_gettextheight

; Shared with the compact builder (tgi-text5x5.s) and the selector
; (tgi-setfont.s).

        .export         build8x8
        .export         draw_and_advance
        .export         text_sprite
        .export         text_c
        .export         text_bitmap
        .export         tgi_buildptr
        .export         tgi_pitch
        .export         tgi_fontheight

        .macpack        generic

; Zero page aliases

STRPTR          := ptr3
FONTOFF         := ptr4
STROFF          := tmp3
STRLEN          := tmp4

.bss

; Text cursor. curx and cury must stay adjacent (indexed addressing in
; the cursor-advance code).

curx:           .res    2
cury:           .res    2

textdir:        .res    1       ; TGI_TEXT_HORIZONTAL is 0

; 8 rows of (one offset-byte plus up to 20 character bytes plus one
; trailing pad byte) plus one 0-offset-byte terminator. Also large enough
; for the compact font's strip (5 rows of up to 1+15 bytes + terminator =
; 81 bytes).
;
; build8x8 emits a per-row pad byte. Suzy drops the final source pixel of
; any literal scan line (confirmed on GearLynx and real hardware), so an
; 8x8 glyph - exactly one byte / 8 px per row - would lose the rightmost
; column of the last character. The pad pushes that dropped pixel past the
; real glyph data. The pad must resolve to pen 0, which a normal sprite
; leaves transparent. This 1bpp font is active-low: build8x8 puts the draw
; pen in the HIGH nibble of text_c, so pixel value 0 is the (opaque) ink
; and pixel value 1 is pen 0. The transparent pad byte is therefore $FF
; (all value-1 pixels), NOT $00 - a $00 pad would paint a solid box after
; the text. The compact 5x5 builder needs no pad: its rows always end on
; the inter-glyph gap or byte padding, both transparent (see
; tgi-text5x5.s). See design/LYNX_SPRITE_PADBYTE_DESIGN.md.

text_bitmap:    .res    8*(1+20+1)+1

.data

text_coll:
        .byte   0
text_sprite:
        .byte   $00, $90, $20
        .addr   0, text_bitmap
text_x:
        .word   0
text_y:
        .word   0
text_sx:
        .word   $100            ; Width scale, 8.8 (set by tgi_settextscale)
text_sy:
        .word   $100            ; Height scale, 8.8
text_c:
        .byte   0

; Active text builder and its metrics. Default = system 8x8 font; only the
; 8x8 path links until tgi_setfont selects the compact font.

tgi_buildptr:   .addr   build8x8
tgi_pitch:      .byte   8       ; cursor advance per character (px)
tgi_fontheight: .byte   8       ; glyph rows, for tgi_gettextheight

.code

;-----------------------------------------------------------------------------
; tgi_gotoxy: Position the text cursor for tgi_outtext.

_tgi_gotoxy:
        sta     cury            ; Y
        stx     cury+1
        jsr     popax
        sta     curx            ; X
        stx     curx+1
        rts

;-----------------------------------------------------------------------------
; tgi_outtextxy: Position the cursor, then output.

_tgi_outtextxy:
        pha                     ; Save s
        txa
        pha
        jsr     popax           ; Y
        sta     cury
        stx     cury+1
        jsr     popax           ; X
        sta     curx
        stx     curx+1
        pla                     ; Restore s
        tax
        pla
        ; Run into _tgi_outtext

;-----------------------------------------------------------------------------
; tgi_outtext: Shared prologue - latch the string pointer and the start
; position, measure the string (capped at the 20-char draw limit), then
; dispatch to the active builder. Each builder fills text_bitmap, then runs
; into draw_and_advance.

_tgi_outtext:
        sta     STRPTR
        stx     STRPTR+1

        lda     curx            ; Start position
        sta     text_x
        lda     curx+1
        sta     text_x+1
        lda     cury
        sta     text_y
        lda     cury+1
        sta     text_y+1

        ldy     #<-1            ; Calculate string length (capped at the
@L2:    iny                     ; 20-char draw limit, so a long string is
        cpy     #20             ; not scanned past what will be drawn)
        beq     @L3
        lda     (STRPTR),y
        bne     @L2
@L3:    sty     STRLEN
        tya
        bne     @L4
        rts                     ; Zero-length string
@L4:    jmp     (tgi_buildptr)

;-----------------------------------------------------------------------------
; build8x8: Build the glyph strip for the 8x8 system font. Each glyph is one
; byte-aligned column of an 8-px-wide literal sprite (8 bytes per glyph).
; (Logic unchanged from the original single-font builder.)

build8x8:
        lda     #$04            ; TYPE_NORMAL: pen 0 is transparent. The font
        sta     text_sprite     ; is active-low (ink = pixel value 0).

        lda     tgi_drawindex   ; Pen byte: draw pen (ink, pixel value 0) in
        asl                     ; the high nibble, bgindex (pixel value 1) in
        asl                     ; the low nibble. The default bgindex 0 -> pen 0
        asl                     ; -> transparent background; a non-zero
        asl                     ; tgi_setbgcolor gives an opaque coloured box.
        ora     tgi_bgindex
        sta     text_c

        ldy     STRLEN          ; offset byte = 1 + len + 1 pad byte
        iny
        iny
        sty     STROFF

        ldy     #8-1            ; 8 pixel lines per character
        ldx     #$00
        clc
@L5:    lda     STROFF
        sta     text_bitmap,x   ; row offset byte
        txa
        adc     STROFF
        tax                     ; X = start of the next row
        lda     #$FF            ; trailing pad byte: value-1 px -> pen 0
        sta     text_bitmap-1,x ; (transparent; the engine drops this, not
                                ;  the last real glyph column)
        dey
        bpl     @L5
        stz     text_bitmap,x

        stz     tmp2
        iny                     ;(ldy #$00)
@L6:    lda     (STRPTR),y
        sty     tmp1

        sub     #' '            ; (ch - ' ') * 8
        stz     FONTOFF+1
        asl
        asl
        rol     FONTOFF+1
        asl
        rol     FONTOFF+1
        ;clc                    ; (cleared by rol)
        adc     #<tgi_font
        sta     FONTOFF
        lda     FONTOFF+1
        adc     #>tgi_font
        sta     FONTOFF+1

; And now, copy the 8 bytes of that glyph.

        ldx     tmp2
        inx
        stx     tmp2

; Draw char. from top to bottom, reading char-data from offset 8-1 to offset 0.
        ldy     #8-1
@L7:    lda     (FONTOFF),y     ; *chptr
        sta     text_bitmap,x   ; textbuf[y*(1+len+1)+1+x]

        txa
        adc     STROFF
        tax

        dey
        bpl     @L7

        ; Goto next char.
        ldy     tmp1
        iny
        dec     STRLEN
        bne     @L6

        ; Run into draw_and_advance

;-----------------------------------------------------------------------------
; draw_and_advance: Shared epilogue. Draw the strip sprite at the cursor,
; then advance the cursor by the string width (TGI_TEXT_VERTICAL advances
; upward).

draw_and_advance:
        lda     #<text_sprite
        ldx     #>text_sprite
        jsr     tgi_draw_sprite

        lda     STRPTR
        ldx     STRPTR+1
        jsr     _tgi_gettextwidth
        ldy     #0
        cpy     textdir         ; Horizontal text?
        beq     @L8             ; Jump if yes
        jsr     negax           ; Vertical: move the cursor up
        ldy     #2              ; ... and address cury
@L8:    clc
        adc     curx,y
        sta     curx,y
        txa
        adc     curx+1,y
        sta     curx+1,y
        rts

;-----------------------------------------------------------------------------
; tgi_settextstyle / tgi_settextscale / tgi_settextdir
;
; The scaling factors are 8.8 fixed point; $100 = 1.0. The font argument
; is ignored (use tgi_setfont to select a font).

_tgi_settextstyle:
        jsr     popa            ; Direction (font in A is ignored)
        sta     textdir
        jsr     popax           ; Height scale; run into tgi_settextscale

_tgi_settextscale:
        sta     text_sy         ; Height (in A/X on entry)
        stx     text_sy+1
        jsr     popax           ; Width
        sta     text_sx
        stx     text_sx+1
        rts

_tgi_settextdir:
        sta     textdir
        rts

;-----------------------------------------------------------------------------
; tgi_gettextwidth: (strlen (s) * tgi_pitch * scalew) >> 8, on Suzy's
; multiplier. The product AB * CD -> EFGH (H low); writing MATHA starts the
; multiply; SPRSYS bit 7 is the math-working flag. __sprsys keeps sign-math
; off, so these are unsigned multiplies. The pitch is a per-font byte so the
; query needs no knowledge of which font is active.

_tgi_gettextwidth:
        jsr     _strlen         ; Length in A/X
        sta     MATHD           ; CD = len (low byte first)
        stx     MATHC
        lda     tgi_pitch
        sta     MATHB           ; AB = pitch
        stz     MATHA           ; (pitch high = 0) starts the multiply
@L1:    lda     SPRSYS
        bmi     @L1             ; Wait until the math unit is done
        lda     MATHH           ; len * pitch, low  (full product, not >>8)
        sta     MATHD           ; -> CD for the scale multiply
        lda     MATHG
        sta     MATHC
        lda     text_sx
        sta     MATHB           ; AB = width scale (low byte first)
        lda     text_sx+1
        sta     MATHA           ; Starts the multiply
@L2:    lda     SPRSYS
        bmi     @L2
        lda     MATHG           ; (EFGH >> 8), low
        ldx     MATHF           ; (EFGH >> 8), high
        rts

;-----------------------------------------------------------------------------
; tgi_gettextheight: (tgi_fontheight * scaleh) >> 8. The argument is
; irrelevant for the single-line bitmap font.

_tgi_gettextheight:
        lda     tgi_fontheight
        sta     MATHD           ; CD = height
        stz     MATHC
        lda     text_sy
        sta     MATHB           ; AB = height scale (low byte first)
        lda     text_sy+1
        sta     MATHA           ; Starts the multiply
@L1:    lda     SPRSYS
        bmi     @L1
        lda     MATHG           ; (height * scale) >> 8, low
        ldx     MATHF           ; high
        rts
