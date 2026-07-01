;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_spawn - rising sweep + wave wobble
;
        .include "lynx/sfx.inc"

        SFX_BEGIN spawn
        SFX_WAVE  SQ
        SFX_VOL   75
        SFX_SWEEP up, 5, 24
        SFX_SHIMMER 4, 24
        SFX_DUR   24
        SFX_DECAY 3, 30
        SFX_NOTE  48
        SFX_END
