; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Lynx static TGI: select the active bitmap font.
;
; void __fastcall__ tgi_setfont (unsigned char font);
;
; font = TGI_FONT_BITMAP (0): the system 8x8 font.
; font = TGI_FONT_COMPACT (1): the transparent 5x5 font (6-px pitch).
; font = TGI_FONT_VARIABLE (2): the proportional caps font (1..5 px + gap).
;
; Sets the builder dispatched by tgi_outtext plus the per-font metrics read
; by tgi_gettextwidth / tgi_gettextheight. The BITMAP and COMPACT branches
; clear tgi_advtab so the width query stays on the fixed-pitch fast path; the
; VARIABLE branch points it at tgi_fontadv to switch on the advance-sum path.
; Referencing this routine links all three builders and fonts; programs that
; only want the 8x8 font simply never call it and keep the default. See
; design/LYNX_TGI_FONT5X5_DESIGN.md sec. 6 and design/LYNX_TGI_FONTVAR_DESIGN.md
; sec. 6.
;

        .import         build8x8
        .import         build5x5
        .import         buildvar
        .import         tgi_buildptr
        .import         tgi_pitch
        .import         tgi_fontheight
        .import         tgi_advtab
        .import         tgi_fontadv

        .export         _tgi_setfont

.code

_tgi_setfont:
        tax                     ; font id; 0 = TGI_FONT_BITMAP
        beq     @sys
        dex
        beq     @compact        ; 1 = TGI_FONT_COMPACT

        lda     #<buildvar      ; TGI_FONT_VARIABLE (anything >= 2)
        sta     tgi_buildptr
        lda     #>buildvar
        sta     tgi_buildptr+1
        lda     #<tgi_fontadv   ; enable the proportional width path
        sta     tgi_advtab
        lda     #>tgi_fontadv
        sta     tgi_advtab+1
        lda     #6              ; fallback metric; unused while advtab is set
        sta     tgi_pitch
        lda     #5
        sta     tgi_fontheight
        rts

@compact:
        lda     #<build5x5      ; TGI_FONT_COMPACT
        sta     tgi_buildptr
        lda     #>build5x5
        sta     tgi_buildptr+1
        stz     tgi_advtab      ; back to the fixed-pitch width path
        stz     tgi_advtab+1
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
        stz     tgi_advtab      ; back to the fixed-pitch width path
        stz     tgi_advtab+1
        lda     #8
        sta     tgi_pitch
        lda     #8
        sta     tgi_fontheight
        rts
