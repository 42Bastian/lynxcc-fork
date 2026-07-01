;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_menu_select - pleasant confirming chirp
;
        .include "lynx/sfx.inc"

        SFX_BEGIN menu_select
        SFX_WAVE  SQ
        SFX_VOL   75
        SFX_DUR   6
        SFX_DECAY 6, 10
        SFX_NOTE  72
        SFX_END
