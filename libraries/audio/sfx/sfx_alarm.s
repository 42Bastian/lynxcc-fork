;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_alarm - pulsing two-tone siren; loops until snd_stop_channel
;
        .include "lynx/sfx.inc"

        SFX_BEGIN alarm
        SFX_WAVE  SQ
        SFX_VOL   85
        SFX_DUR   14
        SFX_LOOP_FOREVER
        SFX_ARP   60, 52
        SFX_LOOP_END
        SFX_END
