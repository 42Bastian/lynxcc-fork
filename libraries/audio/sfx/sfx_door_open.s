;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_door_open - click + short sliding rise
;
        .include "lynx/sfx.inc"

        SFX_BEGIN door_open
        SFX_WAVE  NOISE
        SFX_VOL   70
        SFX_SWEEP up, 4, 30
        SFX_DUR   30
        SFX_DECAY 3, 40
        SFX_NOTE  40
        SFX_END
