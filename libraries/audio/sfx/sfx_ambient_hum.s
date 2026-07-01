;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_ambient_hum - low drone; loops until snd_stop_channel
;
        .include "lynx/sfx.inc"

        SFX_BEGIN ambient_hum
        SFX_WAVE  TRI
        SFX_VOL   60
        SFX_DUR   40
        SFX_LOOP_FOREVER
        SFX_NOTE  30
        SFX_LOOP_END
        SFX_END
