;
; Karri Kaksonen, 17.09.2009
;
; Extracted and adapted from the lynx-comlynx ComLynx serial driver
; (libsrc/lynx/ser/lynx-comlynx.s) that Karri Kaksonen wrote for cc65 in 2009.
; This is original cc65 work and stays under the cc65 package license in the
; root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; unsigned char __fastcall__ ser_put (char b);
; /* Send a character via the ComLynx port. Transmission is interrupt
; ** driven through a ring buffer; returns SER_ERR_OVERFLOW if there is
; ** no space left in it.
; */
;

        .include "lynx/lynx.inc"
        .include        "ser.inc"

        .export         _ser_put
        .import         ser_txbuf, ser_txin, ser_txout
        .import         ser_ctl, ser_txdone

_ser_put:
        tax
        lda     ser_txin
        ina
        cmp     ser_txout
        bne     PutByte
        lda     #<SER_ERR_OVERFLOW
        ldx     #>SER_ERR_OVERFLOW
        rts
PutByte:
        ldy     ser_txin
        txa
        sta     ser_txbuf,y
        inc     ser_txin

        bit     ser_txdone
        bmi     @L1
        php
        sei
        lda     ser_ctl
        ora     #TxIntEnable|ResetErr
        sta     SERCTL                  ; Allow TX-IRQ to hang RX-IRQ
        sta     ser_txdone
        plp
@L1:
        lda     #<SER_ERR_OK
        tax
        rts
