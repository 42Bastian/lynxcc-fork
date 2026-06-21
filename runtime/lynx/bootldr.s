;
; Karri Kaksonen, 2011
;
; This bootloader creates a signed binary so that the Lynx will accept it.
;
        .include "lynx/lynx.inc"
        .include "lynx/extzp.inc"
        .import         __BANK0BLOCKSIZE__
        .export         __BOOTLDR__: absolute = 1


; ------------------------------------------------------------------------
; Bootloader

        .segment "BOOTLDR"
;**********************************
; Here is the bootloader in plaintext
; The idea is to make the smalles possible encrypted loader as decryption
; is very slow. The minimum size is 49 bytes plus a zero byte.
;**********************************
;       EXE = $fb68
;
;       .org $0200
;
;       ; 1. force Mikey to be in memory
;       stz MAPCTL
;
;       ; 2. clear palette
;       ldy #31
;       lda #0
;nextc: sta GCOLMAP, y
;       dey
;       bpl nextc
;
;       ; 3. set ComLynx to open collector
;       lda #4          ; a = 00000100
;       sta SERCTL      ; set the ComLynx to open collector
;
;       ; 4. set AUDIN to output
;       lda #$1a        ; audin = out, rest = out, noexp = in,
;                       ; cart addr = out, ext pwd = in
;       sta IODIR
;
;       ; 5. set AUDIN to LOW (also cart addr low for pwr to cart rom)
;       lda #$08
;       sta $1a         ; save local copy to ZP
;       sta IODAT
;
;       ; 6. read in secondary exe + 8 bytes from the cart and store it in $f000
;       ldx #0          ; x = 0
;       ldy #$97        ; y = secondary loader size (151 bytes)
;rloop1: lda RCART0     ; read a byte from the cart
;       sta EXE,X       ; EXE[X] = a
;       inx             ; x++
;       dey             ; y--
;       bne rloop1      ; loops until y wraps
;
;       ; 7. jump to secondary loader
;       jmp EXE         ; run the secondary loader
;
;       .reloc
;**********************************
; After compilation, encryption and obfuscation it turns into this.
;**********************************
        .byte $ff, $31, $cb, $62, $b4, $4f, $ee, $84
        .byte $3b, $dc, $ab, $d3, $cb, $df, $e3, $77
        .byte $e6, $35, $38, $2b, $52, $81, $d5, $c3
        .byte $ce, $fd, $e0, $d3, $32, $68, $32, $a2
        .byte $b3, $79, $36, $95, $fa, $77, $59, $80
        .byte $12, $01, $25, $49, $d2, $e1, $6e, $5d
        .byte $55, $63, $86, $18

;**********************************
; Now we have the secondary loader
;**********************************
        .org $fb68
        ; 1. Read in the 1st File-entry (main exe) in FileEntry
        ldx #$00
        ldy #8
rloop:  lda RCART0      ; read a byte from the cart
        sta _FileEntry,X ; EXE[X] = a
        inx
        dey
        bne rloop

        ; 2. Set the block hardware to the main exe start
        lda     _FileStartBlock
        sta     _FileCurrBlock
        jsr     seclynxblock

        ; 3. Skip over the block offset
        lda     _FileBlockOffset+1
        eor     #$FF
        tay
        lda     _FileBlockOffset
        eor     #$FF
        tax
        jsr     seclynxskip0

        ; 4. Read in the main exe to RAM
        lda     _FileDestAddr
        ldx     _FileDestAddr+1
        sta     _FileDestPtr
        stx     _FileDestPtr+1
        lda     _FileFileLen+1
        eor     #$FF
        tay
        lda     _FileFileLen
        eor     #$FF
        tax
        jsr     seclynxread0

        ; 5. Jump to start of the main exe code
        jmp     (_FileDestAddr)

;**********************************
; Skip bytes on bank 0
; X:Y count (EOR $FFFF)
;**********************************
seclynxskip0:
        inx
        bne @0
        iny
        beq exit
@0:     jsr secreadbyte0
        bra seclynxskip0

;**********************************
; Read bytes from bank 0
; X:Y count (EOR $ffff)
;**********************************
seclynxread0:
        inx
        bne @1
        iny
        beq exit
@1:     jsr secreadbyte0
        sta (_FileDestPtr)
        inc _FileDestPtr
        bne seclynxread0
        inc _FileDestPtr+1
        bra seclynxread0

;**********************************
; Read one byte from cartridge
;**********************************
secreadbyte0:
        lda RCART0
        inc _FileBlockByte
        bne exit
        inc _FileBlockByte+1
        bne exit

;**********************************
; Select a block 
;**********************************
seclynxblock:
        pha
        phx
        phy
        lda __iodat
        and #$fc
        tay
        ora #2
        tax
        lda _FileCurrBlock
        inc _FileCurrBlock
        sec
        bra @2
@0:     bcc @1
        stx IODAT
        clc
@1:     inx
        stx SYSCTL1
        dex
@2:     stx SYSCTL1
        rol
        sty IODAT
        bne @0
        lda __iodat
        sta IODAT
        stz _FileBlockByte
        lda #<($100-(>__BANK0BLOCKSIZE__))
        sta _FileBlockByte+1
        ply
        plx
        pla

exit:   rts

        .reloc

