;****************************************************************************
;*                                                                          *
;*                                 ser.inc                                  *
;*                                                                          *
;*               ComLynx serial API constants (static library)              *
;*                                                                          *
;* Static, driver-less serial support for the Lynx-only cc65 tree           *
;* (design/LYNX_JOY_SER_DESIGN.md section 3). Constants only --             *
;* deliberately NO .global lists here: unreferenced imports pulled in via   *
;* an include defeat ld65 smart linking (same rule as tgi-kernel.inc).      *
;*                                                                          *
;****************************************************************************


;------------------------------------------------------------------------------
; ser_params offsets

.struct SER_PARAMS
        BAUDRATE        .byte           ; Baudrate
        DATABITS        .byte           ; Number of data bits
        STOPBITS        .byte           ; Number of stop bits
        PARITY          .byte           ; Parity setting
        HANDSHAKE       .byte           ; Type of handshake to use
.endstruct

;------------------------------------------------------------------------------
; Baud rates implemented by the ComLynx hardware (values match serial.h)

SER_BAUD_75             =       $02
SER_BAUD_110            =       $03
SER_BAUD_134_5          =       $04
SER_BAUD_150            =       $05
SER_BAUD_300            =       $06
SER_BAUD_600            =       $07
SER_BAUD_1200           =       $08
SER_BAUD_1800           =       $09
SER_BAUD_2400           =       $0A
SER_BAUD_3600           =       $0B
SER_BAUD_4800           =       $0C
SER_BAUD_7200           =       $0D
SER_BAUD_9600           =       $0E
SER_BAUD_31250          =       $14
SER_BAUD_62500          =       $15

; Data bit settings (only 8 is accepted)
SER_BITS_8              =       $03

; Stop bit settings (only 1 is accepted)
SER_STOP_1              =       $00

; Parity. NONE is rejected (the hardware always sends a ninth bit); note
; that the Lynx includes the parity bit itself in the EVEN/ODD calculation.
SER_PAR_NONE            =       $00
SER_PAR_ODD             =       $01
SER_PAR_EVEN            =       $02
SER_PAR_MARK            =       $03
SER_PAR_SPACE           =       $04

; Handshake (only NONE is accepted)
SER_HS_NONE             =       $00

;------------------------------------------------------------------------------
; Error codes (renumbered for the static library; values match serial.h)

SER_ERR_OK              =       $00     ; Not an error - relax
SER_ERR_BAUD_UNAVAIL    =       $01     ; Baud rate not available
SER_ERR_NO_DATA         =       $02     ; Nothing to read
SER_ERR_OVERFLOW        =       $03     ; No room in send buffer
SER_ERR_INIT_FAILED     =       $04     ; Initialization failed
