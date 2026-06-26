/*****************************************************************************/
/*                                                                           */
/*                                  stdio.h                                  */
/*                                                                           */
/*                               Input/output                                */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2011, Ullrich von Bassewitz                                      */
/*                Roemerstrasse 52                                           */
/*                D-70794 Filderstadt                                        */
/* EMail:         uz@cc65.org                                                */
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



#ifndef _STDIO_H
#define _STDIO_H



#include <stddef.h>
#include <stdarg.h>



/* Standard defines.  SEEK_* describe the whence argument of lseek() (declared
** in <unistd.h>); only SEEK_SET is actually implemented on the Lynx cart.
*/
#define EOF             -1
#define SEEK_CUR        0
#define SEEK_END        1
#define SEEK_SET        2



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



/* Functions */
/* NOTE: the Lynx has no character console and the cart is read-only ROM, so
** there is no FILE object on this target and none of the byte-stream families
** are provided:
**   - OUTPUT: fopen/freopen/fclose, fwrite, printf/fprintf/vprintf/vfprintf,
**     putchar/puts/fputc/fputs, perror, _poserror, remove()/rename().
**   - INPUT:  fopen/fread/fgetc/fgets, fscanf/scanf/vfscanf/vscanf,
**     fseek/ftell/fgetpos/fsetpos/rewind, getchar/gets/ungetc,
**     feof/ferror/clearerr/fflush, fdopen/fileno.
** For on-screen text use gfx_outtext() from <lynx/gfx.h>.  Only the in-RAM string
** functions below remain: sprintf()/snprintf() format into a buffer and
** sscanf() parses one.  To stream raw bytes from the cart use read()/lseek()
** on a handle from openn() (see <unistd.h> and <lynx.h>).
*/
int snprintf (char* buf, size_t size, const char* format, ...);
int sprintf (char* buf, const char* format, ...);
int __fastcall__ vsnprintf (char* buf, size_t size, const char* format, va_list ap);
int __fastcall__ vsprintf (char* buf, const char* format, va_list ap);

int sscanf (const char* s, const char* format, ...);
int __fastcall__ vsscanf (const char* s, const char* format, va_list ap);



/* End of stdio.h */
#endif
