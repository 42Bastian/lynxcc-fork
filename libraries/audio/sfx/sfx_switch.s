;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_switch - crisp mechanical click
;
        .include "lynx/sfx.inc"

        SFX_BEGIN switch
        SFX_WAVE  METAL
        SFX_VOL   75
        SFX_DUR   6
        SFX_DECAY 10, 8
        SFX_NOTE  60
        SFX_END
