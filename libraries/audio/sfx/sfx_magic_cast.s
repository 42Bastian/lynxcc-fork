;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_magic_cast - shimmering sparkling arpeggio
;
        .include "lynx/sfx.inc"

        SFX_BEGIN magic_cast
        SFX_WAVE  TRI
        SFX_VOL   75
        SFX_SHIMMER 4, 40
        SFX_DUR   8
        SFX_DECAY 1, 50
        SFX_ARP   60, 64, 67, 72, 76
        SFX_END
