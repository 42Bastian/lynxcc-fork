; SPDX-License-Identifier: MIT
;
; Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
; Provided under the MIT License; copy it into your own projects freely.
; See the LICENSE file in this directory.

; Build-time sprite data for sprpcktest.c, packed by sprpck (tools/extern/sprpck)
; from two different input formats:
;
;   sprpck -t6 -s4 logo.bmp logo.spr   ; a Windows BMP  -> 4-bpp Lynx sprite
;   sprpck -t2 -i016016 -s4 icon.sps icon.spr  ; an ASCII SPS -> 4-bpp Lynx sprite
;
; sprpck writes the packed Suzy sprite-data block as a raw binary .spr (unlike
; sp65, which can emit a C array directly). We pull each blob in verbatim with
; .incbin and export a label the C side references through an extern array. The
; Makefile assembles this file with --bin-include-dir suzy so the .spr files are
; found next to it. See doc/sprpck.html.

.export _logo_data
.export _icon_data

.rodata

_logo_data:
        .incbin "logo.spr"

_icon_data:
        .incbin "icon.spr"
