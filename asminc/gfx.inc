;*****************************************************************************/
;*                                                                           */
;*                                  gfx.inc                                  */
;*                                                                           */
;*              Internal constants for the Lynx graphics library             */
;*                                                                           */
;* The loadable-driver kernel is gone. Lynx graphics is a direct-call static */
;* library; see design/LYNX_GFX_DESIGN.md for the rationale and layout.      */
;* Modules import/export their shared symbols explicitly so that ld65's      */
;* smart linking applies (no .global lists here; an unreferenced import in   */
;* an include would force-link every module).                               */
;*                                                                           */
;*****************************************************************************/


;------------------------------------------------------------------------------
; Display geometry. Compile-time facts of the one and only video mode.
; Keep in sync with the constants in include/lynx/gfx.h.

GFX_XRES        = 160
GFX_YRES        = 102
GFX_COLORCOUNT  = 16
GFX_PAGECOUNT   = 2

;------------------------------------------------------------------------------
; Frame buffer and collision buffer addresses. These are coupled to the
; memory map in cfg/lynx.cfg (see design/LYNX_GFX_DESIGN.md section 6): each page
; is 8160 bytes, the collision buffer sits at GFX_COLLBUF_ADDR. If the
; linker config changes, these must change with it.

GFX_PAGE0_ADDR   = $E018
GFX_PAGE1_ADDR   = $C038
GFX_COLLBUF_ADDR = $A058

; End-of-buffer offsets used for flipped (left-handed) display scan-out.
; Mikey fetches 80 bytes/line in 4bpp mode but only 40 bytes/line in the
; (unverified, see design section 2.7) 2bpp mode.

GFX_FLIPOFFS_4BPP = 8159
GFX_FLIPOFFS_2BPP = 4079

;------------------------------------------------------------------------------
; Text constants. Vector fonts are gone; three bitmap fonts remain (8x8, the
; compact 5x5 and the proportional caps font), selectable with gfx_setfont.

GFX_FONT_BITMAP         = 0
GFX_FONT_COMPACT        = 1
GFX_FONT_VARIABLE       = 2

GFX_TEXT_HORIZONTAL     = 0
GFX_TEXT_VERTICAL       = 1
