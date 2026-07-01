;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_power_down - descending wobbling tone
;
        .include "lynx/sfx.inc"

        SFX_BEGIN power_down
        SFX_WAVE  TRI
        SFX_VOL   80
        SFX_SWEEP down, 5, 30
        SFX_DUR   36
        SFX_DECAY 2, 36
        SFX_NOTE  60
        SFX_END
