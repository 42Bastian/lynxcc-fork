/*****************************************************************************/
/*                                                                           */
/*                              spritesheet.h                                */
/*                                                                           */
/*     Sprite-sheet driver for the sp65 sprite and bitmap utility            */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2026,      the lynxcc authors                                         */
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



#ifndef SPRITESHEET_H
#define SPRITESHEET_H



/* sp65 */
#include "bitmap.h"



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



void SpriteSheet (const Bitmap* B, const char* ArgList);
/* Slice the bitmap B into a grid of equal-sized frames, encode each frame as
** a standalone Lynx sprite, and write a single C or assembler header that
** exposes the concatenated frame data plus an indexable table of pointers to
** the start of each frame. The grid and output are described by ArgList, a
** comma-separated name=value attribute list:
**
**   fw,fh           frame cell width/height in pixels   (required)
**   cols,rows       grid dimensions                     (default: filled from B)
**   first           index of the first frame to emit    (default 0)
**   count           number of frames to emit            (default cols*rows-first)
**   gap             pixels between adjacent cells        (default 0)
**   margin          pixels of border around the grid     (default 0)
**   mode            literal | packed | shaped | auto     (default auto)
**   ax,ay,edge      passed through to the sprite encoder (default 0)
**   name            output file name                     (required)
**   format          c | asm   (default: from name's extension)
**   ident           C/asm symbol for the frame table     (required)
**   bytesperline    data formatting                       (default 16)
**   base            number base 10|16 (C), 2|10|16 (asm)  (default 16)
**
** Frames are taken in row-major order. The emitted frame data is byte-for-byte
** identical to encoding each cell on its own; the only added bytes are the
** pointer table itself.
*/



/* End of spritesheet.h */

#endif
