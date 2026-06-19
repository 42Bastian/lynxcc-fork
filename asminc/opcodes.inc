; opcodes.inc
; ca65 6502 - opcode definitions, mainly for self modifying code
;
; Christian Krüger, latest change: 18-Sep-2010
;
; This software is provided 'as-is', without any expressed or implied      
; warranty.  In no event will the authors be held liable for any damages   
; arising from the use of this software.                                   
;                                                                          
; Permission is granted to anyone to use this software for any purpose,    
; including commercial applications, and to alter it and redistribute it   
; freely, subject to the following restrictions:                           
;                                                                          
; 1. The origin of this software must not be misrepresented; you must not  
;    claim that you wrote the original software. If you use this software  
;    in a product, an acknowledgment in the product documentation would be 
;    appreciated but is not required.                                      
; 2. Altered source versions must be plainly marked as such, and must not  
;    be misrepresented as being the original software.                     
; 3. This notice may not be removed or altered from any source             
;    distribution.                                                         
;                                                                          

; Opcode-Table
; ------------
; Post fix explanation:
; imm = #$00
; zp = $00
; zpx = $00,X
; zpy = $00,Y
; izp = ($00)
; izx = ($00,X)
; izy = ($00),Y
; abs = $0000
; abx = $0000,X
; aby = $0000,Y
; ind = ($0000)
; iax = ($0000,X)
; rel = $0000 (PC-relative) (supressed here)

.macpack        cpu

OPC_BRK     = $00
OPC_ORA_izx = $01
OPC_ORA_zp  = $05
OPC_ASL_zp  = $06
OPC_PHP     = $08
OPC_ORA_imm = $09
OPC_ASL     = $0A
OPC_ORA_abs = $0D
OPC_ASL_abs = $0E

OPC_BPL     = $10
OPC_ORA_izy = $11
OPC_ORA_zpx = $15
OPC_ASL_zpx = $16
OPC_CLC     = $18
OPC_ORA_aby = $19
OPC_ORA_abx = $1D
OPC_ASL_abx = $1E

OPC_JSR_abs = $20
OPC_AND_izx = $21
OPC_BIT_zp  = $24
OPC_AND_zp  = $25
OPC_ROL_zp  = $26
OPC_PLP     = $28
OPC_AND_imm = $29
OPC_ROL     = $2A
OPC_BIT_abs = $2C
OPC_AND_abs = $2D
OPC_ROL_abs = $2E

OPC_BMI     = $30
OPC_AND_izy = $31
OPC_AND_zpx = $35
OPC_ROL_zpx = $36
OPC_SEC     = $38
OPC_AND_aby = $39
OPC_AND_abx = $3D
OPC_ROL_abx = $3E


OPC_RTI     = $40
OPC_EOR_izx = $41
OPC_EOR_zp  = $45
OPC_LSR_zp  = $46
OPC_PHA     = $48
OPC_EOR_imm = $49
OPC_LSR     = $4A
OPC_JMP_abs = $4C
OPC_EOR_abs = $4D
OPC_LSR_abs = $4E

OPC_BVC     = $50
OPC_EOR_izy = $51
OPC_EOR_zpx = $55
OPC_LSR_zpx = $56
OPC_CLI     = $58
OPC_EOR_aby = $59
OPC_EOR_abx = $5D
OPC_LSR_abx = $5E

OPC_RTS     = $60
OPC_ADC_izx = $61
OPC_ADC_zp  = $65
OPC_ROR_zp  = $66
OPC_PLA     = $68
OPC_ADC_imm = $69
OPC_ROR     = $6A
OPC_JMP_ind = $6C
OPC_ADC_abs = $6D
OPC_ROR_abs = $6E

OPC_BVS     = $70
OPC_ADC_izy = $71
OPC_ADC_zpx = $75
OPC_ROR_zpx = $76
OPC_SEI     = $78
OPC_ADC_aby = $79
OPC_ADC_abx = $7D
OPC_ROR_abx = $7E

OPC_STA_izx = $81
OPC_STY_zp  = $84
OPC_STA_zp  = $85
OPC_STX_zp  = $86
OPC_DEY     = $88
OPC_TXA     = $8A
OPC_STY_abs = $8C
OPC_STA_abs = $8D
OPC_STX_abs = $8E

OPC_BCC     = $90
OPC_STA_izy = $91
OPC_STY_zpx = $94
OPC_STA_zpx = $95
OPC_STX_zpy = $96
OPC_TYA     = $98
OPC_STA_aby = $99
OPC_TXS     = $9A
OPC_STA_abx = $9D

OPC_LDY_imm = $A0
OPC_LDA_izx = $A1
OPC_LDX_imm = $A2
OPC_LDY_zp  = $A4
OPC_LDA_zp  = $A5
OPC_LDX_zp  = $A6
OPC_TAY     = $A8
OPC_LDA_imm = $A9
OPC_TAX     = $AA
OPC_LDY_abs = $AC
OPC_LDA_abs = $AD
OPC_LDX_abs = $AE

OPC_BCS     = $B0
OPC_LDA_izy = $B1
OPC_LDY_zpx = $B4
OPC_LDA_zpx = $B5
OPC_LDX_zpy = $B6
OPC_CLV     = $B8
OPC_LDA_aby = $B9
OPC_TSX     = $BA
OPC_LDY_abx = $BC
OPC_LDA_abx = $BD
OPC_LDX_aby = $BE

OPC_CPY_imm = $C0
OPC_CMP_izx = $C1
OPC_CPY_zp  = $C4
OPC_CMP_zp  = $C5
OPC_DEC_zp  = $C6
OPC_INY     = $C8
OPC_CMP_imm = $C9
OPC_DEX     = $CA
OPC_CPY_abs = $CC
OPC_CMP_abs = $CD
OPC_DEC_abs = $CE

OPC_BNE     = $D0
OPC_CMP_izy = $D1
OPC_CMP_zpx = $D5
OPC_DEC_zpx = $D6
OPC_CLD     = $D8
OPC_CMP_aby = $D9
OPC_CMP_abx = $DD
OPC_DEC_abx = $DE

OPC_CPX_imm = $E0
OPC_SBC_izx = $E1
OPC_CPX_zp  = $E4
OPC_SBC_zp  = $E5
OPC_INC_zp  = $E6
OPC_INX     = $E8
OPC_SBC_imm = $E9
OPC_NOP     = $EA
OPC_CPX_abs = $EC
OPC_SBC_abs = $ED
OPC_INC_abs = $EE


OPC_BEQ     = $F0
OPC_SBC_izy = $F1
OPC_SBC_zpx = $F5
OPC_INC_zpx = $F6
OPC_SED     = $F8
OPC_SBC_aby = $F9
OPC_SBC_abx = $FD
OPC_INC_abx = $FE


.if (.cpu .bitand ::CPU_ISET_65SC02)

; OPC_NOP   = $02               ; doublet
; OPC_NOP   = $03               ; doublet
OPC_TSB_zp  = $04
; OPC_NOP   = $0B               ; doublet
OPC_TSB_abs = $0C

OPC_ORA_izp = $12
; OPC_NOP   = $13               ; doublet
OPC_TRB_zp  = $14
OPC_INC     = $1A
; OPC_NOP   = $1B               ; doublet
OPC_TRB_abs = $1C

; OPC_NOP   = $22               ; doublet
; OPC_NOP   = $23               ; doublet
; OPC_NOP   = $2B               ; doublet

OPC_AND_izp = $32
; OPC_NOP   = $33               ; doublet
OPC_BIT_zpx = $34
OPC_DEC     = $3A
; OPC_NOP   = $3B               ; doublet
OPC_BIT_abx = $3C

; OPC_NOP   = $42               ; doublet
; OPC_NOP   = $43               ; doublet
; OPC_NOP   = $44               ; doublet
; OPC_NOP   = $4B               ; doublet

OPC_EOR_izp = $52
; OPC_NOP   = $53               ; doublet
; OPC_NOP   = $54               ; doublet
; OPC_NOP   = $5A               ; doublet
; OPC_NOP   = $5B               ; doublet

; OPC_NOP   = $62               ; doublet
; OPC_NOP   = $63               ; doublet
OPC_STZ_zp  = $64
; OPC_NOP   = $6B               ; doublet

OPC_ADC_izp = $72
; OPC_NOP   = $73               ; doublet
OPC_STZ_zpx = $74
OPC_PLY     = $7A
; OPC_NOP   = $7B               ; doublet
OPC_JMP_iax = $7C

OPC_BRA     = $80
; OPC_NOP   = $82               ; doublet
; OPC_NOP   = $83               ; doublet
OPC_BIT_imm = $89
; OPC_NOP   = $8B               ; doublet

OPC_STA_izp = $92
; OPC_NOP   = $93               ; doublet
; OPC_NOP   = $9B               ; doublet
OPC_STZ_abs = $9C
OPC_STZ_abx = $9E

; OPC_NOP   = $A3               ; doublet
; OPC_NOP   = $AB               ; doublet

OPC_LDA_izp = $B2
; OPC_NOP   = $B3               ; doublet
; OPC_NOP   = $BB               ; doublet

; OPC_NOP   = $C2               ; doublet
; OPC_NOP   = $C3               ; doublet
; OPC_NOP   = $CB               ; doublet

OPC_CMP_izp = $D2
; OPC_NOP   = $D3               ; doublet
; OPC_NOP   = $D4               ; doublet
OPC_PHX     = $DA
; OPC_NOP   = $DB               ; doublet
; OPC_NOP   = $DC               ; doublet

; OPC_NOP   = $E2               ; doublet
; OPC_NOP   = $E3               ; doublet
; OPC_NOP   = $EB               ; doublet

OPC_SBC_izp = $F2
; OPC_NOP   = $F3               ; doublet
; OPC_NOP   = $F4               ; doublet
OPC_PLX     = $FA
; OPC_NOP   = $FB               ; doublet
; OPC_NOP   = $FC               ; doublet


; The 65SC02 has no RMB/SMB/BBR/BBS bit instructions.

.endif
