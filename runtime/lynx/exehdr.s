;
; Karri Kaksonen, 2011
;
; This header contains data for emulators like Handy and Mednafen
;
        .import         __BANK0BLOCKSIZE__
        .import         __BANK1BLOCKSIZE__
        .export         __EXEHDR__: absolute = 1


; ------------------------------------------------------------------------
; EXE header
        .segment "EXEHDR"
        .byte   'L','Y','N','X'                         ; magic
        .word   __BANK0BLOCKSIZE__                      ; bank 0 page size
        .word   __BANK1BLOCKSIZE__                      ; bank 1 page size
        .word   1                                       ; version number
        .asciiz "Cart name                      "       ; 32 bytes cart name
        .asciiz "Manufacturer   "                       ; 16 bytes manufacturer
        .byte   0                                       ; rotation 1=left
                                                        ; rotation 2=right
        .byte   0                                       ; AUDIN addressing 0=no 1=yes
        .byte   0                                       ; EEPROM flag bit field
                                                        ;   bits0-2 chip, b6 LynxSD,
                                                        ;   b7 8-bit word size
        .byte   0,0,0                                   ; spare (reserved)

; The cart name, manufacturer, rotation, AUDIN and EEPROM fields above can be
; rewritten per game after linking with the `lnx` tool (tools/lnx); see
; doc/lnx.html and design/LYNX_LNX_TOOL_DESIGN.md.

