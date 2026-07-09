;
; Karri Kaksonen, 2011
;
; A default directory with just the main executable.
;
        .include "lynx/lynx.inc"
        .import         __STARTOFDIRECTORY__
        .import         __MAIN_START__
        .import         __ONCE_SIZE__, __ONCE_PHYS__
        .import         __BANK0BLOCKSIZE__
        .export         __DEFDIR__: absolute = 1


; ------------------------------------------------------------------------
; Lynx directory
        .segment "DIRECTORY"

__DIRECTORY_START__:
off0 = __STARTOFDIRECTORY__ + (__DIRECTORY_END__ - __DIRECTORY_START__)
blocka = off0 / __BANK0BLOCKSIZE__
; Entry 0 - first executable
block0 = off0 / __BANK0BLOCKSIZE__
; Length of the single MAIN block the loader copies verbatim to __MAIN_START__.
; This spans from the start of MAIN to the physical end of the ONCE one-shot
; body (__ONCE_PHYS__ is ONCE's load address, exported by the cfg).  For the
; plain carts ONCE is packed immediately after DATA, so this equals the old
; sum of the resident segment sizes; for the padded cart-size cfgs it also
; spans the BSS gap that fill leaves ahead of ONCE.  See
; design/LYNX_STARTUP_RECLAIM_DESIGN.md.
len0 = __ONCE_PHYS__ + __ONCE_SIZE__ - __MAIN_START__
        .byte   <block0
        .word   off0 & (__BANK0BLOCKSIZE__ - 1)
        .byte   $88
        .word   __MAIN_START__
        .word   len0
__DIRECTORY_END__:

; ------------------------------------------------------------------------
; Make sure the optional segments referenced by the layout always exist, even
; if nothing in the program contributes code to them, so their linker-defined
; boundary symbols resolve. ONCE always holds the crt0 one-shot body, but a
; program that uses no IRQ/clock code has no LOWCODE otherwise. Declaring the
; segments empty here costs nothing.
        .segment "LOWCODE"
        .segment "ONCE"
