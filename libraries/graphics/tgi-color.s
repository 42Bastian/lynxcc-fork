;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx static TGI: drawing and text background color.
;
; void __fastcall__ tgi_setcolor (unsigned char color);
; unsigned char tgi_getcolor (void);
; void __fastcall__ tgi_setbgcolor (unsigned char color);
;
; The old range check against the color count is replaced by AND #$0F:
; there are always exactly 16 pens.
;
; tgi_bgindex (text background pen, default 0) lives here rather than in
; tgi-text.s so that calling tgi_setbgcolor never links the font.
;

        .import         tgi_drawindex

        .export         _tgi_setcolor
        .export         _tgi_getcolor
        .export         _tgi_setbgcolor
        .export         tgi_bgindex

.data

tgi_bgindex:    .byte   0       ; Pen for text background

.code

_tgi_setcolor:
        and     #$0F
        sta     tgi_drawindex
        rts

_tgi_getcolor:
        lda     tgi_drawindex
        ldx     #0
        rts

_tgi_setbgcolor:
        and     #$0F
        sta     tgi_bgindex
        rts
