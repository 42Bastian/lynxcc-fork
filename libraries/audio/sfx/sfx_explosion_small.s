;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_explosion_small - noise burst + low pop, quick decay
;
        .include "lynx/sfx.inc"

        SFX_BEGIN explosion_small
        SFX_WAVE  NOISE
        SFX_VOL   90
        SFX_SWEEP down, 4, 30
        SFX_DUR   30
        SFX_DECAY 4, 30
        SFX_NOTE  40
        SFX_END
