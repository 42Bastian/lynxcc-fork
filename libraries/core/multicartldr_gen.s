;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; multicartldr_gen.s -- GENERATED, DO NOT EDIT.
;
; The relocatable multicart runtime loader (runtime/lynx/multicartldr.s) linked
; at $0040 by cfg/lynx-multicartldr.cfg and captured byte for byte. multicart_run
; (libraries/core/multicart.s) copies these 187 bytes to $0040 and jumps there
; to load a bundled game ROM off the cartridge.
;
; Regenerate with tools/lnx/gen-multicartldr.sh (top-level: make
; multicart-loader-gen). See design/LYNX_MULTICART_DESIGN.md.

        .export _multicart_loader
        .export multicart_loader_size : absolute = 187

        .segment "RODATA"

_multicart_loader:
        .byte 32,111,0,32,168,0,108,7,0,100,0,32,198,0,10,10
        .byte 10,24,105,128,73,255,170,169,0,105,3,73,255,168,32,157
        .byte 0,162,0,160,8,32,187,0,149,3,232,136,208,247,96,32
        .byte 73,0,165,7,5,8,208,10,165,11,133,7,165,12,133,8
        .byte 128,8,165,7,133,11,165,8,133,12,165,3,133,0,32,198
        .byte 0,166,4,164,5,32,157,0,166,9,164,10,96,232,208,3
        .byte 200,240,87,32,187,0,128,245,232,208,3,200,240,76,32,187
        .byte 0,146,11,230,11,208,241,230,12,128,237,173,178,252,230,1
        .byte 208,56,230,2,208,52,72,218,90,169,27,41,252,168,9,2
        .byte 170,165,0,230,0,56,128,11,144,4,142,139,253,24,232,142
        .byte 135,253,202,142,135,253,42,140,139,253,208,236,169,27,141,139
        .byte 253,100,1,169,248,133,2,122,250,104,96
