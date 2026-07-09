;
; Ullrich von Bassewitz, 26.10.2000
;
; CC65 runtime: Store a indirect into address at top of stack with index
;

        .export         staspidx
        .import         incsp2
        .importzp       sp, tmp1, ptr1

.proc   staspidx

; 65SC02: save the index on the CPU stack instead of tmp1 (frees tmp1) and
; fetch the pointer low byte through (sp). Exit state matches the 6502
; version: Y restored to the entry index, value stored, address dropped.
        pha
        phy                     ; Save Index
        ldy     #1
        lda     (sp),y
        sta     ptr1+1
        lda     (sp)
        sta     ptr1            ; Pointer now in ptr1
        ply                     ; Restore offset
        pla                     ; Restore value
        sta     (ptr1),y        ; Store
        jmp     incsp2          ; Drop address

.endproc

