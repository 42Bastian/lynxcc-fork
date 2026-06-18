/*****************************************************************************/
/*                                                                           */
/*                                zeropage.h                                  */
/*                                                                           */
/*                  Place a C variable in the 6502 zero page                 */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2026 lynxcc                                                           */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/



#ifndef _ZEROPAGE_H
#define _ZEROPAGE_H



/* Place a variable in the zero page.
**
** Use as a suffix on a file-scope object declaration:
**
**     unsigned char  flag   __zeropage;     // definition (lives in ZEROPAGE)
**     extern unsigned char flag __zeropage;  // reference (imported as zp)
**
** The compiler also accepts the equivalent prefix keyword __zeropage__, which
** needs no header and combines with a storage class:
**
**     __zeropage__ unsigned char flag;
**     static __zeropage__ unsigned char counter;
**
** The two forms are interchangeable. The defining translation unit emits the
** object into the built-in ZEROPAGE segment; every translation unit that marks
** the (extern) declaration as zero page addresses it through the fast zero-page
** addressing modes. Put the tagged extern in a shared header so every user
** agrees on the address size.
**
** Restrictions (diagnosed by the compiler):
**   - only valid at file scope (not on local/automatic objects),
**   - not valid on functions or typedefs,
**   - a zeropage definition may not have an initializer.
**
** Zero page is a scarce, 256-byte resource shared with the cc65 runtime; use
** it for a few hot scalars and pointers, not for arrays or large structs.
** Over-allocation is reported by ld65 as a ZEROPAGE segment overflow.
*/
#define __zeropage      __attribute__ ((zeropage))



/* End of zeropage.h */
#endif
