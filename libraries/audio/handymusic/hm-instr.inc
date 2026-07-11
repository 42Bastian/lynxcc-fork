;
; HandyMusic instrument / SFX script decode commands (ca65 port).
; Included by handymusic.s (in the CODE segment).  Original by Osman Celimli.
;
; Command set:
;   0 Stop        [0]
;   1 Wait        [1][frames]
;   2 Set SFB     [2][shiftLo][shiftHi][fbLo][fbHi+integrate]
;   3 Set Volume  [3][vol][volAdj][volAdjDec]
;   4 Set Freq    [4][offLo][offHi][offAdjLo][offAdjHi][offAdjDec]
;   5 Set Loop    [5][count]   (negative == infinite)
;   6 Loop        [6]          (up to four deep)
;

HandyMusic_SFX_CommandTableLo:
        .byte   <HandyMusic_FreeChannel,<HandyMusic_SFX_Wait,<HandyMusic_SFX_SetSFB
        .byte   <HandyMusic_SFX_SetVolume,<HandyMusic_SFX_SetFrequency
        .byte   <HandyMusic_SFX_SetLoop,<HandyMusic_SFX_Loop
HandyMusic_SFX_CommandTableHi:
        .byte   >HandyMusic_FreeChannel,>HandyMusic_SFX_Wait,>HandyMusic_SFX_SetSFB
        .byte   >HandyMusic_SFX_SetVolume,>HandyMusic_SFX_SetFrequency
        .byte   >HandyMusic_SFX_SetLoop,>HandyMusic_SFX_Loop

; ---------------------------------------------------------------------------
; HandyMusic_SFX_GetBytes: next script byte -> A.
;
HandyMusic_SFX_GetBytes:
        lda     (HandyMusic_Channel_DecodePointer)
        inc     HandyMusic_Channel_DecodePointer
        bne     @norollover
        inc     HandyMusic_Channel_DecodePointer + 1
@norollover:
        rts

; ---------------------------------------------------------------------------
; HandyMusic_SFX_Wait: set a decode delay.
;
HandyMusic_SFX_Wait:
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_DecodeDelay,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_SFX_SetSFB: set shift + feedback registers (channel off until the
; envelope pass turns it back on).
;
HandyMusic_SFX_SetSFB:
        ldy     HandyMusic_Redirect_ChOffs,x
        lda     #0
        sta     Lynx_Audio_TimerCont,y          ; Disable counter
        dec     a
        sta     HandyMusic_Channel_ForceUpd,x   ; Force update channel

        jsr     HandyMusic_SFX_GetBytes
        sta     Lynx_Audio_ShiftRegLo,y         ; Low bits of shift register
        jsr     HandyMusic_SFX_GetBytes
        sta     Lynx_Audio_AudioExtra,y         ; High bits of shift register
        jsr     HandyMusic_SFX_GetBytes
        sta     Lynx_Audio_FeedBackReg,y        ; Feedback bits
        jsr     HandyMusic_SFX_GetBytes
        sta     Lynx_Audio_TimerCont,y          ; Feedback bit 7 + integrate
        rts

; ---------------------------------------------------------------------------
; HandyMusic_SFX_SetVolume: set volume + volume envelope.
;
HandyMusic_SFX_SetVolume:
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_Volume,x
        stz     HandyMusic_Channel_VolumeDec,x  ; Copy volume, clear decimal

        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_VolumeAdjust,x
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_VolumeAdjustDec,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_SFX_SetFrequency: set frequency offset + frequency envelope.
;
HandyMusic_SFX_SetFrequency:
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_FreqOffsetLo,x
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_FreqOffsetHi,x
        stz     HandyMusic_Channel_FreqOffsetDec,x ; Copy freq, clear decimal

        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_OffsetPitAdjLo,x
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_OffsetPitAdjHi,x
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_OffsetPitAdjDec,x

        lda     #$FF
        sta     HandyMusic_Channel_ForceUpd,x   ; Force update channel
        rts

; ---------------------------------------------------------------------------
; HandyMusic_SFX_SetLoop: mark a loop point and copy the loop count.
;
HandyMusic_SFX_SetLoop:
        inc     HandyMusic_Channel_LoopDepth,x  ; Increase loop depth
        clc
        lda     HandyMusic_Channel_LoopAddrDepth,x ; Depth into loop arrays
        adc     HandyMusic_Channel_LoopDepth,x
        tay
        jsr     HandyMusic_SFX_GetBytes         ; Number of times to loop
        sta     HandyMusic_Channel_LoopCount-1,y
        lda     HandyMusic_Channel_DecodePointer
        sta     HandyMusic_Channel_LoopAddrLo-1,y
        lda     HandyMusic_Channel_DecodePointer + 1
        sta     HandyMusic_Channel_LoopAddrHi-1,y
        rts

; ---------------------------------------------------------------------------
; HandyMusic_SFX_Loop: loop back if the loop condition is still valid.
;
HandyMusic_SFX_Loop:
        clc
        lda     HandyMusic_Channel_LoopAddrDepth,x
        adc     HandyMusic_Channel_LoopDepth,x
        tay
        lda     HandyMusic_Channel_LoopCount-1,y ; Negative == infinite
        bmi     @loop
        dec     a
        sta     HandyMusic_Channel_LoopCount-1,y ; Decrement; zero == fail
        beq     @noloop
@loop:
        lda     HandyMusic_Channel_LoopAddrLo-1,y
        sta     HandyMusic_Channel_DecodePointer
        lda     HandyMusic_Channel_LoopAddrHi-1,y
        sta     HandyMusic_Channel_DecodePointer + 1
        rts
@noloop:
        dec     HandyMusic_Channel_LoopDepth,x
        rts
