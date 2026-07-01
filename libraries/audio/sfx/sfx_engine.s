;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_engine - looping low buzz; loops until snd_stop_channel
;
        .include "lynx/sfx.inc"

        SFX_BEGIN engine
        SFX_WAVE  SQ
        SFX_VOL   65
        SFX_DUR   20
        SFX_LOOP_FOREVER
        SFX_NOTE  34
        SFX_LOOP_END
        SFX_END
