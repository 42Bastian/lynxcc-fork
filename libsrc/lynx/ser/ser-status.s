;
; unsigned char __fastcall__ ser_status (unsigned char* status);
; /* Return the serial port status (the error bits accumulated by the IRQ
; ** handler) in the variable pointed to by status.
; */
;

        .include        "zeropage.inc"
        .include        "ser.inc"

        .export         _ser_status
        .import         ser_stat

_ser_status:
        sta     ptr1
        stx     ptr1+1                  ; Save pointer to status
        ldy     ser_stat
        ldx     #$00
        tya
        sta     (ptr1,x)
        txa                             ; Return code = 0
        rts
