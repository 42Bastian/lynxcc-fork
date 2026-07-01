;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_fire - soft foosh + crackle
;
        .include "lynx/sfx.inc"

        SFX_BEGIN fire
        SFX_WAVE  NOISE
        SFX_VOL   65
        SFX_DUR   30
        SFX_DECAY 3, 36
        SFX_NOTE  42
        SFX_END
