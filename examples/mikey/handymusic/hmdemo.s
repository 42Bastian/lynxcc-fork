;
; SPDX-License-Identifier: MIT
;
; Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
; Provided under the MIT License; copy it into your own projects freely.
; See the LICENSE file in the examples directory.
;
; HandyMusic demo data glue.  The hmcc-compiled music (.mus) and SFX (.sfx)
; blobs are ordinary resident RODATA here; handymusic.c copies each to its
; reserved base (__HMMUS_START__ / __HMSFX_START__) at startup, because hmcc
; baked ABSOLUTE pointer tables into them for those fixed bases (design
; sec. 3.2 / 4.3).  The two length symbols are exported as absolute values so
; the C side can take &sym as the byte count.
;
        .export         _hm_music_data, _hm_sfx_data
        .export         _hm_music_len, _hm_sfx_len

        .rodata

_hm_music_data:
        .incbin "music.mus"
hm_music_end:

_hm_sfx_data:
        .incbin "sfx.sfx"
hm_sfx_end:

_hm_music_len = hm_music_end - _hm_music_data
_hm_sfx_len   = hm_sfx_end - _hm_sfx_data
