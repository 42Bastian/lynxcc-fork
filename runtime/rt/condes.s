;
; Ullrich von Bassewitz, 20.11.2000
;
; CC65 runtime: Support for calling module constructors/destructors
;
; The condes routine must be called with the table address in a/x and the
; size of the table (which must not be zero!) in y. The current implementation
; limits the table size to 254 bytes (127 vectors) but this shouldn't be
; problem for now and may be changed later.
;
; initlib calls condes with the predefined module constructor table; it must
; be called from the platform specific startup code.  (The Lynx runs no
; module destructors -- see the note by initlib below.)

        .export initlib, condes

        .import __CONSTRUCTOR_TABLE__, __CONSTRUCTOR_COUNT__

; --------------------------------------------------------------------------
; Initialize library modules

.segment        "ONCE"

.proc   initlib

        ldy     #<(__CONSTRUCTOR_COUNT__*2)
        beq     exit
        lda     #<__CONSTRUCTOR_TABLE__
        ldx     #>__CONSTRUCTOR_TABLE__
        jmp     condes
exit:   rts

.endproc

; NOTE: there is no donelib (module-destructor) counterpart on the Lynx.  The
; program never returns to a host and the runtime runs no teardown, so only the
; startup constructor pass (initlib) is provided.

; --------------------------------------------------------------------------
; Generic table call handler. The code uses self modifying code and goes
; into the data segment for this reason.
; NOTE: The routine must not be called if the table is empty!

.data

.proc   condes

        sta     fetch1+1
        stx     fetch1+2
        sta     fetch2+1
        stx     fetch2+2
loop:   dey
fetch1: lda     $FFFF,y                 ; Patched at runtime
        sta     jmpvec+2
        dey
fetch2: lda     $FFFF,y                 ; Patched at runtime
        sta     jmpvec+1
        sty     index+1
jmpvec: jsr     $FFFF                   ; Patched at runtime
index:  ldy     #$FF                    ; Patched at runtime
        bne     loop
        rts

.endproc

