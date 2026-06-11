;
; Ullrich von Bassewitz, 2003-08-20
; Performance increase (about 20%) by
; Christian Krueger, 2009-09-13
;
; void* __fastcall__ memcpy (void* dest, const void* src, size_t n);
;
; NOTE: This function contains entry points for memmove, which will ressort
; to memcpy for an upwards copy. Don't change this module without looking
; at memmove!
;

        .export         _memcpy, memcpy_upwards, memcpy_getparams
        .import         popax, popptr1
        .importzp       sp, ptr1, ptr2, ptr3

; ----------------------------------------------------------------------
_memcpy:
        jsr     memcpy_getparams

.if .defined(__LYNX__)

; On the Lynx the whole address space is RAM, so we can use self modifying
; code: For large copies, the 256 byte block loop uses absolute,Y addressing
; with patched operands, which is faster than (zp),Y. The patching costs are
; only paid when there is at least one full 256 byte block to copy. The zero
; page pointers are kept in sync, so the tail loop below works unchanged.
; Note: Like the rest of the zero page based runtime, this function is not
; reentrant.

memcpy_upwards:                 ; assert Y = 0
        ldx     ptr3+1          ; Get high byte of n
        beq     L2              ; Jump if zero

        lda     ptr1            ; Patch the source operands
        sta     L1a+1
        sta     L1c+1
        lda     ptr1+1
        sta     L1a+2
        sta     L1c+2
        lda     ptr2            ; Patch the destination operands
        sta     L1b+1
        sta     L1d+1
        lda     ptr2+1
        sta     L1b+2
        sta     L1d+2

L1:                             ; Unrolled copy of 256 byte blocks
L1a:    lda     $FADE,y         ; Patched: source
L1b:    sta     $FADE,y         ; Patched: destination
        iny
L1c:    lda     $FADE,y         ; Patched: source
L1d:    sta     $FADE,y         ; Patched: destination
        iny
        bne     L1
        inc     L1a+2           ; Advance the patched operands
        inc     L1c+2           ; to the next page
        inc     L1b+2
        inc     L1d+2
        inc     ptr1+1          ; Keep the zero page pointers in
        inc     ptr2+1          ; sync for the tail loop
        dex                     ; Next 256 byte block
        bne     L1              ; Repeat if any

.else

memcpy_upwards:                 ; assert Y = 0
        ldx     ptr3+1          ; Get high byte of n
        beq     L2              ; Jump if zero

L1:     .repeat 2               ; Unroll this a bit to make it faster...
        lda     (ptr1),Y        ; copy a byte
        sta     (ptr2),Y
        iny
        .endrepeat
        bne     L1
        inc     ptr1+1
        inc     ptr2+1
        dex                     ; Next 256 byte block
        bne     L1              ; Repeat if any

.endif

        ; the following section could be 10% faster if we were able to copy
        ; back to front - unfortunately we are forced to copy strict from
        ; low to high since this function is also used for
        ; memmove and blocks could be overlapping!
        ; {
L2:                             ; assert Y = 0
        ldx     ptr3            ; Get the low byte of n
        beq     done            ; something to copy

L3:     lda     (ptr1),Y        ; copy a byte
        sta     (ptr2),Y
        iny
        dex
        bne     L3

        ; }

done:   jmp     popax           ; Pop ptr and return as result

; ----------------------------------------------------------------------
; Get the parameters from stack as follows:
;
;       size            --> ptr3
;       src             --> ptr1
;       dest            --> ptr2
;       First argument (dest) will remain on stack and is returned in a/x!

memcpy_getparams:               ; IMPORTANT! Function has to leave with Y=0!
        sta     ptr3
        stx     ptr3+1          ; save n to ptr3

        jsr     popptr1         ; save src to ptr1

                                ; save dest to ptr2
        iny                     ; Y=0 guaranteed by popptr1, we need '1' here...                        
                                ; (direct stack access is three cycles faster
                                ; (total cycle count with return))
        lda     (sp),y
        tax
        stx     ptr2+1          ; save high byte of ptr2
        dey                     ; Y = 0
        lda     (sp),y          ; Get ptr2 low
        sta     ptr2
        rts
