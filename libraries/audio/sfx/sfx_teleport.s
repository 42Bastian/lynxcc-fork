;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_teleport - rise/fall sweep + shimmer
;
        .include "lynx/sfx.inc"

        SFX_BEGIN teleport
        SFX_WAVE  TRI
        SFX_VOL   75
        SFX_SWEEP up, 6, 20
        SFX_SHIMMER 4, 40
        SFX_DUR   40
        SFX_DECAY 2, 48
        SFX_NOTE  55
        SFX_END
