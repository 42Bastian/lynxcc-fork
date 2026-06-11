;
; void* __fastcall__ memset (void* ptr, int c, size_t n);
; void* __fastcall__ _bzero (void* ptr, size_t n);
; void __fastcall__ bzero (void* ptr, size_t n);
;
; Ullrich von Bassewitz, 29.05.1998
; Performance increase (about 20%) by
; Christian Krueger, 12.09.2009, slightly improved 12.01.2011
;
; NOTE: bzero will return it's first argument as memset does. It is no problem
;       to declare the return value as void, since it may be ignored. _bzero
;       (note the leading underscore) is declared with the proper return type,
;       because the compiler will replace memset by _bzero if the fill value
;       is zero, and the optimizer looks at the return type to see if the value
;       in a/x is of any use.
;

        .export         _memset, _bzero, __bzero
        .import         popax
        .importzp       sp, ptr1, ptr2, ptr3

_bzero:
__bzero:
        sta     ptr3
        stx     ptr3+1          ; Save n
        ldx     #0              ; Fill with zeros
        beq     common

_memset:
        sta     ptr3            ; Save n
        stx     ptr3+1
        jsr     popax           ; Get c
        tax

; Common stuff for memset and bzero from here

common:                         ; Fill value is in X!
        ldy     #1
        lda     (sp),y
        sta     ptr1+1          ; save high byte of ptr
        dey                     ; Y = 0
        lda     (sp),y          ; Get ptr
        sta     ptr1

        lsr     ptr3+1          ; divide number of
        ror     ptr3            ; bytes by two to increase
        bcc     evenCount       ; speed (ptr3 = ptr3/2)
oddCount:
                                ; y is still 0 here
        txa                     ; restore fill value
        sta     (ptr1),y        ; save value and increase
        inc     ptr1            ; dest. pointer
        bne     evenCount
        inc     ptr1+1
evenCount:
        lda     ptr1            ; build second pointer section
        clc
        adc     ptr3            ; ptr2 = ptr1 + (length/2) <- ptr3
        sta     ptr2
        lda     ptr1+1
        adc     ptr3+1
        sta     ptr2+1

.if .defined(__LYNX__)

; On the Lynx the whole address space is RAM, so we can use self modifying
; code: For large fills, the 256 byte block loop uses absolute,Y addressing
; with patched operands. Stores with absolute,Y take 5 instead of 6 cycles.
; The patching costs are only paid when there is at least one full block to
; fill. The zero page pointers are kept in sync for the tail loop below.
; Note: Like the rest of the zero page based runtime, this function is not
; reentrant.
; The fill value is in X here, A is free for the patching.

        lda     ptr3+1          ; Get high byte of n
        beq     L2S             ; Jump if zero - no blocks, small fill

        lda     ptr1            ; Patch the lower section operands
        sta     L1a+1
        sta     L1c+1
        lda     ptr1+1
        sta     L1a+2
        sta     L1c+2
        lda     ptr2            ; Patch the upper section operands
        sta     L1b+1
        sta     L1d+1
        lda     ptr2+1
        sta     L1b+2
        sta     L1d+2

        txa                     ; Restore fill value
        ldx     ptr3+1          ; Get the block count

; Set 256/512 byte blocks
                                ; y is still 0 here
L1:
L1a:    sta     $FADE,y         ; Patched: lower section
L1b:    sta     $FADE,y         ; Patched: upper section
        iny
L1c:    sta     $FADE,y         ; Patched: lower section
L1d:    sta     $FADE,y         ; Patched: upper section
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
        beq     L2              ; Blocks done, fill value still in A

L2S:    txa                     ; Restore fill value

.else

        txa                     ; restore fill value
        ldx     ptr3+1          ; Get high byte of n
        beq     L2              ; Jump if zero

; Set 256/512 byte blocks
                                ; y is still 0 here
L1:     .repeat 2               ; Unroll this a bit to make it faster
        sta     (ptr1),y        ; Set byte in lower section
        sta     (ptr2),y        ; Set byte in upper section
        iny
        .endrepeat
        bne     L1
        inc     ptr1+1
        inc     ptr2+1
        dex                     ; Next 256 byte block
        bne     L1              ; Repeat if any

.endif

; Set the remaining bytes if any

L2:     ldy     ptr3            ; Get the low byte of n
        beq     leave           ; something to set? No -> leave

L3:     dey
        sta     (ptr1),y                ; set bytes in low
        sta     (ptr2),y                ; and high section
        bne     L3              ; flags still up to date from dey!
leave:  
        jmp     popax           ; Pop ptr and return as result

                
