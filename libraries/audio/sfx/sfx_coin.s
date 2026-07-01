;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_coin - bright two-note pickup bling, short metallic fade
;
        .include "lynx/sfx.inc"

        SFX_BEGIN coin
        SFX_WAVE  SQ
        SFX_VOL   90
        SFX_DUR   4
        SFX_DECAY 5, 12
        SFX_ARP   72, 79
        SFX_END
