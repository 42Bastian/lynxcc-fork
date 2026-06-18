/*****************************************************************************/
/*                                                                           */
/*                                  fcntl.h                                  */
/*                                                                           */
/*                            File control operations                        */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2004 Ullrich von Bassewitz                                       */
/*               R�merstra�e 52                                              */
/*               D-70794 Filderstadt                                         */
/* EMail:        uz@cc65.org                                                 */
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



#ifndef _FCNTL_H
#define _FCNTL_H



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* Flag values for the open() call */
#define O_RDONLY        0x01
#define O_WRONLY        0x02
#define O_RDWR          0x03
#define O_CREAT         0x10
#define O_TRUNC         0x20
#define O_APPEND        0x40
#define O_EXCL          0x80



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



/* Functions */
/* NOTE: the Atari Lynx cart is read-only ROM whose files are addressed by
** number, so there is nothing to open or create by name, nothing to write,
** and no file descriptors to close.  open(), creat(), close() and write()
** are therefore NOT provided.  Use openn() from <lynx.h> for numbered,
** read-only cart access, then read()/lseek() from <unistd.h> to operate on
** it.  The O_xxx flags above are retained only for the assembler runtime.
*/



/* End of fcntl.h */
#endif



