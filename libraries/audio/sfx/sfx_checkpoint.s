;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_checkpoint - warm two-note chime, lingering decay
;
        .include "lynx/sfx.inc"

        SFX_BEGIN checkpoint
        SFX_WAVE  TRI
        SFX_VOL   80
        SFX_DUR   10
        SFX_DECAY 2, 40
        SFX_ARP   64, 71
        SFX_END
