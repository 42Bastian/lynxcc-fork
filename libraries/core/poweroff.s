; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; Soft power-off for the Atari Lynx.
;
; void poweroff (void);
;
; The Lynx voltage regulator has a "soft" on/off input driven by SYSCTL1
; (0xFD87) bit 1: 1 = system power on, 0 = power off. The bit powers up as 1
; at reset, so clearing it de-asserts the regulator and disconnects the system
; from the battery -- the same mechanism that auto-powers-off an idle unit.
; This is a one-way trip: control does not return to the caller on hardware.
;
; SYSCTL1 is write-only and bit 0 is the cartridge address strobe (shared with
; power), so we cannot read-modify-write it; we write the whole byte with both
; bits low. Interrupts are masked first so nothing can run between the store and
; the regulator actually cutting power.
;
; Emulators generally do not model the soft-off, so there the store is a no-op
; and execution would continue; the trailing spin keeps such a build from
; falling through into whatever follows in memory.
;

        .export         _poweroff

        .include        "lynx/lynx.inc"

_poweroff:
        sei                     ; no IRQ may countermand the power-down
        lda     #$00            ; SYSCTL1 bit 1 = power (1 = on); clear it -> off
        sta     SYSCTL1         ; regulator soft-off (also drops cart strobe)
@halt:  bra     @halt           ; hardware never gets here; spin on emulators
