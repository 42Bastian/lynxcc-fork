;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_power_up - energetic rising sweep + arpeggio
;
        .include "lynx/sfx.inc"

        SFX_BEGIN power_up
        SFX_WAVE  SQ
        SFX_VOL   85
        SFX_SWEEP up, 6, 24
        SFX_DUR   6
        SFX_DECAY 2, 30
        SFX_ARP   48, 55, 60, 67
        SFX_END
