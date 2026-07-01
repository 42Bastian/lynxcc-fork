;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_cursor_move - very short soft blip
;
        .include "lynx/sfx.inc"

        SFX_BEGIN cursor_move
        SFX_WAVE  SQ
        SFX_VOL   60
        SFX_DUR   4
        SFX_DECAY 10, 5
        SFX_NOTE  72
        SFX_END
