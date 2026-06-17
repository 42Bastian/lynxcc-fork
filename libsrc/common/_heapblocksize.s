;
; Ullrich von Bassewitz, 2004-07-17
;
; size_t __fastcall__ _heapblocksize (const void* ptr);
;
; Return the size of an allocated block.
;

        .importzp       ptr1, ptr2
        .export         __heapblocksize

        .include        "_heap.inc"

        .macpack        generic

;-----------------------------------------------------------------------------
; Code

__heapblocksize:

; The raw block is HEAP_ADMIN_SPACE bytes below the user pointer, and its first
; word is the raw block size. Decrement the high byte of the pointer so the size
; word is reachable at offset 254/255 (user-2/user-1), and read it.

        sta     ptr1
        dex
        stx     ptr1+1          ; ptr1 = user - 256
        ldy     #$FE            ; Offset 254 -> user-2: raw size low
        lda     (ptr1),y
        sta     ptr2
        iny                     ; Offset 255 -> user-1: raw size high
        lda     (ptr1),y
        sta     ptr2+1

; Return the user-visible size, which is the raw size minus the admin header.
;
;       return size - HEAP_ADMIN_SPACE

        lda     ptr2
        sub     #HEAP_ADMIN_SPACE
        ldx     ptr2+1
        bcs     @L1
        dex
@L1:    rts

