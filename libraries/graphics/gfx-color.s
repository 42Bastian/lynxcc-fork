;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx graphics: drawing and text background color.
;
; void __fastcall__ gfx_setcolor (unsigned char color);
; unsigned char gfx_getcolor (void);
; void __fastcall__ gfx_setbgcolor (unsigned char color);
;
; The old range check against the color count is replaced by AND #$0F:
; there are always exactly 16 pens.
;
; gfx_bgindex (text background pen, default 0) lives here rather than in
; gfx-text.s so that calling gfx_setbgcolor never links the font.
;

        .import         gfx_drawindex

        .export         _gfx_setcolor
        .export         _gfx_getcolor
        .export         _gfx_setbgcolor
        .export         gfx_bgindex

.data

gfx_bgindex:    .byte   0       ; Pen for text background

.code

_gfx_setcolor:
        and     #$0F
        sta     gfx_drawindex
        rts

_gfx_getcolor:
        lda     gfx_drawindex
        ldx     #0
        rts

_gfx_setbgcolor:
        and     #$0F
        sta     gfx_bgindex
        rts
