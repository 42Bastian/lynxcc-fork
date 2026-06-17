;
; Karri Kaksonen, 2010
;
; openn() positions the cart at the start of a numbered directory entry so
; that read()/lseek() can stream its bytes into RAM.
;
; Files on a Lynx cart are addressed purely by number: entry 0 is the boot
; executable, entry 1 the next file appended after it, and so on. There are
; therefore no file names to parse - the file number IS the handle - so the
; old POSIX-style open(const char *name,...) wrapper that ran the name through
; atoi() has been removed. Use openn() directly, or the lynx_load()/lynx_exec()
; convenience calls built on top of it.
;
; The cart is ROM, so access is read-only; there is no write()/O_CREAT path.
; Persistent game data lives in the EEPROM instead (see the lynx_eeread_93cNN /
; lynx_eewrite_93cNN family in <lynx.h>). Only bank 0 (the CART0 read strobe)
; is supported.
;
; void __fastcall__ openn (int fileno);
;
        .importzp       sreg
        .import         _read
        .import         _lseek
        .import         pushax,decsp6,pusha0,pusheax,ldaxysp
        .import         aslax3,axlong,tosaddeax,steaxysp,stax0sp,incsp8
        .import         lynxskip0, lynxblock
        .importzp       _FileEntry
        .importzp       _FileStartBlock
        .importzp       _FileCurrBlock
        .importzp       _FileBlockOffset
        .import         __STARTOFDIRECTORY__
        .export         _openn

.segment        "DATA"

_startofdirectory:
        .dword  __STARTOFDIRECTORY__

; ---------------------------------------------------------------
; void __near__ __fastcall__ openn (int)
; ---------------------------------------------------------------

.segment        "CODE"

.proc   _openn: near

.segment        "CODE"

        jsr     pushax
        jsr     decsp6
        lda     #$01
        jsr     pusha0
        lda     _startofdirectory+3
        sta     sreg+1
        lda     _startofdirectory+2
        sta     sreg
        ldx     _startofdirectory+1
        lda     _startofdirectory
        jsr     pusheax
        ldy     #$0D
        jsr     ldaxysp
        jsr     aslax3
        jsr     axlong
        jsr     tosaddeax
        jsr     pusheax
        ldx     #$00
        txa
        jsr     _lseek
        ldy     #$02
        jsr     steaxysp
        lda     #$01
        jsr     pusha0
        lda     #<_FileEntry
        ldx     #>_FileEntry
        jsr     pushax
        ldx     #$00
        lda     #$08
        jsr     _read
        lda     _FileStartBlock
        sta     _FileCurrBlock
        jsr     lynxblock
        lda     _FileBlockOffset+1
        eor     #$FF
        tay
        lda     _FileBlockOffset
        eor     #$FF
        tax
        jsr     lynxskip0
        jsr     stax0sp
        jmp     incsp8

.endproc
