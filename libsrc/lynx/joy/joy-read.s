;
; Static joypad read for the Atari Lynx (design/LYNX_JOY_SER_DESIGN.md section 2).
;
; unsigned __fastcall__ joy_read (unsigned char joystick);
;
; Returns the complete input state in one call:
;   bit 8   Pause                  (JOY_PAUSE_MASK,  from SWITCHES)
;   bit 7   Up                     (JOY_UP_MASK)
;   bit 6   Down                   (JOY_DOWN_MASK)
;   bit 5   Left                   (JOY_LEFT_MASK)
;   bit 4   Right                  (JOY_RIGHT_MASK)
;   bit 3   Option 1               (JOY_OPT1_MASK)
;   bit 2   Option 2               (JOY_OPT2_MASK)
;   bit 1   B (inner button)       (JOY_BTN_2_MASK / JOY_BTN_B_MASK)
;   bit 0   A (outer button)       (JOY_BTN_1_MASK / JOY_BTN_A_MASK)
;
; The joystick argument is ignored: the Lynx has exactly one joypad. JOYSTICK
; and SWITCHES are read-only Suzy registers; plain LDA is safe at any time.
; The Lynx hardware handles the left-handed (flipped) configuration itself,
; so no software direction swap is needed here.
;
; There is deliberately no edge-detect/debounce state here (the old conio
; kbhit machinery): at frame-rate polling the switches need no debouncing,
; and "pressed this frame" is one XOR in the caller:
;
;       now = joy_read (JOY_1);  pressed = now & ~prev;  prev = now;
;

        .export         _joy_read

        .include        "lynx.inc"

_joy_read:
        lda     SWITCHES        ; Pause switch, bit 0
        and     #$01
        tax                     ; -> bit 8 of the result
        lda     JOYSTICK        ; Up/Down/Left/Right, Opt1, Opt2, B, A
        rts
