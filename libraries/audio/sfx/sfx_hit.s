;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_hit - sharp click + noise crack
;
        .include "lynx/sfx.inc"

        SFX_BEGIN hit
        SFX_WAVE  NOISE
        SFX_VOL   85
        SFX_DUR   10
        SFX_DECAY 10, 10
        SFX_NOTE  45
        SFX_END
