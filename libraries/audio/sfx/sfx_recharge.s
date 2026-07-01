;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_recharge - series of rising ticks
;
        .include "lynx/sfx.inc"

        SFX_BEGIN recharge
        SFX_WAVE  SQ
        SFX_VOL   75
        SFX_DUR   4
        SFX_DECAY 3, 12
        SFX_ARP   60, 63, 66, 69, 72
        SFX_END
