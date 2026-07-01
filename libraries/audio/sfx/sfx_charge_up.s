;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; sfx_charge_up - rising sweep that brightens; cap/stop on release
;
        .include "lynx/sfx.inc"

        SFX_BEGIN charge_up
        SFX_WAVE  SQ
        SFX_VOL   70
        SFX_SWELL 2, 60
        SFX_SWEEP up, 3, 60
        SFX_DUR   90
        SFX_NOTE  48
        SFX_END
