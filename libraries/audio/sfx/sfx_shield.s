;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_shield - rising shimmer to a sustained hum
;
        .include "lynx/sfx.inc"

        SFX_BEGIN shield
        SFX_WAVE  TRI
        SFX_VOL   75
        SFX_SWELL 2, 40
        SFX_SHIMMER 5, 40
        SFX_DUR   40
        SFX_NOTE  60
        SFX_END
