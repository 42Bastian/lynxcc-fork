/*
** _heap.h
**
** Ullrich von Bassewitz, 1998-06-03, 2004-12-19
**
*/



#ifndef __HEAP_H
#define __HEAP_H



/* Structure that preceeds a user block.
** The user pointer returned by malloc() points HEAP_ADMIN_SPACE bytes above
** this header, so the raw block is always exactly
** (user_ptr - HEAP_ADMIN_SPACE). There is no separate start back-pointer:
** the word immediately below the user pointer is the raw block size.
*/
struct usedblock {
    unsigned            size;
};

/* Space needed for administering used blocks */
#define HEAP_ADMIN_SPACE        sizeof (struct usedblock)

/* The data type used to implement the free list.
** Beware: Field order is significant!
*/
struct freeblock {
    unsigned            size;
    struct freeblock*   next;
    struct freeblock*   prev;
};



/* Variables that describe the heap */
extern unsigned*          _heaporg;     /* Bottom of heap */
extern unsigned*          _heapptr;     /* Current top */
extern unsigned*          _heapend;     /* Upper limit */
extern struct freeblock*  _heapfirst;   /* First free block in list */
extern struct freeblock*  _heaplast;    /* Last free block in list */



/* End of _heap.h */

#endif



