;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_water_drop - rounded up-plink, brief decay
;
        .include "lynx/sfx.inc"

        SFX_BEGIN water_drop
        SFX_WAVE  TRI
        SFX_VOL   70
        SFX_SWEEP up, 10, 10
        SFX_DUR   12
        SFX_DECAY 6, 14
        SFX_NOTE  60
        SFX_END
