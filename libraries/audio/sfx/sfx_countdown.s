;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_countdown - single urgent square beep
;
        .include "lynx/sfx.inc"

        SFX_BEGIN countdown
        SFX_WAVE  SQ
        SFX_VOL   80
        SFX_DUR   12
        SFX_DECAY 4, 16
        SFX_NOTE  67
        SFX_END
