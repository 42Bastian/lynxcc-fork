;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_double_jump - brighter, faster jump
;
        .include "lynx/sfx.inc"

        SFX_BEGIN double_jump
        SFX_WAVE  SQ
        SFX_VOL   80
        SFX_SWEEP up, 14, 7
        SFX_DUR   12
        SFX_DECAY 4, 12
        SFX_NOTE  64
        SFX_END
