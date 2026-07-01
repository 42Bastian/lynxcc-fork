;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_key_pickup - tiny metallic jingle
;
        .include "lynx/sfx.inc"

        SFX_BEGIN key_pickup
        SFX_WAVE  METAL
        SFX_VOL   70
        SFX_DUR   5
        SFX_DECAY 4, 18
        SFX_ARP   72, 79, 84
        SFX_END
