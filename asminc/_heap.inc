;
; _heap.inc
;
; (c) Copyright 2003, Ullrich von Bassewitz (uz@cc65.org)
;

; Assembler include file that makes the constants and structures in _heap.h
; available for asm code.

; Struct freeblock
; NOTE: For performance reasons, the asm code often uses increment/decrement
; operators to access other offsets, so just changing offsets here will
; probably not work.
.struct freeblock
        size    .word
        next    .addr
        prev    .addr
.endstruct

; Struct usedblock
; The raw block is always (user pointer - HEAP_ADMIN_SPACE); the word below the
; user pointer is the raw block size. No separate start back-pointer.
.struct usedblock
        size    .word
.endstruct

HEAP_MIN_BLOCKSIZE  = .sizeof (freeblock)   ; Minimum size of an allocated block
HEAP_ADMIN_SPACE    = .sizeof (usedblock)   ; Additional space for used bock

; Variables
.global         __heaporg
.global         __heapptr
.global         __heapend
.global         __heapfirst
.global         __heaplast
