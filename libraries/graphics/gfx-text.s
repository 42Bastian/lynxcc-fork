;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx graphics: bitmap text output.
;
; void __fastcall__ gfx_gotoxy (int x, int y);
; void __fastcall__ gfx_outtext (const char* s);
; void __fastcall__ gfx_outtextxy (int x, int y, const char* s);
; void __fastcall__ gfx_settextstyle (unsigned width, unsigned height,
;                                     unsigned char dir, unsigned char font);
; void __fastcall__ gfx_settextscale (unsigned width, unsigned height);
; void __fastcall__ gfx_settextdir (unsigned char dir);
; unsigned __fastcall__ gfx_gettextwidth (const char* s);
; unsigned __fastcall__ gfx_gettextheight (const char* s);
;
; The string is built as one glyph-strip bitmap and drawn as a single
; sprite. At most 20 characters per call are drawn (pre-existing limit).
;
; Three bitmap fonts are supported. gfx_outtext dispatches through the
; indirect pointer gfx_buildptr to the active builder: build8x8 (the
; default 8x8 system font), build5x5 (the compact transparent 5x5 font in
; gfx-text5x5.s) or buildvar (the proportional caps font in gfx-textvar.s).
; gfx_setfont (gfx-setfont.s) selects between them and sets gfx_pitch /
; gfx_fontheight, the per-font metrics read by the width and height queries.
; Programs that never call gfx_setfont keep the default 8x8 path and do not
; link the other builders or fonts.
; See design/LYNX_GFX_FONT5X5_DESIGN.md and design/LYNX_GFX_FONTVAR_DESIGN.md.
;
; Text scaling is a true 8.8 fixed point word per axis, stored straight
; into the text sprite's sx/sy fields - Suzy scales sprites natively in
; 8.8, so fractional text scaling is free (design/LYNX_GFX_DESIGN.md sec. 2.3).
; The font argument of gfx_settextstyle is ignored (use gfx_setfont). The
; text direction only affects how the cursor advances; glyphs are never
; rotated (pre-existing behavior).
;
; gfx_gettextwidth = (strlen(s) * gfx_pitch * scalew) >> 8 for the fixed-pitch
; fonts; for the proportional font (gfx_advtab != 0) the strlen*pitch term is
; replaced by the sum of the capped string's per-glyph advances (str_advance).
; gfx_gettextheight = (gfx_fontheight * scaleh) >> 8, computed on Suzy's
; 16x16 multiply. Safe because all drawing is synchronous, so neither the
; sprite engine nor a competing math operation can be in flight (spec ch.
; 3.1.1; design/LYNX_CODEGEN_DESIGN.md sec. 2.6); the math-working bit is polled
; before results are read.
;
; Derived from the lynx-160-102-16 TGI driver by Karri Kaksonen, 2004.
;

        .include        "zeropage.inc"
        .include "lynx/lynx.inc"
        .include        "gfx.inc"

        .import         gfx_draw_sprite
        .import         gfx_drawindex
        .import         gfx_bgindex
        .import         gfx_font
        .import         _strlen
        .import         popa, popax, negax

        .export         _gfx_gotoxy
        .export         _gfx_outtext
        .export         _gfx_outtextxy
        .export         _gfx_settextstyle
        .export         _gfx_settextscale
        .export         _gfx_settextdir
        .export         _gfx_gettextwidth
        .export         _gfx_gettextheight

; Shared with the compact builder (gfx-text5x5.s) and the selector
; (gfx-setfont.s).

        .export         build8x8
        .export         draw_and_advance
        .export         text_sprite
        .export         text_c
        .export         text_bitmap
        .export         gfx_buildptr
        .export         gfx_pitch
        .export         gfx_fontheight
        .export         gfx_advtab
        .export         str_advance

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

textdir:        .res    1       ; GFX_TEXT_HORIZONTAL is 0

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
; gfx-text5x5.s). See design/LYNX_SPRITE_PADBYTE_DESIGN.md.

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
        .word   $100            ; Width scale, 8.8 (set by gfx_settextscale)
text_sy:
        .word   $100            ; Height scale, 8.8
text_c:
        .byte   0

; Active text builder and its metrics. Default = system 8x8 font; only the
; 8x8 path links until gfx_setfont selects the compact font.

gfx_buildptr:   .addr   build8x8
gfx_pitch:      .byte   8       ; cursor advance per character (px)
gfx_fontheight: .byte   8       ; glyph rows, for gfx_gettextheight

; Proportional-width hook. 0 => fixed pitch (8x8, 5x5): gfx_gettextwidth uses
; the strlen*pitch fast path. Non-zero => points at a per-glyph advance table
; (gfx_fontadv), set only by gfx_setfont's variable-font branch, switching the
; width query to the advance-sum path. Default 0 keeps the variable font and
; its builder out of programs that never select it (the width query references
; only this datum, never the font). See design/LYNX_GFX_FONTVAR_DESIGN.md sec. 5.
gfx_advtab:     .addr   0

.code

;-----------------------------------------------------------------------------
; gfx_gotoxy: Position the text cursor for gfx_outtext.

_gfx_gotoxy:
        sta     cury            ; Y
        stx     cury+1
        jsr     popax
        sta     curx            ; X
        stx     curx+1
        rts

;-----------------------------------------------------------------------------
; gfx_outtextxy: Position the cursor, then output.

_gfx_outtextxy:
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
        ; Run into _gfx_outtext

;-----------------------------------------------------------------------------
; gfx_outtext: Shared prologue - latch the string pointer and the start
; position, measure the string (capped at the 20-char draw limit), then
; dispatch to the active builder. Each builder fills text_bitmap, then runs
; into draw_and_advance.

_gfx_outtext:
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
@L4:    jmp     (gfx_buildptr)

;-----------------------------------------------------------------------------
; build8x8: Build the glyph strip for the 8x8 system font. Each glyph is one
; byte-aligned column of an 8-px-wide literal sprite (8 bytes per glyph).
; (Logic unchanged from the original single-font builder.)

build8x8:
        lda     #$04            ; TYPE_NORMAL: pen 0 is transparent. The font
        sta     text_sprite     ; is active-low (ink = pixel value 0).

        lda     gfx_drawindex   ; Pen byte: draw pen (ink, pixel value 0) in
        asl                     ; the high nibble, bgindex (pixel value 1) in
        asl                     ; the low nibble. The default bgindex 0 -> pen 0
        asl                     ; -> transparent background; a non-zero
        asl                     ; gfx_setbgcolor gives an opaque coloured box.
        ora     gfx_bgindex
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
        adc     #<gfx_font
        sta     FONTOFF
        lda     FONTOFF+1
        adc     #>gfx_font
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
; then advance the cursor by the string width (GFX_TEXT_VERTICAL advances
; upward).

draw_and_advance:
        lda     #<text_sprite
        ldx     #>text_sprite
        jsr     gfx_draw_sprite

        lda     STRPTR
        ldx     STRPTR+1
        jsr     _gfx_gettextwidth
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
; gfx_settextstyle / gfx_settextscale / gfx_settextdir
;
; The scaling factors are 8.8 fixed point; $100 = 1.0. The font argument
; is ignored (use gfx_setfont to select a font).

_gfx_settextstyle:
        jsr     popa            ; Direction (font in A is ignored)
        sta     textdir
        jsr     popax           ; Height scale; run into gfx_settextscale

_gfx_settextscale:
        sta     text_sy         ; Height (in A/X on entry)
        stx     text_sy+1
        jsr     popax           ; Width
        sta     text_sx
        stx     text_sx+1
        rts

_gfx_settextdir:
        sta     textdir
        rts

;-----------------------------------------------------------------------------
; gfx_gettextwidth: (strlen (s) * gfx_pitch * scalew) >> 8, on Suzy's
; multiplier. The product AB * CD -> EFGH (H low); writing MATHA starts the
; multiply; SPRSYS bit 7 is the math-working flag. __sprsys keeps sign-math
; off, so these are unsigned multiplies. The pitch is a per-font byte so the
; query needs no knowledge of which font is active.

_gfx_gettextwidth:
        pha                     ; save low(s); X holds high(s) throughout
        lda     gfx_advtab      ; proportional font selected?
        ora     gfx_advtab+1
        bne     @var            ; yes -> sum per-glyph advances
        pla                     ; no: fixed-pitch fast path
        jsr     _strlen         ; Length in A/X
        sta     MATHD           ; CD = len (low byte first)
        stx     MATHC
        lda     gfx_pitch
        sta     MATHB           ; AB = pitch
        stz     MATHA           ; (pitch high = 0) starts the multiply
@L1:    lda     SPRSYS
        bmi     @L1             ; Wait until the math unit is done
        lda     MATHH           ; len * pitch, low  (full product, not >>8)
        sta     MATHD           ; -> CD for the scale multiply
        lda     MATHG
        sta     MATHC
        jmp     @scale          ; CD * text_sx >> 8

        ; Proportional path: latch the string, cap its length at the 20-char
        ; draw limit, then sum the advances. The single-byte total (max 120)
        ; goes straight into CD with a zero high byte.
@var:   pla
        sta     STRPTR
        stx     STRPTR+1
        ldy     #<-1
@cl:    iny
        cpy     #20
        beq     @cd
        lda     (STRPTR),y
        bne     @cl
@cd:    sty     STRLEN
        jsr     str_advance     ; A = total advance
        sta     MATHD           ; CD = total
        stz     MATHC

@scale: lda     text_sx
        sta     MATHB           ; AB = width scale (low byte first)
        lda     text_sx+1
        sta     MATHA           ; Starts the multiply
@L2:    lda     SPRSYS
        bmi     @L2
        lda     MATHG           ; (EFGH >> 8), low
        ldx     MATHF           ; (EFGH >> 8), high
        rts

;-----------------------------------------------------------------------------
; str_advance: sum the per-glyph advances of the capped string for the
; proportional font. On entry STRPTR points at the string and STRLEN holds the
; capped length (<= 20); the caller guarantees gfx_advtab != 0. Returns the
; total advance in A (<= 120, always a single byte). Reads the advance table
; through gfx_advtab, so it never force-links the variable font - the fixed
; fonts simply never reach it. Shared by buildvar (gfx-textvar.s) and the
; proportional branch of gfx_gettextwidth above.

str_advance:
        lda     gfx_advtab      ; ADVPTR (ptr2) = advance table base
        sta     ptr2
        lda     gfx_advtab+1
        sta     ptr2+1
        ldx     #0              ; running total
        ldy     #0              ; char index
@sa:    cpy     STRLEN
        beq     @sd
        lda     (STRPTR),y
        cmp     #'a'            ; foldsplice(ch) -> table index (see buildvar)
        bcc     @nf
        cmp     #'z'+1
        bcs     @nf
        and     #$DF
@nf:    sec
        sbc     #32
        cmp     #65
        bcc     @ns
        sbc     #26
@ns:    sty     tmp1            ; save char index
        tay                     ; Y = glyph index
        lda     (ptr2),y        ; advance for this glyph
        sta     tmp2
        txa
        clc
        adc     tmp2            ; total += advance
        tax
        ldy     tmp1            ; restore char index
        iny
        bne     @sa             ; STRLEN <= 20, so this never wraps
@sd:    txa
        rts

;-----------------------------------------------------------------------------
; gfx_gettextheight: (gfx_fontheight * scaleh) >> 8. The argument is
; irrelevant for the single-line bitmap font.

_gfx_gettextheight:
        lda     gfx_fontheight
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
