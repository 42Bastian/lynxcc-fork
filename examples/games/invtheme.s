;
; SPDX-License-Identifier: MIT
;
; Lynx Game Development SDK example asset, (c) 2026 the lynxcc authors.
; Provided under the MIT License; copy it into your own projects freely.
; See the LICENSE file in this directory.
;
; invtheme.s - the Space Invaders in-game theme for invaders.c.
;
; This is the actual two-voice theme extracted from the reference cartridge's
; compiled event streams and relocated for lynxcc.  The lynxcc snd engine is a
; direct descendant of the 42Bastian "new_bll" rsound player, so the stream
; format carries over unchanged: 2-byte [pitch, duration] notes (volume from the
; instrument), 6-byte SetInstr, pointer-based DefEnvVol, and CallPattern/RetToSong
; shared phrases.  The two voices (theme_a, theme_b) play interlocking 8-note
; patterns in opposite phase; invaders.c plays them on two channels.  Each stream
; loops by calling its own entry (as in the original).
;
;   Voice A: feedback 1, max volume 120   (lead)
;   Voice B: feedback 4, max volume 40     (counter-voice)
;   Both share volume envelope 1: attack +20x5, then decay -10x10 and -4x10.
;
        .export _invaders_theme_a
        .export _invaders_theme_b
        .segment "RODATA"

SndEnv1:
        .byte $02, $03, $05, $14, $0A, $F6, $0A, $FC   ; loop 2, 3 parts

; --- shared note patterns (8 notes each, [pitch,duration]) ---
pat_D299:
        .byte 8,30, 20,30, 8,30, 20,30, 20,30, 20,30, 8,30, 8,30
        .byte $87                       ; RetToSong
pat_D2AA:
        .byte 4,30, 16,30, 4,30, 16,30, 16,30, 16,30, 4,30, 4,30
        .byte $87                       ; RetToSong
pat_D2BB:
        .byte 6,30, 18,30, 6,30, 18,30, 18,30, 18,30, 6,30, 6,30
        .byte $87                       ; RetToSong
pat_D2CC:
        .byte 13,30, 25,30, 13,30, 25,30, 25,30, 25,30, 13,30, 13,30
        .byte $87                       ; RetToSong
pat_D2DD:
        .byte 15,30, 27,30, 15,30, 27,30, 27,30, 27,30, 15,30, 15,30
        .byte $87                       ; RetToSong
pat_D39A:
        .byte 20,30, 8,30, 20,30, 8,30, 8,30, 8,30, 20,30, 20,30
        .byte $87                       ; RetToSong
pat_D3AB:
        .byte 18,30, 6,30, 18,30, 6,30, 6,30, 6,30, 18,30, 18,30
        .byte $87                       ; RetToSong
pat_D3BC:
        .byte 25,30, 13,30, 25,30, 13,30, 13,30, 13,30, 25,30, 25,30
        .byte $87                       ; RetToSong
pat_D3CD:
        .byte 27,30, 15,30, 27,30, 15,30, 15,30, 15,30, 27,30, 27,30
        .byte $87                       ; RetToSong
pat_D3DE:
        .byte 32,30, 20,30, 32,30, 20,30, 20,30, 20,30, 32,30, 32,30
        .byte $87                       ; RetToSong

; --- Voice A ---
_invaders_theme_a:
        .byte $84, 0, 0, 1, 0, 120   ; SetInstr fb=1 vol=0 max=120
        .byte $88, 1, <SndEnv1, >SndEnv1   ; DefEnvVol 1
        .byte $89, 1                    ; SetEnvVol 1
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D299, >pat_D299
        .byte $81                       ; Do
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2CC, >pat_D2CC
        .byte $86, <pat_D2CC, >pat_D2CC
        .byte $86, <pat_D2DD, >pat_D2DD
        .byte $86, <pat_D2DD, >pat_D2DD
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2AA, >pat_D2AA
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2AA, >pat_D2AA
        .byte $80, 2                    ; Loop 2
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D299, >pat_D299
        .byte $81                       ; Do
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2CC, >pat_D2CC
        .byte $86, <pat_D2CC, >pat_D2CC
        .byte $86, <pat_D2DD, >pat_D2DD
        .byte $86, <pat_D2DD, >pat_D2DD
        .byte $80, 4                    ; Loop 4
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2AA, >pat_D2AA
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $81                       ; Do
        .byte $80, 9                    ; Loop 9
        .byte $86, <pat_D299, >pat_D299
        .byte $81                       ; Do
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D299, >pat_D299
        .byte $81                       ; Do
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2CC, >pat_D2CC
        .byte $86, <pat_D2CC, >pat_D2CC
        .byte $86, <pat_D2DD, >pat_D2DD
        .byte $86, <pat_D2DD, >pat_D2DD
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2AA, >pat_D2AA
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D2BB, >pat_D2BB
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <pat_D2AA, >pat_D2AA
        .byte $81                       ; Do
        .byte $86, <pat_D299, >pat_D299
        .byte $86, <_invaders_theme_a, >_invaders_theme_a   ; loop: call own entry

; --- Voice B ---
_invaders_theme_b:
        .byte $84, 0, 0, 4, 0, 40   ; SetInstr fb=4 vol=0 max=40
        .byte $89, 1                    ; SetEnvVol 1
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D39A, >pat_D39A
        .byte $81                       ; Do
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3BC, >pat_D3BC
        .byte $86, <pat_D3BC, >pat_D3BC
        .byte $86, <pat_D3CD, >pat_D3CD
        .byte $86, <pat_D3CD, >pat_D3CD
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3DE, >pat_D3DE
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3DE, >pat_D3DE
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $80, 2                    ; Loop 2
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $81                       ; Do
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3BC, >pat_D3BC
        .byte $86, <pat_D3BC, >pat_D3BC
        .byte $86, <pat_D3CD, >pat_D3CD
        .byte $86, <pat_D3CD, >pat_D3CD
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3DE, >pat_D3DE
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3DE, >pat_D3DE
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $81                       ; Do
        .byte $80, 11                    ; Loop 11
        .byte $86, <pat_D39A, >pat_D39A
        .byte $81                       ; Do
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D39A, >pat_D39A
        .byte $81                       ; Do
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3BC, >pat_D3BC
        .byte $86, <pat_D3BC, >pat_D3BC
        .byte $86, <pat_D3CD, >pat_D3CD
        .byte $86, <pat_D3CD, >pat_D3CD
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3DE, >pat_D3DE
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $80, 3                    ; Loop 3
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <pat_D3DE, >pat_D3DE
        .byte $86, <pat_D3AB, >pat_D3AB
        .byte $81                       ; Do
        .byte $86, <pat_D39A, >pat_D39A
        .byte $86, <_invaders_theme_b, >_invaders_theme_b   ; loop: call own entry

