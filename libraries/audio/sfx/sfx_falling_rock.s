;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_falling_rock - multiple low impacts + rattle
;
        .include "lynx/sfx.inc"

        SFX_BEGIN falling_rock
        SFX_WAVE  NOISE
        SFX_VOL   80
        SFX_DUR   12
        SFX_DECAY 2, 60
        SFX_ARP   45, 40, 36, 42, 38
        SFX_END
