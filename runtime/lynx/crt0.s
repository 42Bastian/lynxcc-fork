; ***
; CC65 Lynx Library
;
; Originally by Bastian Schick
; http://www.geocities.com/SiliconValley/Byte/4242/lynx/
;
; Ported to cc65 (http://www.cc65.org) by
; Shawn Jefferson, June 2004
;
; ***
;
; Startup code for cc65 (Lynx version).  Based on the Atari 8-bit startup
; code structure.  The C stack is located at the end of the RAM memory
; segment, and grows downward.  Bastian Schick's executable header is put
; on the front of the fully linked binary (see EXEHDR segment.)
;
; The startup code is split into two parts (see
; design/LYNX_STARTUP_RECLAIM_DESIGN.md):
;
;   * A tiny permanent STARTUP stub -- the reset entry vector, the resident
;     _exit trap, and a one-shot-body relocator.  These bytes stay resident
;     for the whole run.
;
;   * The one-time hardware/runtime init sequence (label onceinit), which lives
;     in the reclaimable ONCE segment.  ONCE is linked to *run* at the top of
;     the static area (__ONCE_RUN__, which is the heap origin), so once init has
;     finished the very first malloc() grows the heap over these spent bytes and
;     reclaims them.  Because the Lynx copies the file image to RAM verbatim
;     (no per-segment load step), ONCE is *loaded* lower in the image than it
;     runs; the relocator copies it up to its run address before entering it.
;     The cfg exports __ONCE_PHYS__ as ONCE's physical load address (== the
;     packed image position for the plain carts, == __ONCE_RUN__ for the
;     cart-size cfgs whose padding already lands ONCE at its run address, in
;     which case the copy is a harmless self-copy).
;
; _exit is the internal trap the program lands on if main() ever returns, and
; the stack-overflow handler (stkchk.s) jumps here too.  It runs no teardown --
; a console has no host to return to and nothing worth cleaning up -- it just
; masks interrupts and spins.  There is deliberately no public exit() C
; function on the Lynx; see include/stdlib.h.

        .export         _exit
        .export         __STARTUP__ : absolute = 1      ; Mark as startup

        .import         initlib
        .import         zerobss
        .import         callmain
        .import         __ONCE_RUN__, __ONCE_SIZE__, __ONCE_PHYS__
        .import         __MAIN_START__, __MAIN_SIZE__, __STACKSIZE__

        .include        "zeropage.inc"
        .include        "lynx/extzp.inc"
        .include "lynx/lynx.inc"

; ------------------------------------------------------------------------
; Mikey and Suzy init data, reg offsets and data

        .rodata

SuzyInitReg:    .byte $28,$2a,$04,$06,$92,$83,$90
SuzyInitData:   .byte $7f,$7f,$00,$00,$24,$f3,$01

MikeyInitReg:   .byte $00,$01,$08,$09,$20,$28,$30,$38,$44,$50,$8a,$8b,$8c,$92,$93
MikeyInitData:  .byte $9e,$18,$68,$1f,$00,$00,$00,$00,$00,$ff,$1a,$1b,$04,$0d,$29

; ------------------------------------------------------------------------
; Permanent resident stub: reset entry, the _exit trap, and the relocator.
; These bytes are never reclaimed.  The reset entry must be the first byte of
; MAIN (the bootloader jumps to the start of MAIN at the load address).

        .segment "STARTUP"

; Reset entry.  Jump to the relocator, which brings the one-shot body up to its
; run address and enters it.

        jmp     relocate

; Landing pad: main() reaching here -- by returning or falling off its end --
; traps the machine.  There is no host to return to and no teardown worth
; running on a console, so we simply mask interrupts and spin forever.  The
; stack-overflow handler (stkchk.s) also jumps here.  This lives in the
; permanent STARTUP segment so it is never overwritten by the heap.

_exit:  sei
noret:  bra     noret

; One-shot-body relocator.  Copy the ONCE segment from its physical load
; position (__ONCE_PHYS__) up to its run address (__ONCE_RUN__), then enter it.
; The copy runs descending so that a load/run overlap (ONCE larger than the BSS
; gap it is moved across) cannot clobber not-yet-copied source bytes.  A
; byte-wide index keeps this small; the assert guards the one-page assumption.

relocate:
        sei
        ldx     #$FF
        txs

        .assert __ONCE_SIZE__ < $100, lderror, "ONCE one-shot body exceeds 255 bytes; widen the crt0 relocator"
        ldx     #<__ONCE_SIZE__
        beq     enter                   ; nothing to relocate (defensive)
@copy:  dex
        lda     __ONCE_PHYS__,x
        sta     __ONCE_RUN__,x
        txa
        bne     @copy
enter:  jmp     onceinit

; ------------------------------------------------------------------------
; One-shot init body.  Lives in the reclaimable ONCE segment; runs exactly once
; before main() and is then grown over by the heap.

        .segment "ONCE"

onceinit:

; Set up the system.

        sei
        ldx     #$FF
        txs

; Init the bank switching.

        lda     #$C
        sta     MAPCTL          ; $FFF9

; Disable all timer interrupts.

        lda     #$80
        trb     TIM0CTLA
        trb     TIM1CTLA
        trb     TIM2CTLA
        trb     TIM3CTLA
        trb     TIM5CTLA
        trb     TIM6CTLA
        trb     TIM7CTLA

; Disable the TX/RX IRQ; set to 8E1.

        lda     #%00011101
        sta     SERCTL

; Clear all pending interrupts.

        lda     INTSET
        sta     INTRST

; Set up the stack.

        lda     #<(__MAIN_START__ + __MAIN_SIZE__ + __STACKSIZE__)
        ldx     #>(__MAIN_START__ + __MAIN_SIZE__ + __STACKSIZE__)
        sta     sp
        stx     sp+1

; Init Mickey.

        ldx     #.sizeof(MikeyInitReg)-1
mloop:  ldy     MikeyInitReg,x
        lda     MikeyInitData,x
        sta     $fd00,y
        dex
        bpl     mloop

; These are RAM-shadows of read-only regs.

        ldx     #$1b
        stx     __iodat
        dex                     ; $1A
        stx     __iodir
        ldx     #$d
        stx     __viddma

; Init Suzy.

        ldx     #.sizeof(SuzyInitReg)-1
sloop:  ldy     SuzyInitReg,x
        lda     SuzyInitData,x
        sta     $fc00,y
        dex
        bpl     sloop

        lda     #$24
        sta     __sprsys
        cli

; Clear the BSS data.

        jsr     zerobss

; Call the module constructors.

        jsr     initlib

; Push the return address of the resident _exit trap, then tail-jump into
; callmain.  callmain ends with "jmp _main", and main()'s rts then returns to
; the address pushed here -- the permanent _exit -- rather than to any byte of
; this reclaimable ONCE body, which the heap may already have overwritten by
; then.  (6502 rts jumps to the pushed address plus one, hence _exit-1.)

        lda     #>(_exit-1)
        pha
        lda     #<(_exit-1)
        pha
        jmp     callmain
