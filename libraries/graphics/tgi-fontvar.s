; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Lynx static TGI: the proportional (variable-width) caps font.
;
; 70 stored glyphs, 5 bytes each, one byte per pixel row. Ink bits are
; LEFT-aligned from bit 7; bit value 1 = foreground (same convention as the
; 5x5 font, opposite the 8x8 font). The companion table tgi_fontadv gives each
; glyph's cursor advance (ink width + 1-px gap).
;
; Table order is NOT a plain (ch-32): the builder folds lower-case a-z onto
; A-Z and splices the freed slots out, so a-z are not stored twice. Layout:
;   index  0..64 = ASCII  32..96   (space .. backtick)
;   index 65..68 = ASCII 123..126  ( { | } ~ )
;   index    69  = ASCII 127       ( DEL, blank )
; The index math (fold + splice) lives in buildvar; see design/LYNX_TGI_FONTVAR_DESIGN.md.
;
; Recovered from img_help.bmp (EggSavier intro screen); glyphs absent from that
; art (K Q W X Z, digits, most punctuation) are designed to match.

        .export         tgi_fontvar
        .export         tgi_fontadv

.rodata

tgi_fontvar:
        .byte $00, $00, $00, $00, $00   ;  32 'space'
        .byte $80, $80, $80, $00, $80   ;  33 '!'
        .byte $A0, $A0, $00, $00, $00   ;  34 'dquote'
        .byte $50, $F8, $50, $F8, $50   ;  35 '#'
        .byte $40, $E0, $C0, $E0, $40   ;  36 '$'
        .byte $90, $20, $40, $90, $00   ;  37 '%'
        .byte $40, $A0, $40, $A0, $60   ;  38 '&'
        .byte $80, $80, $00, $00, $00   ;  39 'quote'
        .byte $40, $80, $80, $80, $40   ;  40 '('
        .byte $80, $40, $40, $40, $80   ;  41 ')'
        .byte $00, $A0, $40, $A0, $00   ;  42 '*'
        .byte $00, $40, $E0, $40, $00   ;  43 '+'
        .byte $00, $00, $00, $00, $80   ;  44 ','
        .byte $00, $00, $E0, $00, $00   ;  45 '-'
        .byte $00, $00, $00, $00, $80   ;  46 '.'
        .byte $20, $20, $40, $80, $80   ;  47 '/'
        .byte $E0, $A0, $A0, $A0, $E0   ;  48 '0'
        .byte $40, $C0, $40, $40, $E0   ;  49 '1'
        .byte $E0, $20, $E0, $80, $E0   ;  50 '2'
        .byte $E0, $20, $E0, $20, $E0   ;  51 '3'
        .byte $A0, $A0, $E0, $20, $20   ;  52 '4'
        .byte $E0, $80, $E0, $20, $E0   ;  53 '5'
        .byte $E0, $80, $E0, $A0, $E0   ;  54 '6'
        .byte $E0, $20, $20, $20, $20   ;  55 '7'
        .byte $E0, $A0, $E0, $A0, $E0   ;  56 '8'
        .byte $E0, $A0, $E0, $20, $E0   ;  57 '9'
        .byte $00, $80, $00, $80, $00   ;  58 ':'
        .byte $00, $80, $00, $80, $80   ;  59 ';'
        .byte $20, $40, $80, $40, $20   ;  60 '<'
        .byte $00, $E0, $00, $E0, $00   ;  61 '='
        .byte $80, $40, $20, $40, $80   ;  62 '>'
        .byte $E0, $20, $40, $00, $40   ;  63 '?'
        .byte $70, $88, $B0, $80, $70   ;  64 '@'
        .byte $40, $A0, $A0, $E0, $A0   ;  65 'A'
        .byte $C0, $A0, $C0, $A0, $C0   ;  66 'B'
        .byte $C0, $80, $80, $80, $C0   ;  67 'C'
        .byte $C0, $A0, $A0, $A0, $C0   ;  68 'D'
        .byte $E0, $80, $C0, $80, $E0   ;  69 'E'
        .byte $C0, $80, $C0, $80, $80   ;  70 'F'
        .byte $E0, $80, $80, $A0, $E0   ;  71 'G'
        .byte $A0, $A0, $E0, $A0, $A0   ;  72 'H'
        .byte $80, $80, $80, $80, $80   ;  73 'I'
        .byte $E0, $20, $20, $A0, $C0   ;  74 'J'
        .byte $A0, $A0, $C0, $A0, $A0   ;  75 'K'
        .byte $80, $80, $80, $80, $C0   ;  76 'L'
        .byte $88, $D8, $A8, $88, $88   ;  77 'M'
        .byte $90, $D0, $B0, $90, $90   ;  78 'N'
        .byte $E0, $A0, $A0, $A0, $E0   ;  79 'O'
        .byte $E0, $A0, $A0, $E0, $80   ;  80 'P'
        .byte $E0, $A0, $A0, $C0, $E0   ;  81 'Q'
        .byte $E0, $A0, $A0, $C0, $A0   ;  82 'R'
        .byte $E0, $80, $E0, $20, $E0   ;  83 'S'
        .byte $E0, $40, $40, $40, $40   ;  84 'T'
        .byte $A0, $A0, $A0, $A0, $E0   ;  85 'U'
        .byte $A0, $A0, $A0, $A0, $40   ;  86 'V'
        .byte $88, $88, $A8, $D8, $88   ;  87 'W'
        .byte $A0, $A0, $40, $A0, $A0   ;  88 'X'
        .byte $A0, $A0, $40, $40, $40   ;  89 'Y'
        .byte $E0, $20, $40, $80, $E0   ;  90 'Z'
        .byte $C0, $80, $80, $80, $C0   ;  91 '['
        .byte $80, $80, $40, $20, $20   ;  92 'bksl'
        .byte $C0, $40, $40, $40, $C0   ;  93 ']'
        .byte $40, $A0, $00, $00, $00   ;  94 '^'
        .byte $00, $00, $00, $00, $E0   ;  95 '_'
        .byte $80, $40, $00, $00, $00   ;  96 'btick'
        .byte $60, $40, $80, $40, $60   ; 123 '{'
        .byte $80, $80, $80, $80, $80   ; 124 '|'
        .byte $C0, $40, $20, $40, $C0   ; 125 '}'
        .byte $00, $50, $A0, $00, $00   ; 126 '~'
        .byte $00, $00, $00, $00, $00   ; 127 'DEL'

; Cursor advance per glyph (ink width + 1), same index order.
tgi_fontadv:
        .byte 4, 2, 4, 6, 4, 5, 4, 2, 3, 3, 4, 4, 2, 4, 2, 4
        .byte 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 4, 4
        .byte 6, 4, 4, 3, 4, 4, 3, 4, 4, 2, 4, 4, 3, 6, 5, 4
        .byte 4, 4, 4, 4, 4, 4, 4, 6, 4, 4, 4, 3, 4, 3, 4, 4
        .byte 3, 4, 2, 4, 5, 2
