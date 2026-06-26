;
; Karri Kaksonen, 17.09.2009
;
; Extracted and adapted from the lynx-comlynx ComLynx serial driver
; (libsrc/lynx/ser/lynx-comlynx.s) that Karri Kaksonen wrote for cc65 in 2009.
; This is original cc65 work and stays under the cc65 package license in the
; root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; ComLynx serial core: buffers, state, and the serial IRQ handler.
; (design/LYNX_JOY_SER_DESIGN.md section 3.3.)
;
; This module links whenever any ser function is used. The IRQ handler is
; in the interruptor chain at priority 29 -- deliberately higher than the
; Lynx graphics VBL swap handler, so a pending serial byte is drained before a frame
; swap burns cycles. When the port has never been opened, timer 4 is never
; started, the INTSET bit stays clear, and the handler exits immediately.
;
; Invariant (Lynx graphics design section 5): SERCTL is write-only -- its read view
; is the status register. Every SERCTL store is composed from the ser_ctl
; shadow; never read SERCTL to "preserve bits".
;
; The ring buffers are 256 bytes per direction: the pointer arithmetic
; relies on natural 8-bit wraparound. Not tunable without rewriting it.
;

        .include "lynx/lynx.inc"

        .export         ser_txbuf, ser_rxbuf
        .export         ser_rxin, ser_rxout, ser_txin, ser_txout
        .export         ser_ctl, ser_stat, ser_txdone

        .interruptor    ser_irq, 29     ; Export as high priority IRQ handler

;----------------------------------------------------------------------------
; Buffers and state

        .bss

ser_txbuf:      .res    256
ser_rxbuf:      .res    256
ser_rxin:       .res    1
ser_rxout:      .res    1
ser_txin:       .res    1
ser_txout:      .res    1
ser_ctl:        .res    1               ; SERCTL shadow (sans int enables)
ser_stat:       .res    1               ; Accumulated status/error bits
ser_txdone:     .res    1               ; $80 while a transmit is in progress

        .code

;----------------------------------------------------------------------------
; ser_irq: Called from the runtime IRQ handler as a subroutine. All registers
; are already saved, no parameters are passed, but the carry flag is clear on
; entry. The routine must return with carry set if the interrupt was handled,
; otherwise with carry clear.
;
; Both the Tx and Rx interrupts are level sensitive instead of edge sensitive.
; Due to this bug you have to disable the interrupt before clearing it.

ser_irq:
        lda     INTSET          ; Poll all pending interrupts
        and     #SERIAL_INTERRUPT
        bne     @L0
        clc
        rts
@L0:
        bit     ser_txdone
        bmi     @tx_irq         ; Transmit in progress
        ldx     SERDAT
        lda     SERCTL
        and     #RxParityErr|RxOverrun|RxFrameErr|RxBreak
        beq     @rx_irq
        tsb     ser_stat        ; Save error condition
        bit     #RxBreak
        beq     @noBreak
        stz     ser_txin        ; Break received - drop buffers
        stz     ser_txout
        stz     ser_rxin
        stz     ser_rxout
@noBreak:
        lda     ser_ctl
        ora     #RxIntEnable|ResetErr
        sta     SERCTL
        lda     #$10
        sta     INTRST
        bra     @IRQexit
@rx_irq:
        lda     ser_ctl
        ora     #RxIntEnable|ResetErr
        sta     SERCTL
        txa
        ldx     ser_rxin
        sta     ser_rxbuf,x
        txa
        inx

@cont0:
        cpx     ser_rxout
        beq     @1
        stx     ser_rxin
        lda     #SERIAL_INTERRUPT
        sta     INTRST
        bra     @IRQexit

@1:
        sta     ser_rxin
        lda     #$80
        tsb     ser_stat
@tx_irq:
        ldx     ser_txout       ; Has all bytes been sent?
        cpx     ser_txin
        beq     @allSent

        lda     ser_txbuf,x     ; Send next byte
        sta     SERDAT
        inc     ser_txout

@exit1:
        lda     ser_ctl
        ora     #TxIntEnable|ResetErr
        sta     SERCTL
        lda     #SERIAL_INTERRUPT
        sta     INTRST
        bra     @IRQexit

@allSent:
        lda     SERCTL          ; All bytes sent
        bit     #TxEmpty
        beq     @exit1
        bvs     @exit1
        stz     ser_txdone
        lda     ser_ctl
        ora     #RxIntEnable|ResetErr
        sta     SERCTL

        lda     #SERIAL_INTERRUPT
        sta     INTRST
@IRQexit:
        clc
        rts
