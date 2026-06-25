;
; Karri Kaksonen, 17.09.2009
;
; Extracted and adapted from the lynx-comlynx ComLynx serial driver
; (libsrc/lynx/ser/lynx-comlynx.s) that Karri Kaksonen wrote for cc65 in 2009.
; This is original cc65 work and stays under the cc65 package license in the
; root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; unsigned char ser_close (void);
; /* Close the port: disable serial interrupts, stop the baud timer, drop
; ** buffered data. (design/LYNX_JOY_SER_DESIGN.md section 3.1 -- behavior fix:
; ** the old driver's CLOSE was a stub that returned OK without touching
; ** the hardware.)
; */
;

        .include "lynx/lynx.inc"
        .include        "ser.inc"

        .export         _ser_close
        .import         ser_rxin, ser_rxout, ser_txin, ser_txout
        .import         ser_ctl, ser_txdone

_ser_close:
        php
        sei                             ; Don't race the IRQ handler
        lda     ser_ctl                 ; Shadow never carries int enables
        ora     #ResetErr
        sta     SERCTL                  ; Rx/Tx interrupts off
        stz     TIM4CTLA                ; Stop the baud rate timer
        stz     ser_txdone
        stz     ser_rxin                ; Drop buffers
        stz     ser_rxout
        stz     ser_txin
        stz     ser_txout
        lda     #SERIAL_INTERRUPT       ; Clear any pending serial interrupt
        sta     INTRST
        plp
        lda     #<SER_ERR_OK
        ldx     #>SER_ERR_OK
        rts
