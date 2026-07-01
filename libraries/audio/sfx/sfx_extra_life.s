;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_extra_life - rising arpeggio ending on a sustained high note
;
        .include "lynx/sfx.inc"

        SFX_BEGIN extra_life
        SFX_WAVE  SQ
        SFX_VOL   90
        SFX_DUR   6
        SFX_DECAY 1, 70
        SFX_ARP   60, 64, 67, 72, 76
        SFX_DUR   28
        SFX_NOTE  79
        SFX_END
