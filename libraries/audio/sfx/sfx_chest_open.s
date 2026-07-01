;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_chest_open - wooden clunk + bright sparkle
;
        .include "lynx/sfx.inc"

        SFX_BEGIN chest_open
        SFX_WAVE  METAL
        SFX_VOL   75
        SFX_DUR   8
        SFX_DECAY 2, 50
        SFX_ARP   60, 67, 72, 79
        SFX_END
