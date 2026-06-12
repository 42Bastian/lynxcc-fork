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
; Text scaling is a true 8.8 fixed point word per axis, stored straight
; into the text sprite's sx/sy fields - Suzy scales sprites natively in
; 8.8, so fractional text scaling is free (LYNX_TGI_DESIGN.md sec. 2.3).
; The font argument of tgi_settextstyle is ignored: only the bitmap font
; exists. The text direction only affects how the cursor advances; glyphs
; are never rotated (pre-existing behavior).
;
; tgi_gettextwidth = (strlen(s) * 8 * scalew) >> 8, computed on Suzy's
; 16x16 multiply. Safe because all drawing is synchronous, so neither
; the sprite engine nor a competing math operation can be in flight
; (spec ch. 3.1.1; LYNX_CODEGEN_DESIGN.md sec. 2.6); the math-working
; bit is polled before results are read.
;
; Derived from the lynx-160-102-16 TGI driver by Karri Kaksonen, 2004.
;

        .include        "zeropage.inc"
        .include        "lynx.inc"
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

; 8 rows with (one offset-byte plus 20 character bytes plus one fill-byte)
; plus one 0-offset-byte.
; (As an experiment, the fill-byte isn't being generated.
;  It might not be needed to work around a Suzy bug.)

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
; tgi_outtext: Build the glyph strip for s into text_bitmap and draw it as
; one sprite at the cursor position; then advance the cursor by the string
; width (TGI_TEXT_VERTICAL advances upward).

_tgi_outtext:
        sta     STRPTR
        stx     STRPTR+1

        lda     tgi_bgindex
        beq     @L1             ; Background pen 0: opaque background sprite
        lda     #$04            ; Else: normal sprite
@L1:    sta     text_sprite

        lda     tgi_drawindex   ; Pen byte: foreground high nibble, bg low
        asl
        asl
        asl
        asl
        ora     tgi_bgindex
        sta     text_c

        lda     curx            ; Start position
        sta     text_x
        lda     curx+1
        sta     text_x+1
        lda     cury
        sta     text_y
        lda     cury+1
        sta     text_y+1

        ldy     #<-1            ; Calculate string length
@L2:    iny
        lda     (STRPTR),y
        bne     @L2
        cpy     #20
        bmi     @L3
        ldy     #20             ; Draw at most 20 characters
@L3:    sty     STRLEN
        tya
        bne     @L4
        rts                     ; Zero-length string
@L4:    iny                     ; Prepare text_bitmap

; The next instruction is commented because the code won't include a fill-byte.
;        iny
        sty     STROFF

        ldy     #8-1            ; 8 pixel lines per character
        ldx     #$00
        clc
@L5:    lda     STROFF
        sta     text_bitmap,x
        txa
        adc     STROFF
        tax

; This was the fill-byte.
;        lda     #$FF
;        sta     text_bitmap-1,x
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

        lda     #<text_sprite
        ldx     #>text_sprite
        jsr     tgi_draw_sprite

; Advance the text cursor by the width of the string.

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
; is ignored (bitmap font only).

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
; tgi_gettextwidth: (strlen (s) * 8 * scalew) >> 8, on Suzy's multiplier.
; AB * CD -> EFGH (H low); writing MATHA starts the multiply; SPRSYS bit 7
; is the math-working flag. __sprsys keeps sign-math off, so this is the
; unsigned multiply.

_tgi_gettextwidth:
        jsr     _strlen         ; Length in A/X
        sta     ptr1            ; * 8 -> 16 bit
        stx     ptr1+1
        asl     ptr1
        rol     ptr1+1
        asl     ptr1
        rol     ptr1+1
        asl     ptr1
        rol     ptr1+1
        lda     ptr1
        sta     MATHD           ; CD = len * 8 (low byte first)
        lda     ptr1+1
        sta     MATHC
        lda     text_sx
        sta     MATHB           ; AB = width scale (low byte first)
        lda     text_sx+1
        sta     MATHA           ; Starts the multiply
@L1:    lda     SPRSYS
        bmi     @L1             ; Wait until the math unit is done
        lda     MATHG           ; (EFGH >> 8), low
        ldx     MATHF           ; (EFGH >> 8), high
        rts

;-----------------------------------------------------------------------------
; tgi_gettextheight: (8 * scaleh) >> 8 = scaleh >> 5. The argument is
; irrelevant for the single-line bitmap font.

_tgi_gettextheight:
        lda     text_sy+1
        sta     tmp1
        lda     text_sy
        ldy     #5
@L1:    lsr     tmp1
        ror     a
        dey
        bne     @L1
        ldx     tmp1
        rts
