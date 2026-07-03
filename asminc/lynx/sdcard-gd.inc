;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; asminc/lynx/sdcard-gd.inc - register and command equates for the RetroHQ
; Lynx SD / GD flash cartridge MCU protocol (design/LYNX_SDCARD_GD_API_DESIGN.md,
; libraries/sdcard-gd/lynx-sdcard-gd.c).
;
; Communication is a byte FIFO across two cartridge data ports, gated by bit 4
; of the Mikey IODAT register. Multi-byte values are little-endian; path
; strings are NUL-terminated.
;

; --------------------------------------------------------------------------
; Registers (mirrors of the equates in asminc/lynx/lynx.inc)

SDGD_IODIR      = $FD8A         ; Mikey I/O direction (set 0 = all input)
SDGD_IODAT      = $FD8B         ; Mikey I/O data; bit 4 is the FIFO ready/busy flag
SDGD_CART0      = $FCB2         ; cartridge bank 0 data port - FIFO read side
SDGD_CART1      = $FCB3         ; cartridge bank 1 data port - FIFO write side

SDGD_AUXMASK    = $10           ; IODAT bit 4: set = MCU busy / no data yet
SDGD_MAGIC      = $AA           ; written to CART1 by init to wake MCU comms

; --------------------------------------------------------------------------
; Command bytes (0-based; written first, before any argument bytes)

SDGD_CMD_OPENDIR    = 0
SDGD_CMD_READDIR    = 1
SDGD_CMD_OPENFILE   = 2
SDGD_CMD_GETSIZE    = 3
SDGD_CMD_SEEK       = 4
SDGD_CMD_READ       = 5
SDGD_CMD_WRITE      = 6
SDGD_CMD_CLOSE      = 7
SDGD_CMD_PROGRAM    = 8
SDGD_CMD_CLEAR      = 9
SDGD_CMD_LOWPOWER   = 10

; --------------------------------------------------------------------------
; ProgramFile / ClearBlocks large-card (512-block, A19-via-aux) modifiers

SDGD_PROG_512   = $10           ; OR into the ProgramFile block-size byte
SDGD_CLEAR_512  = $8000         ; OR into the ClearBlocks block-count word

; --------------------------------------------------------------------------
; FRESULT status codes

SDGD_FR_OK          = 0
SDGD_FR_DISK_ERR    = 1
SDGD_FR_NOT_READY   = 2
SDGD_FR_NO_FILE     = 3
SDGD_FR_NOT_OPENED  = 4
SDGD_FR_NOT_ENABLED = 5
SDGD_FR_NO_FILESYSTEM = 6

; --------------------------------------------------------------------------
; Directory-entry attribute bits (SFileInfo.fattrib)

SDGD_AM_RDO     = $01
SDGD_AM_HID     = $02
SDGD_AM_SYS     = $04
SDGD_AM_VOL     = $08
SDGD_AM_LFN     = $0F
SDGD_AM_DIR     = $10
SDGD_AM_ARC     = $20
SDGD_AM_MASK    = $3F
