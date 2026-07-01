;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_attack - whoosh + slight pitch fall
;
        .include "lynx/sfx.inc"

        SFX_BEGIN attack
        SFX_WAVE  NOISE
        SFX_VOL   75
        SFX_SWEEP down, 6, 12
        SFX_DUR   14
        SFX_DECAY 6, 14
        SFX_NOTE  50
        SFX_END
