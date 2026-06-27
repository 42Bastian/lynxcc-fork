; SPDX-License-Identifier: MIT
;
; abcrom template ROM: reserved tune regions.
;
; Each region is fronted by the "ABCR" magic header (design sec. 9.3) so abcrom
; can locate and patch it by magic + channel index, and is .export'ed so its
; runtime address appears in the linker map -- abcrom reads that address to pass
; --org to abccc, resolving envelope pointers into the patched-in region.
;
; Default payload is a single END byte ($00); the rest is reserved capacity.

        .export _abcr_region0, _abcr_region1, _abcr_region2, _abcr_region3

CAP     = 512                    ; payload capacity per channel region
FMTVER  = 1                      ; stream format version (must match abccc)

        .segment "RODATA"

.macro  REGION  chan
        .byte   'A','B','C','R'         ; magic
        .byte   FMTVER                  ; format version
        .byte   chan                    ; channel index
        .word   CAP                     ; capacity (LE)
        .word   1                       ; used (LE) -- 1 = single END byte
        .res    CAP, $00                ; payload: END + reserved space
.endmacro

_abcr_region0:  REGION 0
_abcr_region1:  REGION 1
_abcr_region2:  REGION 2
_abcr_region3:  REGION 3
