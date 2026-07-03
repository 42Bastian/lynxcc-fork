;
; IRQ handling (Lynx version)
;

        .export         initirq
        .import         callirq

        .include "lynx/lynx.inc"

; ------------------------------------------------------------------------

.segment        "ONCE"

initirq:
        lda     #<IRQStub
        ldx     #>IRQStub
        sei
        sta     INTVECTL
        stx     INTVECTH
        cli
        rts

; NOTE: there is no doneirq (IRQ-teardown) counterpart.  The runtime runs no
; module destructors on the Lynx (the program never returns to a host), so
; there is nothing to release the IRQ vector for.

; ------------------------------------------------------------------------

.segment        "LOWCODE"

IRQStub:
        phy
        phx
        pha
        jsr     callirq
        lda     INTSET
        sta     INTRST
        pla
        plx
        ply
        rti
