;
; Karri Kaksonen, 17.09.2009
;
; Extracted and adapted from the lynx-comlynx ComLynx serial driver
; (libsrc/lynx/ser/lynx-comlynx.s) that Karri Kaksonen wrote for cc65 in 2009.
; This is original cc65 work and stays under the cc65 package license in the
; root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; unsigned char __fastcall__ ser_get (char* b);
; /* Get a character from the receive buffer and store it into the variable
; ** pointed to by b. If no data is available, SER_ERR_NO_DATA is returned.
; */
;

        .include        "zeropage.inc"
        .include        "ser.inc"

        .export         _ser_get
        .import         ser_rxbuf, ser_rxin, ser_rxout

_ser_get:
        sta     ptr1
        stx     ptr1+1                  ; Save pointer to char

        lda     ser_rxin
        cmp     ser_rxout
        bne     GetByte
        lda     #<SER_ERR_NO_DATA
        ldx     #>SER_ERR_NO_DATA
        rts
GetByte:
        ldy     ser_rxout
        lda     ser_rxbuf,y
        inc     ser_rxout
        ldx     #$00
        sta     (ptr1,x)
        txa                             ; Return code = 0
        rts
