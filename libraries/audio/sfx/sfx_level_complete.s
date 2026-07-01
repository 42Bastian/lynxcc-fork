;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_level_complete - triumphant multi-note fanfare
;
        .include "lynx/sfx.inc"

        SFX_BEGIN level_complete
        SFX_WAVE  SQ
        SFX_VOL   90
        SFX_DUR   6
        SFX_DECAY 1, 72
        SFX_ARP   60, 64, 67, 72, 76, 79
        SFX_DUR   24
        SFX_NOTE  84
        SFX_END
