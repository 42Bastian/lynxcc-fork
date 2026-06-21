;
; unsigned char __fastcall__ ser_open (const struct ser_params* params);
; /* Set the port parameters and enable interrupts. */
;
; (design/LYNX_JOY_SER_DESIGN.md section 3.1; baud/format logic from Karri
; Kaksonen's lynx-comlynx driver, 17.09.2009.)
;
; The Lynx has only two correct serial data formats:
; 8 bits, parity mark, 1 stop bit
; 8 bits, parity space, 1 stop bit
;
; It also has two wrong formats;
; 8 bits, even parity, 1 stop bit
; 8 bits, odd parity, 1 stop bit
;
; Unfortunately the parity bit includes itself in the calculation making
; parity not compatible with the rest of the world.
;
; We can only specify a few baud rates.
; Lynx has two non-standard speeds 31250 and 62500 which are
; frequently used in games.
;
; The receiver will always read the parity and report parity errors.
;

        .include "lynx/lynx.inc"
        .include        "zeropage.inc"
        .include        "ser.inc"

        .export         _ser_open
        .import         ser_rxin, ser_rxout, ser_txin, ser_txout
        .import         ser_ctl

_ser_open:
        sta     ptr1
        stx     ptr1+1                  ; Save pointer to params

        stz     ser_rxin
        stz     ser_rxout
        stz     ser_txin
        stz     ser_txout

        ; clock = 8 * 15625
        lda     #%00011000
        sta     TIM4CTLA
        ldy     #SER_PARAMS::BAUDRATE
        lda     (ptr1),y

        ldx     #1
        cmp     #SER_BAUD_62500
        beq     setbaudrate

        ldx     #2
        cmp     #SER_BAUD_31250
        beq     setbaudrate

        ldx     #12
        cmp     #SER_BAUD_9600
        beq     setbaudrate

        ldx     #25
        cmp     #SER_BAUD_4800
        beq     setbaudrate

        ldx     #51
        cmp     #SER_BAUD_2400
        beq     setbaudrate

        ldx     #103
        cmp     #SER_BAUD_1200
        beq     setbaudrate

        ldx     #207
        cmp     #SER_BAUD_600
        beq     setbaudrate

        ; clock = 6 * 15625
        ldx     #%00011010
        stx     TIM4CTLA

        ldx     #12
        cmp     #SER_BAUD_7200
        beq     setbaudrate

        ldx     #25
        cmp     #SER_BAUD_3600
        beq     setbaudrate

        ldx     #207
        stx     TIM4BKUP

        ; clock = 4 * 15625
        ldx     #%00011100
        cmp     #SER_BAUD_300
        beq     setprescaler

        ; clock = 6 * 15625
        ldx     #%00011110
        cmp     #SER_BAUD_150
        beq     setprescaler

        ; clock = 1 * 15625
        ldx     #%00011111
        stx     TIM4CTLA
        cmp     #SER_BAUD_75
        beq     baudsuccess

        ldx     #141
        cmp     #SER_BAUD_110
        beq     setbaudrate

        ; clock = 2 * 15625
        ldx     #%00011010
        stx     TIM4CTLA
        ldx     #68
        cmp     #SER_BAUD_1800
        beq     setbaudrate

        ; clock = 6 * 15625
        ldx     #%00011110
        stx     TIM4CTLA
        ldx     #231
        cmp     #SER_BAUD_134_5
        beq     setbaudrate

        lda     #<SER_ERR_BAUD_UNAVAIL
        ldx     #>SER_ERR_BAUD_UNAVAIL
        rts
setprescaler:
        stx     TIM4CTLA
        bra     baudsuccess
setbaudrate:
        stx     TIM4BKUP
baudsuccess:
        ldx     #TxOpenColl|ParEven
        stx     ser_ctl
        ldy     #SER_PARAMS::DATABITS   ; Databits
        lda     (ptr1),y
        cmp     #SER_BITS_8
        bne     invparameter
        ldy     #SER_PARAMS::STOPBITS   ; Stopbits
        lda     (ptr1),y
        cmp     #SER_STOP_1
        bne     invparameter
        ldy     #SER_PARAMS::PARITY     ; Parity
        lda     (ptr1),y
        cmp     #SER_PAR_NONE
        beq     invparameter
        cmp     #SER_PAR_MARK
        beq     checkhs
        cmp     #SER_PAR_SPACE
        bne     @L0
        ldx     #TxOpenColl
        stx     ser_ctl
        bra     checkhs
@L0:
        ldx     #TxParEnable|TxOpenColl|ParEven
        stx     ser_ctl
        cmp     #SER_PAR_EVEN
        beq     checkhs
        ldx     #TxParEnable|TxOpenColl
        stx     ser_ctl
checkhs:
        ldx     ser_ctl
        stx     SERCTL
        ldy     #SER_PARAMS::HANDSHAKE  ; Handshake
        lda     (ptr1),y
        cmp     #SER_HS_NONE
        bne     invparameter
        lda     SERDAT
        lda     ser_ctl
        ora     #RxIntEnable|ResetErr
        sta     SERCTL
        lda     #<SER_ERR_OK
        ldx     #>SER_ERR_OK
        rts
invparameter:
        lda     #<SER_ERR_INIT_FAILED
        ldx     #>SER_ERR_INIT_FAILED
        rts
