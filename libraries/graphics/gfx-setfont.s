; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Lynx graphics: select the active bitmap font.
;
; void __fastcall__ gfx_setfont (unsigned char font);
;
; font = GFX_FONT_BITMAP (0): the system 8x8 font.
; font = GFX_FONT_COMPACT (1): the transparent 5x5 font (6-px pitch).
; font = GFX_FONT_VARIABLE (2): the proportional caps font (1..5 px + gap).
;
; Sets the builder dispatched by gfx_outtext plus the per-font metrics read
; by gfx_gettextwidth / gfx_gettextheight. The BITMAP and COMPACT branches
; clear gfx_advtab so the width query stays on the fixed-pitch fast path; the
; VARIABLE branch points it at gfx_fontadv to switch on the advance-sum path.
; Referencing this routine links all three builders and fonts; programs that
; only want the 8x8 font simply never call it and keep the default. See
; design/LYNX_GFX_FONT5X5_DESIGN.md sec. 6 and design/LYNX_GFX_FONTVAR_DESIGN.md
; sec. 6.
;

        .import         build8x8
        .import         build5x5
        .import         buildvar
        .import         gfx_buildptr
        .import         gfx_pitch
        .import         gfx_fontheight
        .import         gfx_advtab
        .import         gfx_fontadv

        .export         _gfx_setfont

.code

_gfx_setfont:
        tax                     ; font id; 0 = GFX_FONT_BITMAP
        beq     @sys
        dex
        beq     @compact        ; 1 = GFX_FONT_COMPACT

        lda     #<buildvar      ; GFX_FONT_VARIABLE (anything >= 2)
        sta     gfx_buildptr
        lda     #>buildvar
        sta     gfx_buildptr+1
        lda     #<gfx_fontadv   ; enable the proportional width path
        sta     gfx_advtab
        lda     #>gfx_fontadv
        sta     gfx_advtab+1
        lda     #6              ; fallback metric; unused while advtab is set
        sta     gfx_pitch
        lda     #5
        sta     gfx_fontheight
        rts

@compact:
        lda     #<build5x5      ; GFX_FONT_COMPACT
        sta     gfx_buildptr
        lda     #>build5x5
        sta     gfx_buildptr+1
        stz     gfx_advtab      ; back to the fixed-pitch width path
        stz     gfx_advtab+1
        lda     #6
        sta     gfx_pitch
        lda     #5
        sta     gfx_fontheight
        rts

@sys:
        lda     #<build8x8
        sta     gfx_buildptr
        lda     #>build8x8
        sta     gfx_buildptr+1
        stz     gfx_advtab      ; back to the fixed-pitch width path
        stz     gfx_advtab+1
        lda     #8
        sta     gfx_pitch
        lda     #8
        sta     gfx_fontheight
        rts
