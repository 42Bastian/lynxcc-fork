;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_wind - broadband noise, volume swell then fall
;
        .include "lynx/sfx.inc"

        SFX_BEGIN wind
        SFX_WAVE  NOISE
        SFX_VOL   60
        SFX_SWEEP up, 3, 30
        SFX_DUR   72
        SFX_DECAY 2, 72
        SFX_NOTE  40
        SFX_END
