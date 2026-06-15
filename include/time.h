/*****************************************************************************/
/*                                                                           */
/*                                  time.h                                   */
/*                                                                           */
/*                               Date and time                               */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2012 Ullrich von Bassewitz                                       */
/*               Roemerstrasse 52                                            */
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



#ifndef _TIME_H
#define _TIME_H



/* NULL pointer */
#ifndef _HAVE_NULL
#define NULL    0
#define _HAVE_NULL
#endif

/* size_t is needed */
#ifndef _HAVE_size_t
#define _HAVE_size_t
typedef unsigned size_t;
#endif

typedef unsigned long clock_t;

/* NOTE: The Lynx has no real-time clock, so this target intentionally omits
** wall-clock timekeeping. The calendar API (time_t, struct tm, struct timespec,
** time, gmtime/localtime/mktime, asctime/ctime/strftime, the timezone struct,
** and the POSIX clock_get/setres/time family) is therefore NOT provided. Only
** the free-running tick counter below is available. See doc/funcref.html.
*/



/* The Lynx clock-rate depends on the video scan-rate, so read it at run-time. */
extern clock_t _clk_tck (void);
#define CLK_TCK                 _clk_tck()
#define CLOCKS_PER_SEC          _clk_tck()



/* ISO C function prototypes */
clock_t clock (void);



/* End of time.h */

#endif



