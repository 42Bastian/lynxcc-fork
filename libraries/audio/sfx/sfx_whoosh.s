;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_whoosh - fast filtered-noise burst
;
        .include "lynx/sfx.inc"

        SFX_BEGIN whoosh
        SFX_WAVE  NOISE
        SFX_VOL   65
        SFX_SWEEP up, 6, 16
        SFX_DUR   18
        SFX_DECAY 4, 18
        SFX_NOTE  40
        SFX_END
