;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_secret_found - mysterious arpeggio, unusual intervals
;
        .include "lynx/sfx.inc"

        SFX_BEGIN secret_found
        SFX_WAVE  SQ
        SFX_VOL   80
        SFX_DUR   8
        SFX_DECAY 2, 40
        SFX_ARP   61, 66, 73
        SFX_END
