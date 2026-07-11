;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; multicartldr.s -- the relocatable multicart runtime ROM loader.
;
; This is the small loader a multicart MENU copies into low memory ($0040) and
; jumps to, in order to pull one of the bundled game ROMs off the cartridge and
; run it over the top of the menu. It is a trimmed reimplementation of Bastian
; Schick's new_bll file loader (https://github.com/42Bastian/new_bll,
; includes/file.inc); the SDK's power-on bootloader (runtime/lynx/bootldr.s)
; loads only the FIRST directory entry, whereas this loader takes a file number
; and indexes into the game directory the ROM builder (lynxdir) lays down.
;
; It is NOT linked into a program directly. It is assembled and linked on its
; own with cfg/lynx-multicartldr.cfg (which locates CODE at $0040), its raw
; bytes are extracted, and those bytes are committed as
; libraries/core/multicartldr_gen.s (the _multicart_loader blob that
; multicart_run copies to $0040). Regenerate that blob with
; tools/lnx/gen-multicartldr.sh (top-level: make multicart-loader-gen) after any
; change here. See design/LYNX_MULTICART_DESIGN.md.
;
; Because the extracted bytes carry the absolute addresses the linker resolved
; at $0040, the blob is position-DEPENDENT: it only runs correctly at $0040, and
; it uses zero-page $00..$0C as scratch (the menu is being torn down, so this is
; free). The game directory sits at a fixed cart offset (MULTICART_DIROFFSET);
; entries are 8 bytes each and match new_bll's NEWHEAD directory format.

        .include "lynx/lynx.inc"

; Cartridge block size. 2048 bytes/block reaches the whole 512 KiB address space
; a Lynx cart supports without bank switching, which is the multicart target
; (bundling several games needs the space). This MUST match the #BLOCKSIZE the
; ROM builder uses -- see the .mak that "lnx multicart" writes.
BlockSize      .set 2048

; Byte offset of the game directory within cart block 0. lynxdir places the BLL
; game directory here (its DIROFFSET); each 8-byte entry N lives at
; MULTICART_DIROFFSET + N*8. $0380 = 896, the layout the LynxJam multicart used.
MULTICART_DIROFFSET .set $0380

; Resting IODAT value used while strobing the cart block-address shift register.
; Every lynxcc program runs with IODAT = $1B (crt0 onceinit programs it via the
; Mikey init table), so the loader bakes it in rather than depending on an
; initialised zero-page shadow -- it is copied to $0040 into an unknown menu ZP
; state, so a self-contained constant is the robust choice.
IODAT_RESTING  .set $1B

; new_bll directory entry: 8 bytes, little-endian words.
.struct DirEntry
    StartBlock      .byte
    BlockOffset     .word
    ExecFlag        .byte
    DestAddr        .word
    FileLen         .word
.endstruct

; ------------------------------------------------------------------------
; Private scratch. Linked at $0000..$000D by cfg/lynx-multicartldr.cfg; the
; absolute addresses are baked into the extracted blob.
.segment "ZEROPAGE"

CurrBlock:  .byte 0
BlockByte:  .word 0
entry:      .tag DirEntry
DestPtr:    .word 0

.segment "CODE"

; ------------------------------------------------------------------------
; ENTRY POINT (must be the first byte of CODE, i.e. $0040).
; Load and execute game ROM A.
; IN:  A = file number (game directory entry, 0-based)
.proc _LoadPrg
    jsr OpenFile
    jsr ReadBytes
    jmp (entry+DirEntry::DestAddr)
.endproc

; ------------------------------------------------------------------------
; Load one directory entry into `entry`.
; IN:  A = directory entry number
.proc LoadDir
    stz CurrBlock
    jsr SelectBlock
    asl
    asl
    asl                             ; A = entry * 8
    clc
    adc #<MULTICART_DIROFFSET
    eor #$FF
    tax
    lda #0
    adc #>MULTICART_DIROFFSET
    eor #$ff
    tay
    jsr ReadOver                    ; over-read to MULTICART_DIROFFSET + entry*8
    ldx #0
    ldy #8
@loopLD:
    jsr ReadByte
    sta entry,x
    inx
    dey
    bne @loopLD
    rts
.endproc

; ------------------------------------------------------------------------
; Open a file: select its block, over-read its offset, set DestPtr.
; IN:  A   : file number
; OUT: X:Y : file length
.proc OpenFile
    jsr LoadDir
    lda entry+DirEntry::DestAddr
    ora entry+DirEntry::DestAddr+1  ; dest == 0 ?
    bne @cont0                      ; no =>
    lda DestPtr                     ; yes: keep running DestPtr
    sta entry+DirEntry::DestAddr
    lda DestPtr+1
    sta entry+DirEntry::DestAddr+1
    bra @cont1
@cont0:
    lda entry+DirEntry::DestAddr
    sta DestPtr
    lda entry+DirEntry::DestAddr+1
    sta DestPtr+1
@cont1:
    lda entry+DirEntry::StartBlock
    sta CurrBlock
    jsr SelectBlock
    ldx entry+DirEntry::BlockOffset
    ldy entry+DirEntry::BlockOffset+1
    jsr ReadOver
    ldx entry+DirEntry::FileLen
    ldy entry+DirEntry::FileLen+1
    rts
.endproc

; ------------------------------------------------------------------------
; Over-read (discard) X:Y bytes ( count EOR $FFFF ).
.proc ReadOver
    inx
    bne @cont0
    iny
    beq exit
@cont0:
    jsr ReadByte
    bra ReadOver
.endproc

; ------------------------------------------------------------------------
; Read X:Y bytes ( count EOR $FFFF ) to DestPtr.
.proc ReadBytes
    inx
    bne @cont1
    iny
    beq exit
@cont1:
    jsr ReadByte
    sta (DestPtr)
    inc DestPtr
    bne ReadBytes
    inc DestPtr+1
    bra ReadBytes
.endproc

; ------------------------------------------------------------------------
; Fetch one byte from the cartridge (bank 0).
.proc ReadByte
    lda RCART0
    inc BlockByte
    bne exit
    inc BlockByte+1
    bne exit
.endproc

; ------------------------------------------------------------------------
; Strobe the cart block-address shift register to select CurrBlock.
.proc SelectBlock
    pha
    phx
    phy
    lda #IODAT_RESTING
    and #$fC
    tay
    ora #2
    tax
    lda CurrBlock
    inc CurrBlock
    SEC
    BRA @SBL2
@SLB0:
    BCC @SLB1
    STX IODAT
    CLC
@SLB1:
    INX
    STX SYSCTL1
    DEX
@SBL2:
    STX SYSCTL1
    ROL
    STY IODAT
    BNE @SLB0
    lda #IODAT_RESTING
    sta IODAT
    stz BlockByte
    lda #$100-(>BlockSize)
    sta BlockByte+1
    ply
    plx
    pla
.endproc

.proc exit
    rts
.endproc
