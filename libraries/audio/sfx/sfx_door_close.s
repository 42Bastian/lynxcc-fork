;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_door_close - solid low thunk
;
        .include "lynx/sfx.inc"

        SFX_BEGIN door_close
        SFX_WAVE  NOISE
        SFX_VOL   75
        SFX_DUR   20
        SFX_DECAY 5, 28
        SFX_NOTE  32
        SFX_END
