;*****************************************************************************/
;*                                                                           */
;*                                tgi-kernel.inc                             */
;*                                                                           */
;*            Internal constants for the Lynx static TGI library             */
;*                                                                           */
;* The loadable-driver TGI kernel is gone. TGI on the Lynx is a direct-call  */
;* static library; see LYNX_TGI_DESIGN.md for the rationale and layout.      */
;* Modules import/export their shared symbols explicitly so that ld65's      */
;* smart linking applies (no .global lists here; an unreferenced import in   */
;* an include would force-link every module).                                */
;*                                                                           */
;*****************************************************************************/


;------------------------------------------------------------------------------
; Display geometry. Compile-time facts of the one and only video mode.
; Keep in sync with the constants in include/tgi.h.

TGI_XRES        = 160
TGI_YRES        = 102
TGI_COLORCOUNT  = 16
TGI_PAGECOUNT   = 2
TGI_FONTWIDTH   = 8
TGI_FONTHEIGHT  = 8

;------------------------------------------------------------------------------
; Frame buffer and collision buffer addresses. These are coupled to the
; memory map in cfg/lynx.cfg (see LYNX_TGI_DESIGN.md section 6): each page
; is 8160 bytes, the collision buffer sits at TGI_COLLBUF_ADDR. If the
; linker config changes, these must change with it.

TGI_PAGE0_ADDR   = $E018
TGI_PAGE1_ADDR   = $C038
TGI_COLLBUF_ADDR = $A058

; End-of-buffer offsets used for flipped (left-handed) display scan-out.
; Mikey fetches 80 bytes/line in 4bpp mode but only 40 bytes/line in the
; (unverified, see design section 2.7) 2bpp mode.

TGI_FLIPOFFS_4BPP = 8159
TGI_FLIPOFFS_2BPP = 4079

;------------------------------------------------------------------------------
; Text constants. Vector fonts are gone; two bitmap fonts remain (8x8 and
; the compact 5x5), selectable with tgi_setfont.

TGI_FONT_BITMAP         = 0
TGI_FONT_COMPACT        = 1

TGI_TEXT_HORIZONTAL     = 0
TGI_TEXT_VERTICAL       = 1
