;
; Ullrich von Bassewitz, 03.06.1998
;
; Heap variables and initialization.
;

        .constructor    initheap, 24
        .import         __ONCE_RUN__, __STACKSIZE__
        .importzp       sp

        .include        "_heap.inc"


.data

; The heap anchors on __ONCE_RUN__, the run base of the reclaimable crt0
; one-shot body (see runtime/lynx/crt0.s and
; design/LYNX_STARTUP_RECLAIM_DESIGN.md).  ONCE runs at the very top of the
; static area -- immediately above BSS -- so allocation begins over the spent
; startup code and reclaims it.  With ONCE placed directly after BSS this is
; numerically the old end-of-BSS origin (__BSS_RUN__ + __BSS_SIZE__); the
; symbolic form keeps the heap correct if ONCE ever gains leading alignment.
__heaporg:
        .word   __ONCE_RUN__                    ; Linker calculates this symbol
__heapptr:
        .word   __ONCE_RUN__                    ; Dito
__heapend:
        .word   __ONCE_RUN__
__heapfirst:
        .word   0
__heaplast:
        .word   0


; Initialization. Will be called from startup!

.segment        "ONCE"

initheap:
        sec
        lda     sp
        sbc     #<__STACKSIZE__
        sta     __heapend
        lda     sp+1
        sbc     #>__STACKSIZE__
        sta     __heapend+1
        rts

                      
