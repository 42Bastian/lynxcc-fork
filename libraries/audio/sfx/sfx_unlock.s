;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_unlock - lock-release click then clunk
;
        .include "lynx/sfx.inc"

        SFX_BEGIN unlock
        SFX_WAVE  METAL
        SFX_VOL   75
        SFX_DUR   10
        SFX_DECAY 4, 30
        SFX_ARP   55, 48
        SFX_END
