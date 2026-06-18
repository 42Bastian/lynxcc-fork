;
; Lynx static TGI: select the active bitmap font.
;
; void __fastcall__ tgi_setfont (unsigned char font);
;
; font = TGI_FONT_BITMAP (0): the system 8x8 font.
; font = TGI_FONT_COMPACT (1): the transparent 5x5 font (6-px pitch).
;
; Sets the builder dispatched by tgi_outtext plus the per-font metrics read
; by tgi_gettextwidth / tgi_gettextheight. Referencing this routine links
; both builders and both fonts; programs that only want the 8x8 font simply
; never call it and keep the default. See design/LYNX_TGI_FONT5X5_DESIGN.md sec. 6.
;

        .import         build8x8
        .import         build5x5
        .import         tgi_buildptr
        .import         tgi_pitch
        .import         tgi_fontheight

        .export         _tgi_setfont

.code

_tgi_setfont:
        tax                     ; font id; 0 = TGI_FONT_BITMAP
        beq     @sys

        lda     #<build5x5      ; TGI_FONT_COMPACT
        sta     tgi_buildptr
        lda     #>build5x5
        sta     tgi_buildptr+1
        lda     #6
        sta     tgi_pitch
        lda     #5
        sta     tgi_fontheight
        rts

@sys:
        lda     #<build8x8
        sta     tgi_buildptr
        lda     #>build8x8
        sta     tgi_buildptr+1
        lda     #8
        sta     tgi_pitch
        lda     #8
        sta     tgi_fontheight
        rts
