;
; HandyMusic music script decode commands (ca65 port).
; Included by handymusic.s (in the CODE segment).  Original by Osman Celimli.
;
; Command set:
;   0 Set Priority   [0][pri]
;   1 Set Panning    [1][pan]
;   2 Note On        [2][instr][baseLo][baseHi][delayLo]
;   3 Note Off       [3][delayLo]
;   4 Set Freq Adj   [4][adjLo][adjHi][adjDec]
;   5 Set Loop       [5][count]
;   6 Loop           [6]
;   7 Wait           [7][delayLo][delayHi]
;   8 Play Sample    [8][sampleNo]   (RAM sample from the registry; design §4.4)
;   9 Pattern Call   [9][addrLo][addrHi]
;   A Pattern Return [A]
;   B Short Note On  [B][baseLo][baseHi][delayLo]
;   C Pattern Break  [C]
;

HandyMusic_Mus_CommandTableLo:
        .byte   <HandyMusic_Mus_SetPri,<HandyMusic_Mus_SetPan
        .byte   <HandyMusic_Mus_NoteOn,<HandyMusic_Mus_NoteOff
        .byte   <HandyMusic_Mus_SetFreqAdj,<HandyMusic_Music_SetLoop
        .byte   <HandyMusic_Mus_Loop,<HandyMusic_Mus_Wait
        .byte   <HandyMusic_Mus_Sampl,<HandyMusic_Mus_Call
        .byte   <HandyMusic_Mus_Ret,<HandyMusic_Mus_SNoteOn
        .byte   <HandyMusic_Mus_Break
HandyMusic_Mus_CommandTableHi:
        .byte   >HandyMusic_Mus_SetPri,>HandyMusic_Mus_SetPan
        .byte   >HandyMusic_Mus_NoteOn,>HandyMusic_Mus_NoteOff
        .byte   >HandyMusic_Mus_SetFreqAdj,>HandyMusic_Music_SetLoop
        .byte   >HandyMusic_Mus_Loop,>HandyMusic_Mus_Wait
        .byte   >HandyMusic_Mus_Sampl,>HandyMusic_Mus_Call
        .byte   >HandyMusic_Mus_Ret,>HandyMusic_Mus_SNoteOn
        .byte   >HandyMusic_Mus_Break

; ---------------------------------------------------------------------------
; HandyMusic_Mus_GetBytes: next script byte -> A.
;
HandyMusic_Mus_GetBytes:
        lda     (HandyMusic_Music_DecodePointer)
        inc     HandyMusic_Music_DecodePointer
        bne     @norollover
        inc     HandyMusic_Music_DecodePointer + 1
@norollover:
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_SetPri: set track priority (0 disables the track).
;
HandyMusic_Mus_SetPri:
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_Priority,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_SetPan: set stereo panning (LLLLRRRR) for the next note on.
;
HandyMusic_Mus_SetPan:
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Channel_Panning,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_NoteOn / HandyMusic_Mus_SNoteOn: play a note on an instrument
; at a base frequency, then wait a one-byte delay.
;
HandyMusic_Mus_NoteOn:
        jsr     HandyMusic_Mus_GetBytes         ; Instrument #
        sta     HandyMusic_Music_LastInstrument,x
HandyMusic_Mus_SNoteOn:
        lda     HandyMusic_Music_LastInstrument,x
        tay
        lda     HandyMusic_Music_Priority,x     ; Still in control of channel?
        cmp     HandyMusic_Channel_Priority,x
        bcs     @playnote00
        jsr     HandyMusic_Mus_GetBytes         ; No: skip play, just delay
        jsr     HandyMusic_Mus_GetBytes
        bra     @playnote10
@playnote00:
        sta     HandyMusic_Channel_Priority,x   ; Channel pri = music pri
        stz     HandyMusic_Channel_LoopDepth,x
        stz     HandyMusic_Channel_DecodeDelay,x
        stz     HandyMusic_Channel_BaseFreqDec,x
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Channel_BaseFreqLo,x
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Channel_BaseFreqHi,x ; Copy passed frequency

        lda     HandyMusic_Music_BasePitAdjLo,x
        sta     HandyMusic_Channel_BasePitAdjLo,x
        lda     HandyMusic_Music_BasePitAdjHi,x
        sta     HandyMusic_Channel_BasePitAdjHi,x
        lda     HandyMusic_Music_BasePitAdjDec,x
        sta     HandyMusic_Channel_BasePitAdjDec,x

        lda     (HandyMusic_Instrument_AddrTableLoLo),y ; Script pointer
        sta     HandyMusic_Channel_DecodePointer
        lda     (HandyMusic_Instrument_AddrTableHiLo),y
        sta     HandyMusic_Channel_DecodePointer + 1

        jsr     HandyMusic_Enqueue_IS           ; Shared NoteOff/SFB/Vol/Freq

        lda     HandyMusic_Channel_NoWriteBack,x ; Writing disabled?
        bne     @playnote10
        lda     HandyMusic_Channel_Panning,x
        sta     Lynx_Audio_Atten_0,x            ; Set up panning
@playnote10:
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_DecodeDelayLo,x
        stz     HandyMusic_Music_DecodeDelayHi,x ; One-byte delay
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_NoteOff: force the current note into its note-off section (if
; still in control), then wait a one-byte delay.
;
HandyMusic_Mus_NoteOff:
        lda     HandyMusic_Music_Priority,x     ; Still in control of channel?
        cmp     HandyMusic_Channel_Priority,x
        bne     @instatedelay
        lda     HandyMusic_Channel_NoteOffPLo,x
        sta     HandyMusic_Channel_DecodePointerLo,x
        lda     HandyMusic_Channel_NoteOffPHi,x
        sta     HandyMusic_Channel_DecodePointerHi,x
        stz     HandyMusic_Channel_DecodeDelay,x ; Force decode
@instatedelay:
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_DecodeDelayLo,x
        stz     HandyMusic_Music_DecodeDelayHi,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_SetFreqAdj: set the track base-frequency adjustment (pitch
; bends etc.).
;
HandyMusic_Mus_SetFreqAdj:
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_BasePitAdjLo,x
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_BasePitAdjHi,x
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_BasePitAdjDec,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Music_SetLoop: mark a loop point and copy the loop count.
;
HandyMusic_Music_SetLoop:
        inc     HandyMusic_Music_LoopDepth,x
        clc
        lda     HandyMusic_Channel_LoopAddrDepth,x
        adc     HandyMusic_Music_LoopDepth,x
        tay
        jsr     HandyMusic_Mus_GetBytes
HandyMusic_Music_LoopPush:
        sta     HandyMusic_Music_LoopCount-1,y
        lda     HandyMusic_Music_DecodePointer
        sta     HandyMusic_Music_LoopAddrLo-1,y
        lda     HandyMusic_Music_DecodePointer + 1
        sta     HandyMusic_Music_LoopAddrHi-1,y
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_Loop: loop back if the loop condition is still valid.
;
HandyMusic_Mus_Loop:
        clc
        lda     HandyMusic_Channel_LoopAddrDepth,x
        adc     HandyMusic_Music_LoopDepth,x
        tay
        lda     HandyMusic_Music_LoopCount-1,y  ; Negative == infinite
        bmi     @loop
        dec     a
        sta     HandyMusic_Music_LoopCount-1,y
        beq     @noloop
@loop:
        lda     HandyMusic_Music_LoopAddrLo-1,y
        sta     HandyMusic_Music_DecodePointer
        lda     HandyMusic_Music_LoopAddrHi-1,y
        sta     HandyMusic_Music_DecodePointer + 1
        rts
@noloop:
        dec     HandyMusic_Music_LoopDepth,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_Wait: two-byte delay.
;
HandyMusic_Mus_Wait:
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_DecodeDelayLo,x
        jsr     HandyMusic_Mus_GetBytes
        sta     HandyMusic_Music_DecodeDelayHi,x
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_Sampl: grab channel 0 and play the RAM sample registered under
; the following number (design §4.4).  Script decoding continues while the
; sample streams, so delays are added by the script.  An unregistered or empty
; slot plays nothing.  X (the track index) is preserved.
;
HandyMusic_Mus_Sampl:
        phx
        jsr     HandyMusic_Mus_GetBytes         ; Sample number
        tax
        lda     HandyMusic_Sample_TblLenLo,x    ; Registered / non-empty?
        sta     HandyMusic_Sample_LenLo
        ora     HandyMusic_Sample_TblLenHi,x
        beq     @none
        lda     HandyMusic_Sample_TblLenHi,x
        sta     HandyMusic_Sample_LenHi
        lda     HandyMusic_Sample_TblPtrLo,x
        sta     HandyMusic_Sample_Pointer
        lda     HandyMusic_Sample_TblPtrHi,x
        sta     HandyMusic_Sample_Pointer + 1
        jsr     HandyMusic_PlayPCM
@none:
        plx
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_Call: "jsr" to a script address (logged as an infinite loop).
;
HandyMusic_Mus_Call:
        jsr     HandyMusic_Mus_GetBytes         ; Pattern address lo
        pha
        jsr     HandyMusic_Mus_GetBytes         ; Pattern address hi
        pha
        inc     HandyMusic_Music_LoopDepth,x
        clc
        lda     HandyMusic_Channel_LoopAddrDepth,x
        adc     HandyMusic_Music_LoopDepth,x
        tay
        lda     #$80
        jsr     HandyMusic_Music_LoopPush

        pla
        sta     HandyMusic_Music_DecodePointer + 1
        pla
        sta     HandyMusic_Music_DecodePointer
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_Ret: "rts" from the current script location.
;
HandyMusic_Mus_Ret:
        jsr     HandyMusic_Mus_Loop             ; "Loop" to return address
        dec     HandyMusic_Music_LoopDepth,x    ; Force depth to decrement
        rts

; ---------------------------------------------------------------------------
; HandyMusic_Mus_Break: return all channels to the main track (lowest CALL
; found, based on $80 tags).
;
HandyMusic_Mus_Break:
        jsr     HandyMusic_BackupDecodePointer  ; Temp pointer will get trashed
        phx
        ldx     #3
@checkNextTrack:
        clc
        lda     HandyMusic_Channel_LoopAddrDepth,x
        tay
        adc     HandyMusic_Music_LoopDepth,x
        sta     @checkLoopPoint + 1
@checkLoopPoint:
        cpy     #0                              ; Operand modified above
        beq     @trackDone
        lda     HandyMusic_Music_LoopCount,y
        iny
        cmp     #$80                            ; $80 tags a Call
        beq     @foundBase
        bra     @checkLoopPoint
@foundBase:
        tya
        sec
        sbc     HandyMusic_Channel_LoopAddrDepth,x
        sta     HandyMusic_Music_LoopDepth,x
        jsr     HandyMusic_Mus_Ret              ; Force RET on the lowest Call
        stz     HandyMusic_Music_DecodeDelayLo,x
        stz     HandyMusic_Music_DecodeDelayHi,x ; Proceed immediately
        jsr     HandyMusic_BackupDecodePointer
@trackDone:
        dex
        bpl     @checkNextTrack
        plx
        lda     HandyMusic_Music_DecodePointerLo,x
        sta     HandyMusic_Music_DecodePointer
        lda     HandyMusic_Music_DecodePointerHi,x
        sta     HandyMusic_Music_DecodePointer + 1
        rts
