;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; multicart.s -- multicart_run(romNum): launch a bundled game ROM from a
; multicart menu. See include/lynx/multicart.h and
; design/LYNX_MULTICART_DESIGN.md.
;
; A multicart is a single .lnx that holds a menu program plus several game ROMs
; laid out by the ROM builder (lynxdir), with a game directory the runtime
; loader indexes by number. This routine performs the hand-off: it masks
; interrupts, stops the display timers, copies the relocatable runtime loader
; (runtime/lynx/multicartldr.s, committed as the _multicart_loader blob) to
; $0040, and jumps to it with the requested game number. The loader reads that
; game off the cartridge over the top of the menu and runs it, so multicart_run
; never returns.

        .include "lynx/lynx.inc"

        .import  _multicart_loader
        .import  multicart_loader_size
        .export  _multicart_run

; Low-memory address the loader is copied to and executed at. Fixed: the blob is
; linked for this address (cfg/lynx-multicartldr.cfg) and is position-dependent.
MULTICART_LOADER_ADDR = $0040

.segment "CODE"

; ---------------------------------------------------------------
; void __fastcall__ multicart_run (unsigned char romNum)
; romNum arrives in A (fastcall, single 8-bit argument). Does not return.
; ---------------------------------------------------------------
.proc   _multicart_run: near

        tax                     ; stash romNum in X across the teardown + copy
        sei                     ; the loader runs with interrupts masked

        ; Disable the display-timer interrupts (HBL = timer 0, VBL = timer 2) so a
        ; pending frame IRQ can't run menu code we are about to overwrite. The
        ; game's crt0 re-initialises the timers from scratch. This mirrors the
        ; hand-off the LynxJam 2024 multicart used on real hardware.
        lda     TIM0CTLA
        and     #$7f
        sta     TIM0CTLA
        lda     TIM2CTLA
        and     #$7f
        sta     TIM2CTLA

        ; Blank the palette for a clean hand-off: no garbage flash while the
        ; incoming game loads and before it installs its own palette.
        lda     #0
        ldy     #31
@pal:   sta     PALETTE,y
        dey
        bpl     @pal

        ; Copy the relocatable loader blob down to $0040. The blob is larger
        ; than 128 bytes, so an ascending count-up copy is used: a descending
        ; dey/bpl loop would treat any length >= 128 as already negative and
        ; copy nothing. multicart_loader_size is asserted below to fit one page.
        .assert multicart_loader_size <= $100, error, "multicart loader blob exceeds one page; widen the copy loop"
        ldy     #0
@cpy:   lda     _multicart_loader,y
        sta     MULTICART_LOADER_ADDR,y
        iny
        cpy     #<multicart_loader_size
        bne     @cpy

        ; Enter the loader with A = romNum.
        txa
        jmp     MULTICART_LOADER_ADDR

.endproc
