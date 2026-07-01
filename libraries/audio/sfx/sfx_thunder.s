;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_thunder - sharp crack then fading rumble
;
        .include "lynx/sfx.inc"

        SFX_BEGIN thunder
        SFX_WAVE  NOISE
        SFX_VOL   100
        SFX_DUR   12
        SFX_DECAY 1, 108
        SFX_NOTE  33
        SFX_END
