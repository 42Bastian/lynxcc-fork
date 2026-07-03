/*****************************************************************************/
/*                                                                           */
/*                                 dirent.h                                  */
/*                                                                           */
/*                        Directory entries for cc65                         */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2005  Oliver Schmidt, <ol.sc@web.de>                                  */
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



#ifndef _DIRENT_H
#define _DIRENT_H



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* The cart's per-file directory entry, as populated by the runtime. */
struct dirent {
    unsigned char       d_blocks;
    unsigned int        d_offset;
    char                d_type;
    void                *d_address;
    unsigned int        d_size;
};

extern struct dirent FileEntry;
#pragma zpsym ("FileEntry");

/* NOTE: the _DE_ISREG/_DE_ISDIR/_DE_ISLBL/_DE_ISLNK entry-type classifier
** macros are intentionally NOT provided on the Lynx.  They only make sense
** together with the directory-stream functions (opendir/readdir/...), which
** the cart does not have (see the note below), so on this target they would
** be dead macros with nothing to classify.
*/



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



/* NOTE: the Lynx cart has no traversable directory, so the directory-stream
** functions (opendir/readdir/closedir/telldir/seekdir/rewinddir) are NOT
** provided.  This header only defines the cart's per-file directory entry
** (struct dirent) and the zero-page FileEntry that the runtime populates.
*/



/* End of dirent.h */
#endif
