; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Lynx static TGI: the 5x5 compact bitmap font.
;
; 70 stored glyphs, 5 bytes each, one byte per pixel row. The 5 ink pixels
; occupy bits 7..3 (left-aligned); bit value 1 = foreground (opposite the 8x8
; font), so the builder ORs glyphs into a zero (= transparent) strip with no
; inversion. See design/LYNX_TGI_FONT5X5_DESIGN.md sec. 3 & 5.
;
; Table order is NOT a plain (ch-32): build5x5 folds lower-case a-z onto A-Z
; and splices the freed slots out, so the 26 duplicate a-z glyphs are not
; stored. Layout:
;   index  0..64 = ASCII  32..96   (space .. backtick)
;   index 65..68 = ASCII 123..126  ( { | } ~ , designed to match)
;   index    69  = ASCII 127       ( DEL, blank )
;
; Source: img_font5x5.bmp (ASCII 32-96, caps only). Referenced only when the
; program selects TGI_FONT_COMPACT, so it links only on demand.

        .export         tgi_font5x5

.rodata

tgi_font5x5:
        .byte $00, $00, $00, $00, $00   ;  32 'space'
        .byte $20, $20, $20, $00, $20   ;  33 '!'
        .byte $50, $50, $00, $00, $00   ;  34 'dquote'
        .byte $50, $F8, $50, $F8, $50   ;  35 '#'
        .byte $20, $F8, $80, $F8, $20   ;  36 '$'
        .byte $C8, $D0, $20, $58, $98   ;  37 '%'
        .byte $40, $A0, $E8, $90, $68   ;  38 '&'
        .byte $20, $20, $00, $00, $00   ;  39 'quote'
        .byte $10, $20, $20, $20, $10   ;  40 '('
        .byte $40, $20, $20, $20, $40   ;  41 ')'
        .byte $00, $50, $20, $50, $00   ;  42 '*'
        .byte $00, $20, $70, $20, $00   ;  43 '+'
        .byte $00, $00, $00, $20, $20   ;  44 ','
        .byte $00, $00, $70, $00, $00   ;  45 '-'
        .byte $00, $00, $00, $00, $40   ;  46 '.'
        .byte $00, $50, $20, $50, $00   ;  47 '/'
        .byte $70, $98, $A8, $C8, $70   ;  48 '0'
        .byte $20, $60, $20, $20, $70   ;  49 '1'
        .byte $F0, $08, $78, $80, $F8   ;  50 '2'
        .byte $F0, $08, $38, $08, $F0   ;  51 '3'
        .byte $88, $88, $78, $08, $08   ;  52 '4'
        .byte $F8, $80, $F0, $08, $F0   ;  53 '5'
        .byte $78, $80, $F0, $88, $70   ;  54 '6'
        .byte $F8, $08, $10, $20, $40   ;  55 '7'
        .byte $70, $88, $70, $88, $70   ;  56 '8'
        .byte $70, $88, $F8, $08, $F0   ;  57 '9'
        .byte $00, $20, $00, $20, $00   ;  58 ':'
        .byte $00, $20, $00, $20, $20   ;  59 ';'
        .byte $20, $40, $F8, $40, $20   ;  60 '<'
        .byte $00, $70, $00, $70, $00   ;  61 '='
        .byte $20, $10, $F8, $10, $20   ;  62 '>'
        .byte $70, $88, $30, $00, $20   ;  63 '?'
        .byte $70, $B8, $B8, $80, $78   ;  64 '@'
        .byte $20, $50, $88, $F8, $88   ;  65 'A'
        .byte $F0, $88, $F0, $88, $F0   ;  66 'B'
        .byte $78, $80, $80, $80, $78   ;  67 'C'
        .byte $F0, $88, $88, $88, $F0   ;  68 'D'
        .byte $F8, $80, $F0, $80, $F8   ;  69 'E'
        .byte $F8, $80, $F0, $80, $80   ;  70 'F'
        .byte $78, $80, $80, $88, $78   ;  71 'G'
        .byte $88, $88, $F8, $88, $88   ;  72 'H'
        .byte $F8, $20, $20, $20, $F8   ;  73 'I'
        .byte $F8, $08, $08, $88, $70   ;  74 'J'
        .byte $88, $90, $E0, $90, $88   ;  75 'K'
        .byte $80, $80, $80, $80, $F8   ;  76 'L'
        .byte $88, $D8, $A8, $88, $88   ;  77 'M'
        .byte $88, $C8, $A8, $98, $88   ;  78 'N'
        .byte $70, $88, $88, $88, $70   ;  79 'O'
        .byte $F0, $88, $F0, $80, $80   ;  80 'P'
        .byte $70, $88, $88, $98, $78   ;  81 'Q'
        .byte $F0, $88, $F8, $90, $88   ;  82 'R'
        .byte $78, $80, $F8, $08, $F0   ;  83 'S'
        .byte $F8, $20, $20, $20, $20   ;  84 'T'
        .byte $88, $88, $88, $88, $70   ;  85 'U'
        .byte $88, $88, $88, $50, $20   ;  86 'V'
        .byte $88, $88, $A8, $D8, $88   ;  87 'W'
        .byte $88, $50, $20, $50, $88   ;  88 'X'
        .byte $88, $88, $50, $20, $20   ;  89 'Y'
        .byte $F8, $10, $20, $40, $F8   ;  90 'Z'
        .byte $30, $20, $20, $20, $30   ;  91 '['
        .byte $00, $70, $70, $70, $00   ;  92 'bksl'
        .byte $60, $20, $20, $20, $60   ;  93 ']'
        .byte $20, $20, $A8, $70, $20   ;  94 '^'
        .byte $00, $00, $00, $00, $F8   ;  95 '_'
        .byte $F8, $D8, $88, $D8, $F8   ;  96 'btick'
        .byte $60, $40, $80, $40, $60   ; 123 '{'
        .byte $20, $20, $20, $20, $20   ; 124 '|'
        .byte $C0, $40, $20, $40, $C0   ; 125 '}'
        .byte $00, $48, $90, $00, $00   ; 126 '~'
        .byte $00, $00, $00, $00, $00   ; 127 'DEL'
