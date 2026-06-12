;
; Lynx static TGI: palette.
;
; void __fastcall__ tgi_setpalette (const unsigned char* palette);
; const unsigned char* tgi_getpalette (void);
; const unsigned char* tgi_getdefpalette (void);
;
; The palette is 32 bytes: 16 green bytes, then 16 blue/red bytes
; (GCOLMAP hardware layout). GCOLMAP is readable, so tgi_getpalette
; returns the hardware palette itself. The default palette table lives
; in tgi-init.s (which is always linked and loads it).
;

        .include        "lynx.inc"

        .importzp       ptr1
        .import         tgi_defpalette

        .export         _tgi_setpalette
        .export         _tgi_getpalette
        .export         _tgi_getdefpalette

.code

_tgi_setpalette:
        sta     ptr1
        stx     ptr1+1
        ldy     #31
@L1:    lda     (ptr1),y
        sta     GCOLMAP,y       ; $FDA0
        dey
        bpl     @L1
        rts

_tgi_getpalette:
        lda     #<GCOLMAP
        ldx     #>GCOLMAP
        rts

_tgi_getdefpalette:
        lda     #<tgi_defpalette
        ldx     #>tgi_defpalette
        rts
