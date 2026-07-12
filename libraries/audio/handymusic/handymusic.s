;
; HandyMusic sound driver (ca65 port) for the Atari Lynx.
;
; Script-driven macro-instrument BGM + SFX engine.  Original driver by Osman
; Celimli (HandyMusic 1.40cx+), written for Bastian Schick's lyxass/BLL kit and
; fixed at $A000.  This is a ca65 port for the lynxcc SDK: syntax converted,
; the layout made relocatable, and the music-data region handed to the reserved
; HMMUS/HMSFX regions defined by cfg/lynx-handymusic.cfg (see
; design/LYNX_HANDYMUSIC_DESIGN.md).
;
; HandyMusic is completely free to use and modify in your own projects.
;   -- Osman Celimli.  See doc/licenses.html for the verbatim grant.
;
; Music, SFX and PCM sample playback.  PCM is RAM-sourced (design
; /LYNX_HANDYMUSIC_DESIGN.md §4.4): the upstream cart streamer is replaced with a
; plain pointer+length over a caller-supplied RAM buffer, fed to channel 0's
; direct-output register ($FD22) by a timer-3 8 kHz IRQ.  The music "play sample"
; command triggers a sample registered in the small RAM sample table.
;

        .include        "lynx/lynx.inc"

; ---------------------------------------------------------------------------
; Hardware register aliases.  Kept under the driver's original names so the
; ported code stays a faithful, minimally-changed copy of Celimli's source.
;
Lynx_Audio_Ch0          = 0
Lynx_Audio_Ch1          = 8
Lynx_Audio_Ch2          = 16
Lynx_Audio_Ch3          = 24

Lynx_Audio_Volume       = $FD20         ; AUD0VOL
Lynx_Audio_FeedBackReg  = $FD21         ; AUD0FEED
Lynx_Audio_DirectVol    = $FD22         ; AUD0OUT
Lynx_Audio_ShiftRegLo   = $FD23         ; AUD0SHIFT
Lynx_Audio_TimerBack    = $FD24         ; AUD0BKUP
Lynx_Audio_TimerCont    = $FD25         ; AUD0CTLA
Lynx_Audio_AudioExtra   = $FD27         ; AUD0CTLB

Lynx_Audio_Atten_0      = $FD40         ; attenuation ch0..3
Lynx_Audio_Panning      = $FD44         ; MPAN
Lynx_Audio_Stereo       = $FD50         ; MSTEREO

; ---------------------------------------------------------------------------
; Song header: read from the base of the reserved music-data region.  The game
; arranges the hmcc `.mus` blob at HMMUS_START (design §4.3), so the absolute
; pointers hmcc baked into the header land at these fixed offsets.
;
        .import         __HMMUS_START__

HandyMusic_Song_BaseAddress = __HMMUS_START__
HandyMusic_Song_Priorities  = HandyMusic_Song_BaseAddress + 0
HandyMusic_Song_TrackAddrLo = HandyMusic_Song_BaseAddress + 4
HandyMusic_Song_TrackAddrHi = HandyMusic_Song_BaseAddress + 8
HandyMusic_Song_InstrLoLo   = HandyMusic_Song_BaseAddress + 12
HandyMusic_Song_InstrLoHi   = HandyMusic_Song_BaseAddress + 13
HandyMusic_Song_InstrHiLo   = HandyMusic_Song_BaseAddress + 14
HandyMusic_Song_InstrHiHi   = HandyMusic_Song_BaseAddress + 15

; ---------------------------------------------------------------------------
; Exports: the public API the manual documents, plus the C-callable wrappers
; and the pre-init SFX table pointers (see include/lynx/handymusic.h).
;
        .export         HandyMusic_Init, HandyMusic_Main
        .export         HandyMusic_Pause, HandyMusic_UnPause
        .export         HandyMusic_StopAll
        .export         HandyMusic_PlayMusic, HandyMusic_StopMusic
        .export         HandyMusic_PlaySFX, HandyMusic_StopSoundEffect
        .export         HandyMusic_PlayPCM

        .export         _handymusic_init, _handymusic_main
        .export         _handymusic_play_music, _handymusic_stop_music
        .export         _handymusic_play_sfx, _handymusic_stop_sfx
        .export         _handymusic_stop_all
        .export         _handymusic_pause, _handymusic_unpause
        .export         _handymusic_play_pcm, _handymusic_register_pcm
        .export         _handymusic_pcm_playing
        .export         _handymusic_disable_pcm

        .exportzp       _handymusic_sfx_addr_lo
        .exportzp       _handymusic_sfx_addr_hi
        .exportzp       _handymusic_sfx_prio

        .import         popa, popax

; Number of RAM sample slots the music "play sample" command can trigger by
; number (design §4.4).  Kept small; the game registers buffers with
; handymusic_register_pcm before the song references them.
HANDYMUSIC_MAX_SAMPLES  = 8

; Install HandyMusic_Main on the VBL IRQ chain (design §4.5: the library owns
; its own 60 Hz hook; the game just calls init/play/stop).  A second interruptor
; services the timer-3 PCM stream (§4.4); it never claims the interrupt, so the
; VBL tick still runs on the same IRQ pass.
        .interruptor    handymusic_vbl_irq
        .interruptor    handymusic_pcm_irq

;****************************************************************************
;                          Zero-page variables
;****************************************************************************
; Only the pointers that MUST be zero page live here: the four indirect
; ((zp) / (zp),Y) decode pointers.  Everything else the original kept in ZP is
; only ever touched through absolute ,X / ,Y indexing and moves to the RAM
; block below (design §4.2, and the addressing audit in
; design/LYNX_HANDYMUSIC_DESIGN.md §8).
;
        .segment "APPZP" : zeropage

; Pre-init SFX script tables (three 16-bit pointers, contiguous).  Set by the
; game from the hmcc `.equ` addresses before the first HandyMusic_PlaySFX.
HandyMusic_SFX_AddressTableLoLo:
_handymusic_sfx_addr_lo:         .res 1
HandyMusic_SFX_AddressTableLoHi: .res 1
HandyMusic_SFX_AddressTableHiLo:
_handymusic_sfx_addr_hi:         .res 1
HandyMusic_SFX_AddressTableHiHi: .res 1
HandyMusic_SFX_AddressTablePriLo:
_handymusic_sfx_prio:            .res 1
HandyMusic_SFX_AddressTablePriHi: .res 1

; Instrument script tables (two 16-bit pointers, contiguous).  Loaded from the
; song header by HandyMusic_PlayMusic.
HandyMusic_Instrument_AddrTableLoLo: .res 1
HandyMusic_Instrument_AddrTableLoHi: .res 1
HandyMusic_Instrument_AddrTableHiLo: .res 1
HandyMusic_Instrument_AddrTableHiHi: .res 1

; The two shared decode work pointers.
HandyMusic_Channel_DecodePointer:    .res 2
HandyMusic_Music_DecodePointer:      .res 2

; PCM read cursor (design §4.4).  Must be zero page for the (zp) indirect fetch
; in the timer-3 IRQ; advanced one byte per sample.
HandyMusic_Sample_Pointer:           .res 2

;****************************************************************************
;                        RAM (BSS) variables
;****************************************************************************
        .bss

;***********************
; General/Flow Control *
;***********************
HandyMusic_Enable:              .res 1
HandyMusic_Active:              .res 1
HandyMusic_BGMPlaying:          .res 1

;***************
; Pause Backup *
;***************
HandyMusic_Pause_TimerBack:     .res 4
HandyMusic_Pause_VolumeBack:    .res 4

;***************
; For Channels *
;***************
HandyMusic_Channel_NoWriteBack: .res 4

HandyMusic_SFX_EnqueueNext:     .res 1
HandyMusic_SFX_PlayRequest:     .res 1

HandyMusic_Channel_Priority:    .res 4
HandyMusic_Channel_LoopDepth:   .res 4
HandyMusic_Channel_FinalFreqLo: .res 1
HandyMusic_Channel_FinalFreqHi: .res 1

;********************
; For Music Scripts *
;********************
HandyMusic_Music_Priority:      .res 4
HandyMusic_Music_LoopDepth:     .res 4

; The base frequency for each channel (16.8), set from the note.
HandyMusic_Channel_BaseFreqLo:  .res 4
HandyMusic_Channel_BaseFreqHi:  .res 4
HandyMusic_Channel_BaseFreqDec: .res 4

HandyMusic_Channel_BasePitAdjLo:  .res 4
HandyMusic_Channel_BasePitAdjHi:  .res 4
HandyMusic_Channel_BasePitAdjDec: .res 4

HandyMusic_Channel_FreqOffsetLo:  .res 4
HandyMusic_Channel_FreqOffsetHi:  .res 4
HandyMusic_Channel_FreqOffsetDec: .res 4

HandyMusic_Channel_OffsetPitAdjLo:  .res 4
HandyMusic_Channel_OffsetPitAdjHi:  .res 4
HandyMusic_Channel_OffsetPitAdjDec: .res 4

HandyMusic_Channel_LastFreqLo:  .res 4
HandyMusic_Channel_LastFreqHi:  .res 4

HandyMusic_Channel_ForceUpd:    .res 4

HandyMusic_Channel_Volume:      .res 4
HandyMusic_Channel_VolumeDec:   .res 4

HandyMusic_Channel_VolumeAdjust:    .res 4
HandyMusic_Channel_VolumeAdjustDec: .res 4

HandyMusic_Channel_Panning:     .res 4

HandyMusic_Channel_DecodePointerLo: .res 4
HandyMusic_Channel_DecodePointerHi: .res 4

HandyMusic_Channel_DecodeDelay: .res 4

HandyMusic_Channel_NoteOffPLo:  .res 4
HandyMusic_Channel_NoteOffPHi:  .res 4

HandyMusic_Channel_LoopAddrLo:  .res 16
HandyMusic_Channel_LoopAddrHi:  .res 16
HandyMusic_Channel_LoopCount:   .res 16

HandyMusic_Music_DecodePointerLo: .res 4
HandyMusic_Music_DecodePointerHi: .res 4

HandyMusic_Music_DecodeDelayLo: .res 4
HandyMusic_Music_DecodeDelayHi: .res 4

HandyMusic_Music_LoopAddrLo:    .res 16
HandyMusic_Music_LoopAddrHi:    .res 16
HandyMusic_Music_LoopCount:     .res 16

HandyMusic_Music_BasePitAdjLo:  .res 4
HandyMusic_Music_BasePitAdjHi:  .res 4
HandyMusic_Music_BasePitAdjDec: .res 4

HandyMusic_Music_LastInstrument: .res 4

;***********************
; PCM sample playback  *
;***********************
; Runtime mute for the PCM path (design §4.4): nonzero blocks all sample
; playback, both music-triggered and direct.  Zero (the BSS default) = enabled.
HandyMusic_Disable_Samples:
_handymusic_disable_pcm:        .res 1

HandyMusic_Sample_Playing:      .res 1  ; nonzero while a sample streams
HandyMusic_Sample_PanBackup:    .res 1  ; ch0 attenuation saved during playback
HandyMusic_Sample_LenLo:        .res 1  ; bytes remaining (16-bit down-counter)
HandyMusic_Sample_LenHi:        .res 1

; Sample registry: pointer + length per slot, indexed by sample number.  Filled
; by handymusic_register_pcm; read by the music "play sample" command.
HandyMusic_Sample_TblPtrLo:     .res HANDYMUSIC_MAX_SAMPLES
HandyMusic_Sample_TblPtrHi:     .res HANDYMUSIC_MAX_SAMPLES
HandyMusic_Sample_TblLenLo:     .res HANDYMUSIC_MAX_SAMPLES
HandyMusic_Sample_TblLenHi:     .res HANDYMUSIC_MAX_SAMPLES

; Scratch for the three-argument register wrapper.
HandyMusic_Reg_Ptr:             .res 2
HandyMusic_Reg_Len:             .res 2

;****************************************************************************
;                        Read-only constant tables
;****************************************************************************
        .rodata

; Loop-array base offsets per channel (4 slots each).
HandyMusic_Channel_LoopAddrDepth: .byte 0,4,8,12

; Audio-register channel offsets, indexed by X/Y for ,Y hardware access.
HandyMusic_Redirect_ChOffs:       .byte 0,8,16,24

;****************************************************************************
;                              Driver code
;****************************************************************************
        .code

; ---------------------------------------------------------------------------
; C-callable wrappers.  Single-char arguments arrive in A (cc65 fastcall),
; which is exactly what PlaySFX / StopSoundEffect expect.
;
_handymusic_init:               jmp HandyMusic_Init
_handymusic_main:               jmp HandyMusic_Main
_handymusic_play_music:         jmp HandyMusic_PlayMusic
_handymusic_stop_music:         jmp HandyMusic_StopMusic
_handymusic_stop_all:           jmp HandyMusic_StopAll
_handymusic_pause:              jmp HandyMusic_Pause
_handymusic_unpause:            jmp HandyMusic_UnPause
_handymusic_play_sfx:           jmp HandyMusic_PlaySFX
_handymusic_stop_sfx:           jmp HandyMusic_StopSoundEffect

; void __fastcall__ handymusic_play_pcm (const unsigned char *buf,
;                                        unsigned int len);
; len arrives in A/X (cc65 fastcall); buf is on the C parameter stack.
_handymusic_play_pcm:
        sta     HandyMusic_Sample_LenLo
        stx     HandyMusic_Sample_LenHi
        jsr     popax                           ; buf -> A/X
        sta     HandyMusic_Sample_Pointer
        stx     HandyMusic_Sample_Pointer + 1
        jmp     HandyMusic_PlayPCM

; void __fastcall__ handymusic_register_pcm (unsigned char num,
;                                            const unsigned char *buf,
;                                            unsigned int len);
; len in A/X; buf then num on the C parameter stack (last-pushed on top).
_handymusic_register_pcm:
        sta     HandyMusic_Reg_Len
        stx     HandyMusic_Reg_Len + 1
        jsr     popax                           ; buf -> A/X
        sta     HandyMusic_Reg_Ptr
        stx     HandyMusic_Reg_Ptr + 1
        jsr     popa                            ; num -> A
        tay
        lda     HandyMusic_Reg_Ptr
        sta     HandyMusic_Sample_TblPtrLo,y
        lda     HandyMusic_Reg_Ptr + 1
        sta     HandyMusic_Sample_TblPtrHi,y
        lda     HandyMusic_Reg_Len
        sta     HandyMusic_Sample_TblLenLo,y
        lda     HandyMusic_Reg_Len + 1
        sta     HandyMusic_Sample_TblLenHi,y
        rts

; unsigned char handymusic_pcm_playing (void);  -> nonzero while a sample plays.
_handymusic_pcm_playing:
        lda     HandyMusic_Sample_Playing
        ldx     #0
        rts

; ---------------------------------------------------------------------------
; handymusic_vbl_irq: interruptor.  Runs HandyMusic_Main once per VBL.  Never
; claims the interrupt (returns carry clear).
;
handymusic_vbl_irq:
        lda     INTSET
        and     #VBL_INTERRUPT
        beq     @done
        jsr     HandyMusic_Main
@done:  clc
        rts

; ---------------------------------------------------------------------------
; handymusic_pcm_irq: interruptor.  On each timer-3 tick (8 kHz) feeds the next
; sample byte to channel 0's direct-output register and advances the RAM cursor,
; stopping and handing channel 0 back to the music engine when the buffer is
; exhausted (design §4.4).  Never claims the interrupt (returns carry clear) so
; the VBL tick still runs on the same IRQ pass.
;
handymusic_pcm_irq:
        lda     HandyMusic_Sample_Playing
        beq     @done                           ; nothing streaming
        lda     INTSET
        and     #TIMER3_INTERRUPT
        beq     @done                           ; not our timer
        lda     (HandyMusic_Sample_Pointer)      ; next PCM byte (65SC02 (zp))
        sta     Lynx_Audio_DirectVol            ; -> ch0 direct output ($FD22)
        inc     HandyMusic_Sample_Pointer
        bne     @declen
        inc     HandyMusic_Sample_Pointer + 1
@declen:
        lda     HandyMusic_Sample_LenLo         ; 16-bit remaining-- with borrow
        bne     @declo
        dec     HandyMusic_Sample_LenHi
@declo:
        dec     HandyMusic_Sample_LenLo
        lda     HandyMusic_Sample_LenLo
        ora     HandyMusic_Sample_LenHi
        bne     @done                           ; more to play
        stz     TIM3CTLA                        ; buffer exhausted: kill timer 3
        stz     HandyMusic_Sample_Playing
        lda     HandyMusic_Sample_PanBackup
        sta     Lynx_Audio_Atten_0             ; restore ch0 attenuation
        stz     HandyMusic_Channel_NoWriteBack  ; give channel 0 back to music
@done:  clc
        rts

;****************************************************************
; HandyMusic_PlayPCM:                                          *
;   Start streaming the RAM buffer at HandyMusic_Sample_Pointer *
;   (HandyMusic_Sample_LenLo/Hi bytes) out channel 0 via the   *
;   timer-3 8 kHz IRQ.  Captures channel 0 from the music      *
;   engine; the IRQ hands it back when the buffer ends.        *
;   No-op if PCM is muted or the length is zero.  (design §4.4) *
;****************************************************************
HandyMusic_PlayPCM:
        sei
        lda     HandyMusic_Disable_Samples      ; muted?
        bne     @done
        lda     HandyMusic_Sample_LenLo         ; empty buffer?
        ora     HandyMusic_Sample_LenHi
        beq     @done
        lda     #$FF
        sta     HandyMusic_Channel_NoWriteBack  ; capture channel 0
        stz     Lynx_Audio_TimerCont            ; stop ch0 timer ($FD25)
        stz     TIM3CTLA                        ; disable T3 if already running
        lda     Lynx_Audio_Atten_0             ; back up + force ch0 attenuation
        sta     HandyMusic_Sample_PanBackup
        lda     #$FF
        sta     Lynx_Audio_Atten_0
        sta     HandyMusic_Sample_Playing       ; mark streaming
        lda     #125                            ; 125 us backup -> ~8 kHz
        sta     TIM3BKUP
        sta     TIM3CNT
        lda     #$D8                            ; reload + count + IRQ, 1 us clock
        sta     TIM3CTLA
@done:  cli
        rts

;****************************************************************
; HandyMusic_Main:                                             *
;   Process all music/sfx data and adjust the audio registers. *
;   Tied to VBL (60 Hz) by the interruptor above.              *
;****************************************************************
HandyMusic_Main:
        sei                                     ; Quickly disable IRQs

        lda     HandyMusic_Enable               ; Is HandyMusic enabled?
        bne     @CActive                        ; If not, bail.
        rts
@CActive:
        lda     HandyMusic_Active               ; Or already in a decode?
        beq     @Decode00
        rts
@Decode00:
        inc     HandyMusic_Active               ; We're decoding now.
        cli                                     ; Re-enable so we don't miss anything.

; Update all channels (SFX/Instrument processing)
        ldx     #3
@ChannelUpdates01:
        jsr     HandyMusic_UpdateChannel
        dex
        bpl     @ChannelUpdates01

; Check if a SFX was requested to be played
        lda     HandyMusic_SFX_PlayRequest
        beq     @SFXPlayCheck01
        stz     HandyMusic_SFX_PlayRequest
        jsr     HandyMusic_EnqueueSFX
@SFXPlayCheck01:

; Music update call
        lda     HandyMusic_BGMPlaying
        beq     @BGMPlayCheck10
        ldx     #3
@BGMPlayCheck01:
        jsr     HandyMusic_DecodeMusic
        dex
        bpl     @BGMPlayCheck01
@BGMPlayCheck10:

        stz     HandyMusic_Active               ; Done with decoding.
        rts

;****************************************************************
; HandyMusic_DecodeMusic:                                      *
;   Decodes the music script for the channel number in X.      *
;****************************************************************
HandyMusic_DecodeMusic:
        lda     HandyMusic_Music_Priority,x     ; is the channel enabled?
        beq     @return
        lda     HandyMusic_Music_DecodeDelayLo,x
        ora     HandyMusic_Music_DecodeDelayHi,x ; Any decoding delays?
        beq     @MusicDecode00
        lda     HandyMusic_Music_DecodeDelayHi,x
        bne     @hidelayactive
        dec     HandyMusic_Music_DecodeDelayLo,x
        bne     @return
        bra     @MusicDecode00
@hidelayactive:
        lda     HandyMusic_Music_DecodeDelayLo,x
        bne     @justdeclo
        dec     HandyMusic_Music_DecodeDelayHi,x
@justdeclo:
        dec     HandyMusic_Music_DecodeDelayLo,x
@return:
        rts
@MusicDecode00:
        lda     HandyMusic_Music_DecodePointerLo,x
        sta     HandyMusic_Music_DecodePointer
        lda     HandyMusic_Music_DecodePointerHi,x
        sta     HandyMusic_Music_DecodePointer + 1
@MusicDecode01:
        jsr     HandyMusic_Mus_GetBytes         ; Get current command byte
        tay
        lda     HandyMusic_Mus_CommandTableLo,y
        sta     @MusicDecode12 + 1
        lda     HandyMusic_Mus_CommandTableHi,y
        sta     @MusicDecode12 + 2
@MusicDecode12:
        jsr     $0000                           ; (destination overwritten)
        lda     HandyMusic_Music_Priority,x     ; is the channel enabled?
        beq     @return
        lda     HandyMusic_Music_DecodeDelayLo,x ; Delay happen?
        ora     HandyMusic_Music_DecodeDelayHi,x
        beq     @MusicDecode01                  ; If not, get next command
HandyMusic_BackupDecodePointer:
        lda     HandyMusic_Music_DecodePointer
        sta     HandyMusic_Music_DecodePointerLo,x
        lda     HandyMusic_Music_DecodePointer + 1
        sta     HandyMusic_Music_DecodePointerHi,x
        rts

;****************************************************************
; HandyMusic_EnqueueSFX:                                       *
;   Enqueue a sound effect in an open or lower-priority chan.  *
;****************************************************************
HandyMusic_EnqueueSFX:
        ldy     HandyMusic_SFX_EnqueueNext
        ldx     #3
@checkzeroes:
        lda     HandyMusic_Channel_Priority,x
        beq     @foundchannel
        dex
        bpl     @checkzeroes
        ldx     #3
        lda     (HandyMusic_SFX_AddressTablePriLo),y
@checkpriorities:
        cmp     HandyMusic_Channel_Priority,x
        bcs     @foundchannel
        dex
        bpl     @checkpriorities
        rts                                     ; No available channels
@foundchannel:
        lda     (HandyMusic_SFX_AddressTablePriLo),y ; Requested SFX priority
        sta     HandyMusic_Channel_Priority,x
        lda     #$FF
        sta     Lynx_Audio_Atten_0,x            ; Force center panning
        stz     HandyMusic_Channel_LoopDepth,x
        stz     HandyMusic_Channel_DecodeDelay,x

        stz     HandyMusic_Channel_BaseFreqDec,x ; Clear base freq / pitch adj
        stz     HandyMusic_Channel_BaseFreqLo,x  ; (instrument-only fields)
        stz     HandyMusic_Channel_BaseFreqHi,x
        stz     HandyMusic_Channel_BasePitAdjDec,x
        stz     HandyMusic_Channel_BasePitAdjLo,x
        stz     HandyMusic_Channel_BasePitAdjHi,x

        lda     (HandyMusic_SFX_AddressTableLoLo),y ; Copy script pointer
        sta     HandyMusic_Channel_DecodePointer
        lda     (HandyMusic_SFX_AddressTableHiLo),y
        sta     HandyMusic_Channel_DecodePointer + 1
HandyMusic_Enqueue_IS:
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_NoteOffPLo,x
        jsr     HandyMusic_SFX_GetBytes
        sta     HandyMusic_Channel_NoteOffPHi,x ; Copy note off pointer

        jsr     HandyMusic_SFX_SetSFB
        jsr     HandyMusic_SFX_SetVolume
        jsr     HandyMusic_SFX_SetFrequency     ; Shift, Feedback, Volume, Freq
        lda     HandyMusic_Channel_DecodePointer
        sta     HandyMusic_Channel_DecodePointerLo,x
        lda     HandyMusic_Channel_DecodePointer + 1
        sta     HandyMusic_Channel_DecodePointerHi,x
        rts

;****************************************************************
; HandyMusic_UpdateChannel:                                    *
;   Per-frame updates on channel X: script decode, frequency   *
;   and volume envelopes.  Skipped if priority is zero.        *
;****************************************************************
HandyMusic_UpdateChannel:
        lda     HandyMusic_Channel_Priority,x   ; is the channel enabled?
        bne     @doupdates00
@return:
        ldy     HandyMusic_Redirect_ChOffs,x
        sta     Lynx_Audio_TimerCont,y
        rts
@doupdates00:
        lda     HandyMusic_Channel_DecodeDelay,x ; Any decode delays active?
        beq     @doupdates10
        dec     a
        sta     HandyMusic_Channel_DecodeDelay,x
        bra     @processenvelopes
@doupdates10:
        lda     HandyMusic_Channel_DecodePointerLo,x
        sta     HandyMusic_Channel_DecodePointer
        lda     HandyMusic_Channel_DecodePointerHi,x
        sta     HandyMusic_Channel_DecodePointer + 1
@doupdates11:
        jsr     HandyMusic_SFX_GetBytes         ; Get current command byte
        tay
        lda     HandyMusic_SFX_CommandTableLo,y
        sta     @doupdates12 + 1
        lda     HandyMusic_SFX_CommandTableHi,y
        sta     @doupdates12 + 2
@doupdates12:
        jsr     $0000                           ; (destination overwritten)
        lda     HandyMusic_Channel_Priority,x   ; is the channel enabled?
        beq     @return
        lda     HandyMusic_Channel_DecodeDelay,x ; Delay happen?
        beq     @doupdates11                    ; If not, get next command

        lda     HandyMusic_Channel_DecodePointer
        sta     HandyMusic_Channel_DecodePointerLo,x
        lda     HandyMusic_Channel_DecodePointer + 1
        sta     HandyMusic_Channel_DecodePointerHi,x

@processenvelopes:
; Base frequency update
        clc
        lda     HandyMusic_Channel_BaseFreqDec,x
        adc     HandyMusic_Channel_BasePitAdjDec,x
        sta     HandyMusic_Channel_BaseFreqDec,x
        lda     HandyMusic_Channel_BaseFreqLo,x
        adc     HandyMusic_Channel_BasePitAdjLo,x
        sta     HandyMusic_Channel_BaseFreqLo,x
        lda     HandyMusic_Channel_BaseFreqHi,x
        adc     HandyMusic_Channel_BasePitAdjHi,x
        sta     HandyMusic_Channel_BaseFreqHi,x

; Frequency offset update
        clc
        lda     HandyMusic_Channel_FreqOffsetDec,x
        adc     HandyMusic_Channel_OffsetPitAdjDec,x
        sta     HandyMusic_Channel_FreqOffsetDec,x
        lda     HandyMusic_Channel_FreqOffsetLo,x
        adc     HandyMusic_Channel_OffsetPitAdjLo,x
        sta     HandyMusic_Channel_FreqOffsetLo,x
        lda     HandyMusic_Channel_FreqOffsetHi,x
        adc     HandyMusic_Channel_OffsetPitAdjHi,x
        sta     HandyMusic_Channel_FreqOffsetHi,x

; Volume update
        clc
        lda     HandyMusic_Channel_VolumeDec,x
        adc     HandyMusic_Channel_VolumeAdjustDec,x
        sta     HandyMusic_Channel_VolumeDec,x
        lda     HandyMusic_Channel_Volume,x
        adc     HandyMusic_Channel_VolumeAdjust,x
        sta     HandyMusic_Channel_Volume,x

; Now calculate the final frequency
        clc
        lda     HandyMusic_Channel_BaseFreqLo,x
        adc     HandyMusic_Channel_FreqOffsetLo,x
        sta     HandyMusic_Channel_FinalFreqLo
        lda     HandyMusic_Channel_BaseFreqHi,x
        adc     HandyMusic_Channel_FreqOffsetHi,x
        sta     HandyMusic_Channel_FinalFreqHi

        lda     HandyMusic_Channel_NoWriteBack,x ; Writing disabled?
        beq     @doWriteback
        rts
@doWriteback:
        ldy     HandyMusic_Redirect_ChOffs,x
        lda     HandyMusic_Channel_Volume,x
        sta     Lynx_Audio_Volume,y             ; Write volume value

        lda     HandyMusic_Channel_ForceUpd,x
        bne     @updatelastfreq
        lda     HandyMusic_Channel_FinalFreqLo
        cmp     HandyMusic_Channel_LastFreqLo,x
        bne     @updatelastfreq
        lda     HandyMusic_Channel_FinalFreqHi  ; Do we need to update the
        cmp     HandyMusic_Channel_LastFreqHi,x ; counter at all?
        bne     @updatelastfreq
        rts
@updatelastfreq:
        stz     HandyMusic_Channel_ForceUpd,x
        lda     HandyMusic_Channel_FinalFreqLo
        sta     HandyMusic_Channel_LastFreqLo,x
        lda     HandyMusic_Channel_FinalFreqHi
        sta     HandyMusic_Channel_LastFreqHi,x

        lda     Lynx_Audio_TimerCont,y          ; Y set from the volume write
        and     #%10100000                      ; Preserve Feedback & Integrate
        sta     Lynx_Audio_TimerCont,y          ; But shut off channel

        lda     HandyMusic_Channel_FinalFreqHi  ; Using a 1us prescale?
        and     #3
        bne     @not1usclock

        lda     HandyMusic_Channel_FinalFreqLo
        sta     Lynx_Audio_TimerBack,y          ; Straight frequency divider
        lda     Lynx_Audio_TimerCont,y
        ora     #%00011000
        sta     Lynx_Audio_TimerCont,y          ; Prescale + channel back on
        rts
@not1usclock:
        lda     HandyMusic_Channel_FinalFreqLo  ; Truncate to 7 bits if >1us
        ora     #$80
        sta     Lynx_Audio_TimerBack,y

        lda     HandyMusic_Channel_FinalFreqLo
        rol     a
        lda     HandyMusic_Channel_FinalFreqHi
        rol     a
        dec     a
        and     #7
        ora     Lynx_Audio_TimerCont,y
        ora     #%00011000
        sta     Lynx_Audio_TimerCont,y          ; Prescale + channel back on
        rts

;****************************************************************
; HandyMusic_PlaySFX:                                          *
;   Set a sound effect to play next frame.  Number in A.       *
;****************************************************************
HandyMusic_PlaySFX:
        phx
        phy
        tax
        lda     HandyMusic_SFX_PlayRequest
        beq     @SFXisOK
        ldy     HandyMusic_SFX_EnqueueNext
        lda     (HandyMusic_SFX_AddressTablePriLo),y
        phx
        ply
        cmp     (HandyMusic_SFX_AddressTablePriLo),y
        bcs     @BadPriority
@SFXisOK:
        stx     HandyMusic_SFX_EnqueueNext
        inc     HandyMusic_SFX_PlayRequest
@BadPriority:
        ply
        plx
        rts

;****************************************************************
; HandyMusic_StopSoundEffect:                                  *
;   Disable the first sound (or note!) whose priority matches  *
;   A.  X is preserved.                                        *
;****************************************************************
HandyMusic_StopSoundEffect:
        phx
        ldx     #3
@StopSFX00:
        sei
        cmp     HandyMusic_Channel_Priority,x
        bne     @StopSFX10
        jsr     HandyMusic_FreeChannel
        cli
        bra     @exit
@StopSFX10:
        cli
        dex
        bpl     @StopSFX00
@exit:
        plx
        rts

;****************************************************************
; HandyMusic_StopAll:                                          *
;   Stop all playing music tracks and sound effects.          *
;****************************************************************
HandyMusic_StopAll:
        jsr     HandyMusic_StopMusic
        phx
        ldx     #3
@Stopall0:
        sei
        jsr     HandyMusic_FreeChannel
        stz     HandyMusic_SFX_PlayRequest
        cli
        dex
        bpl     @Stopall0
        plx
        rts

;****************************************************************
; HandyMusic_PlayMusic:                                        *
;   Start playing the song resident in the reserved music      *
;   region.  Stop the current song before loading another.     *
;****************************************************************
HandyMusic_PlayMusic:
        phx
        ldx     #3
@PlayMusic00:
        lda     HandyMusic_Song_Priorities,x    ; Copy song header data
        sta     HandyMusic_Music_Priority,x
        lda     HandyMusic_Song_TrackAddrLo,x
        sta     HandyMusic_Music_DecodePointerLo,x
        lda     HandyMusic_Song_TrackAddrHi,x
        sta     HandyMusic_Music_DecodePointerHi,x
        stz     HandyMusic_Music_LoopDepth,x
        stz     HandyMusic_Music_DecodeDelayLo,x
        stz     HandyMusic_Music_DecodeDelayHi,x
        stz     HandyMusic_Music_BasePitAdjLo,x
        stz     HandyMusic_Music_BasePitAdjHi,x
        stz     HandyMusic_Music_BasePitAdjDec,x
        dex
        bpl     @PlayMusic00
        plx
        lda     HandyMusic_Song_InstrLoLo
        sta     HandyMusic_Instrument_AddrTableLoLo
        lda     HandyMusic_Song_InstrLoHi
        sta     HandyMusic_Instrument_AddrTableLoHi
        lda     HandyMusic_Song_InstrHiLo
        sta     HandyMusic_Instrument_AddrTableHiLo
        lda     HandyMusic_Song_InstrHiHi
        sta     HandyMusic_Instrument_AddrTableHiHi
        inc     HandyMusic_BGMPlaying            ; Turn on decoder
        rts

;****************************************************************
; HandyMusic_StopMusic:                                        *
;   Stop the current music track from decoding, freeing any    *
;   channels it was using.  A trashed, X preserved.            *
;****************************************************************
HandyMusic_StopMusic:
        stz     HandyMusic_BGMPlaying
        phx
        ldx     #3
@StopMusic00:
        sei
        lda     HandyMusic_Music_Priority,x     ; Identical priority == a note
        cmp     HandyMusic_Channel_Priority,x
        bne     @StopMusic10
        jsr     HandyMusic_FreeChannel
@StopMusic10:
        stz     HandyMusic_Music_Priority,x
        cli
        dex
        bpl     @StopMusic00
        plx
        rts

;****************************************************************
; HandyMusic_FreeChannel:                                      *
;   Free the channel in X for future notes/instruments.        *
;****************************************************************
HandyMusic_FreeChannel:
        stz     HandyMusic_Channel_Priority,x   ; Also stops decoding.

        ldy     HandyMusic_Redirect_ChOffs,x
        lda     #0
        sta     Lynx_Audio_TimerCont,y
        sta     Lynx_Audio_Volume,y
        sta     Lynx_Audio_DirectVol,y
        dec     a
        sta     Lynx_Audio_Atten_0,x            ; Reset panning to $FF
        rts

;****************************************************************
; HandyMusic_Pause:                                            *
;   Pause HandyMusic and mute all channels (double-pause safe).*
;****************************************************************
HandyMusic_Pause:
        lda     HandyMusic_Enable               ; Already disabled?
        beq     @return
        phx
        phy
        ldx     #3
        sei
        stz     HandyMusic_Enable
@backupregs:
        ldy     HandyMusic_Redirect_ChOffs,x
        lda     Lynx_Audio_TimerCont,y
        sta     HandyMusic_Pause_TimerBack,x    ; Back up + shut off timers
        lda     Lynx_Audio_Volume,y
        sta     HandyMusic_Pause_VolumeBack,x   ; Same for the volumes
        lda     #0
        sta     Lynx_Audio_Volume,y
        sta     Lynx_Audio_TimerCont,y
        dex
        bpl     @backupregs
        cli
        plx
        ply
@return:
        rts

;****************************************************************
; HandyMusic_UnPause:                                          *
;   Un-pause HandyMusic, restoring all channels (double-safe). *
;****************************************************************
HandyMusic_UnPause:
        lda     HandyMusic_Enable               ; Already enabled?
        bne     @return
        phx
        phy
        ldx     #3
        sei
@restoreregs:
        ldy     HandyMusic_Redirect_ChOffs,x
        lda     HandyMusic_Pause_VolumeBack,x
        sta     Lynx_Audio_Volume,y             ; Restore volumes
        lda     HandyMusic_Pause_TimerBack,x
        sta     Lynx_Audio_TimerCont,y          ; Restore timers
        dex
        bpl     @restoreregs
        inc     HandyMusic_Enable               ; HandyMusic + IRQs on
        stz     HandyMusic_SFX_PlayRequest      ; Kill SFX requests from pause
        cli
        plx
        ply
@return:
        rts

;****************************************************************
; HandyMusic_Init:                                             *
;   Initialize HandyMusic and the audio hardware: stereo on,   *
;   no channels active.                                        *
;****************************************************************
HandyMusic_Init:
        jsr     HandyMusic_StopAll              ; All channels open.
        lda     #$FF                            ; Enable attenuation on all chs
        sta     Lynx_Audio_Panning
        stz     Lynx_Audio_Stereo              ; Output in both L & R
        sta     HandyMusic_Enable              ; Enable HandyMusic.
        rts

; ---------------------------------------------------------------------------
; Instrument/SFX and music script decode command handlers.
;
        .include        "hm-instr.inc"
        .include        "hm-mus.inc"
