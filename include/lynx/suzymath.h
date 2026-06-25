/*
** SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/*****************************************************************************/
/*                                                                           */
/*                                suzymath.h                                  */
/*                                                                           */
/*       Asynchronous (non-blocking) Suzy hardware math for the Lynx         */
/*                                                                           */
/*                                                                           */
/* cc65 Lynx fork. See design/LYNX_SUZY_ASYNC_MATH_DESIGN.md.                */
/*                                                                           */
/* These intrinsics are the start/poll/harvest counterpart of the           */
/* synchronous '!*'/'!/'/'!%' operators. A *_start call writes the operand   */
/* registers and triggers the Suzy math unit, then returns immediately; the  */
/* matching *_result call harvests the result. Between the two, the caller    */
/* may run unrelated CPU work so a slow divide overlaps useful computation.   */
/*                                                                           */
/* RULES (the operators' 2.6 contracts plus one more):                       */
/*   - At most ONE async operation in flight at any time.                     */
/*   - Between a *_start and its *_result, touch NO Suzy state: no TGI draw,  */
/*     no sprite launch, no other Suzy math, no SPRSYS write. (The sprite     */
/*     engine shares the math registers.)                                     */
/*   - Not interrupt-safe; sprite engine must be idle; divide-by-zero        */
/*     yields $FFFF, as with the operators.                                   */
/*                                                                           */
/* Only the DIVIDE is really worth overlapping (~44-100 cycles). Multiply is  */
/* ~11 cycles - it finishes before the next few instructions retire - and is  */
/* offered only for symmetry. Modulo and fused muldiv overlap their divide    */
/* phase; their trailing multiply/subtract run inside *_result.               */
/*                                                                           */
/*****************************************************************************/



#ifndef _SUZYMATH_H
#define _SUZYMATH_H



/* Nonzero while the math unit is still computing (SPRSYS MATHWORKING, bit 7).
** Use it to decide whether overlapped work is still pending; the *_result
** routines also poll defensively, so calling one early is safe (it blocks).
*/
#define suzy_math_busy()        ((*(volatile unsigned char*)0xFC92) & 0x80u)


/* ---- Divide: the primary async use --------------------------------------*/
void     __fastcall__ suzy_udiv_start  (unsigned n, unsigned d);
unsigned __fastcall__ suzy_udiv_result (void);
void     __fastcall__ suzy_div_start   (int n, int d);
int      __fastcall__ suzy_div_result  (void);

/* ---- Modulo: overlaps the divide; (n/d)*d finishes in *_result ----------*/
void     __fastcall__ suzy_umod_start  (unsigned n, unsigned d);
unsigned __fastcall__ suzy_umod_result (void);
void     __fastcall__ suzy_mod_start   (int n, int d);
int      __fastcall__ suzy_mod_result  (void);

/* ---- Multiply: low 16 bits, signed == unsigned; rarely worth it ---------*/
void     __fastcall__ suzy_umul_start  (unsigned a, unsigned b);
unsigned __fastcall__ suzy_umul_result (void);
#define  suzy_mul_start                 suzy_umul_start
#define  suzy_mul_result                suzy_umul_result

/* ---- Fused (a*b)/c: 32-bit intermediate; divide half overlaps -----------*/
/* All three operands are taken at start (the divisor must be in place before
** the divide is triggered). A FULL 32-bit signed product is out of scope -
** these return the 16-bit quotient, matching '!*'/'!/'. */
void     __fastcall__ suzy_umuldiv_start  (unsigned a, unsigned b, unsigned c);
unsigned __fastcall__ suzy_umuldiv_result (void);
void     __fastcall__ suzy_muldiv_start   (int a, int b, int c);
int      __fastcall__ suzy_muldiv_result  (void);



/* End of suzymath.h */
#endif
