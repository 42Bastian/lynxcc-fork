;
; Karri Kaksonen, 2011
;
; This header is required for BLL builds.
;
        .import         __MAIN_START__
        .import         __ONCE_SIZE__, __ONCE_PHYS__
        .export         __BLLHDR__: absolute = 1

; ------------------------------------------------------------------------
; BLL header (BLL header)
;
; The length word covers the payload the BLL loader copies (10-byte header plus
; the MAIN image up to the physical end of the reclaimable ONCE one-shot body).
; __ONCE_PHYS__ is ONCE's load address; on the BLL cart ONCE is packed straight
; after DATA, so __ONCE_PHYS__ + __ONCE_SIZE__ is the end of the resident
; image. See runtime/lynx/crt0.s and design/LYNX_STARTUP_RECLAIM_DESIGN.md.

        .segment "BLLHDR"
        .word   $0880
        .dbyt   __MAIN_START__
        .dbyt   __ONCE_PHYS__ + __ONCE_SIZE__ - __MAIN_START__ + 10
        .byte   $42,$53
        .byte   $39,$33
