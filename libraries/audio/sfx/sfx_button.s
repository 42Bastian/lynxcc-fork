;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_button - very short high tick
;
        .include "lynx/sfx.inc"

        SFX_BEGIN button
        SFX_WAVE  SQ
        SFX_VOL   65
        SFX_DUR   4
        SFX_DECAY 12, 5
        SFX_NOTE  84
        SFX_END
