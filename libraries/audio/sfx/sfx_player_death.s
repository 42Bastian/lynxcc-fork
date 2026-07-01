;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_player_death - dramatic falling tone to silence
;
        .include "lynx/sfx.inc"

        SFX_BEGIN player_death
        SFX_WAVE  SQ
        SFX_VOL   85
        SFX_DUR   12
        SFX_DECAY 1, 84
        SFX_ARP   79, 72, 64, 55, 48
        SFX_END
