/*****************************************************************************/
/*                                                                           */
/*                                   tgi.h                                   */
/*                                                                           */
/*           DEPRECATED compatibility shim for the Lynx graphics API         */
/*                                                                           */
/*                                                                           */
/* The Lynx graphics API moved from the tgi_/TGI_ prefix to gfx_/GFX_. This  */
/* header exists only so that existing programs using the old names keep     */
/* compiling; it includes <lynx/gfx.h> and defines one preprocessor alias    */
/* per public symbol. The aliases add no code and no data and do not defeat  */
/* ld65 smart linking. Include <lynx/gfx.h> (or <lynx.h>) and use the gfx_   */
/* names in new code. These aliases will be removed in a future release.     */
/*                                                                           */
/* Define LYNX_NO_TGI_COMPAT before including this header to opt out early    */
/* and surface any lingering tgi_/TGI_ uses as errors.                       */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty. Use and redistribution are free for any purpose; see the        */
/* original cc65 license terms.                                              */
/*                                                                           */
/*****************************************************************************/



#ifndef _TGI_H
#define _TGI_H

#include <lynx/gfx.h>

#ifndef LYNX_NO_TGI_COMPAT

/* Functions */
#define tgi_init                gfx_init
#define tgi_clear               gfx_clear
#define tgi_clearrows           gfx_clearrows
#define tgi_sprite              gfx_sprite
#define tgi_flip                gfx_flip
#define tgi_setviewpage         gfx_setviewpage
#define tgi_setdrawpage         gfx_setdrawpage
#define tgi_busy                gfx_busy
#define tgi_updatedisplay       gfx_updatedisplay
#define tgi_setframerate        gfx_setframerate
#define tgi_setcollisiondetection gfx_setcollisiondetection
#define tgi_setbpp              gfx_setbpp
#define tgi_setcolor            gfx_setcolor
#define tgi_getcolor            gfx_getcolor
#define tgi_setbgcolor          gfx_setbgcolor
#define tgi_setpalette          gfx_setpalette
#define tgi_getpalette          gfx_getpalette
#define tgi_getdefpalette       gfx_getdefpalette
#define tgi_gotoxy              gfx_gotoxy
#define tgi_outtext             gfx_outtext
#define tgi_outtextxy           gfx_outtextxy
#define tgi_settextscale        gfx_settextscale
#define tgi_settextstyle        gfx_settextstyle
#define tgi_setfont             gfx_setfont
#define tgi_settextdir          gfx_settextdir
#define tgi_gettextwidth        gfx_gettextwidth
#define tgi_gettextheight       gfx_gettextheight

/* Zero-cost query macros. These alias function-like macros, so the alias must
** itself be function-like: cc65's preprocessor does not re-expand a bare
** object-like alias into the underlying gfx_getxxx() macro.
*/
#define tgi_getxres()           gfx_getxres()
#define tgi_getmaxx()           gfx_getmaxx()
#define tgi_getyres()           gfx_getyres()
#define tgi_getmaxy()           gfx_getmaxy()
#define tgi_getcolorcount()     gfx_getcolorcount()
#define tgi_getmaxcolor()       gfx_getmaxcolor()
#define tgi_getpagecount()      gfx_getpagecount()

/* Constants */
#define TGI_XRES                GFX_XRES
#define TGI_YRES                GFX_YRES
#define TGI_COLORCOUNT          GFX_COLORCOUNT
#define TGI_PAGECOUNT           GFX_PAGECOUNT
#define TGI_FONT_BITMAP         GFX_FONT_BITMAP
#define TGI_FONT_COMPACT        GFX_FONT_COMPACT
#define TGI_FONT_VARIABLE       GFX_FONT_VARIABLE
#define TGI_TEXT_HORIZONTAL     GFX_TEXT_HORIZONTAL
#define TGI_TEXT_VERTICAL       GFX_TEXT_VERTICAL

#endif /* LYNX_NO_TGI_COMPAT */



/* End of tgi.h */
#endif
