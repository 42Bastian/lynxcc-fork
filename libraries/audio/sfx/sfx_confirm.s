;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_confirm - bright ascending chime
;
        .include "lynx/sfx.inc"

        SFX_BEGIN confirm
        SFX_WAVE  SQ
        SFX_VOL   85
        SFX_DUR   6
        SFX_DECAY 3, 20
        SFX_ARP   67, 74
        SFX_END
