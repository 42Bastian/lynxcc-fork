;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_splash - noise burst + soft low tail
;
        .include "lynx/sfx.inc"

        SFX_BEGIN splash
        SFX_WAVE  NOISE
        SFX_VOL   75
        SFX_SWEEP down, 5, 20
        SFX_DUR   30
        SFX_DECAY 4, 36
        SFX_NOTE  45
        SFX_END
