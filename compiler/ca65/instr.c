/*****************************************************************************/
/*                                                                           */
/*                                  instr.c                                  */
/*                                                                           */
/*             Instruction encoding for the ca65 macroassembler              */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2012, Ullrich von Bassewitz                                      */
/*                Roemerstrasse 52                                           */
/*                D-70794 Filderstadt                                        */
/* EMail:         uz@cc65.org                                                */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/



#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* common */
#include "addrsize.h"
#include "attrib.h"
#include "bitops.h"
#include "check.h"
#include "mmodel.h"

/* ca65 */
#include "asserts.h"
#include "ea.h"
#include "ea65.h"
#include "error.h"
#include "expr.h"
#include "global.h"
#include "instr.h"
#include "nexttok.h"
#include "objcode.h"
#include "spool.h"
#include "studyexpr.h"
#include "symtab.h"



/*****************************************************************************/
/*                                 Forwards                                  */
/*****************************************************************************/



static void PutPCRel8 (const InsDesc* Ins);
/* Handle branches with a 8 bit distance */

/* Handle branches with an 16 bit distance and PER */

/* Handle branches with a 16 bit distance for 4510 */

/* Handle the blockmove instructions (65816) */

/* Handle the block transfer instructions (HuC6280) */

/* Handle 65C02 branch on bit condition */

/* Emit a REP instruction, track register sizes */

/* Emit a SEP instruction (65816), track register sizes */

/* Emit a TAMn instruction (HuC6280). Since this is a two byte instruction with
** implicit addressing mode, the opcode byte in the table is actually the
** second operand byte. The TAM instruction is the more generic form, it takes
** an immediate argument.
*/

/* Emit a TMA instruction (HuC6280) with an immediate argument. Only one bit
** in the argument byte may be set.
*/

/* Emit a TMAn instruction (HuC6280). Since this is a two byte instruction with
** implicit addressing mode, the opcode byte in the table is actually the
** second operand byte. The TAM instruction is the more generic form, it takes
** an immediate argument.
*/

/* Emit a TST instruction (HuC6280). */

static void PutJMP (const InsDesc* Ins);
/* Handle the jump instruction for the 6502. Problem is that these chips have
** a bug: If the address crosses a page, the upper byte gets not corrected and
** the instruction will fail. The PutJmp function will add a linker assertion
** to check for this case and is otherwise identical to PutAll.
*/

/* Handle the RTS instruction for the 816. In smart mode emit a RTL opcode if
** the enclosing scope is FAR.
*/

static void PutAll (const InsDesc* Ins);
/* Handle all other instructions */

/* Handle instructions of 4510 not matching any EATab */

/* Handle a generic sweet16 instruction */

/* Handle a sweet16 branch instruction */



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* Empty instruction table */
static const struct {
    unsigned Count;
} InsTabNone = {
    0
};

/* Instruction table for the 6502 */
static const struct {
    unsigned Count;
    InsDesc  Ins[56];
} InsTab6502 = {
    sizeof (InsTab6502.Ins) / sizeof (InsTab6502.Ins[0]),
    {
        { "ADC",  0x080A26C, 0x60, 0, PutAll },
        { "AND",  0x080A26C, 0x20, 0, PutAll },
        { "ASL",  0x000006e, 0x02, 1, PutAll },
        { "BCC",  0x0020000, 0x90, 0, PutPCRel8 },
        { "BCS",  0x0020000, 0xb0, 0, PutPCRel8 },
        { "BEQ",  0x0020000, 0xf0, 0, PutPCRel8 },
        { "BIT",  0x000000C, 0x00, 2, PutAll },
        { "BMI",  0x0020000, 0x30, 0, PutPCRel8 },
        { "BNE",  0x0020000, 0xd0, 0, PutPCRel8 },
        { "BPL",  0x0020000, 0x10, 0, PutPCRel8 },
        { "BRK",  0x0000001, 0x00, 0, PutAll },
        { "BVC",  0x0020000, 0x50, 0, PutPCRel8 },
        { "BVS",  0x0020000, 0x70, 0, PutPCRel8 },
        { "CLC",  0x0000001, 0x18, 0, PutAll },
        { "CLD",  0x0000001, 0xd8, 0, PutAll },
        { "CLI",  0x0000001, 0x58, 0, PutAll },
        { "CLV",  0x0000001, 0xb8, 0, PutAll },
        { "CMP",  0x080A26C, 0xc0, 0, PutAll },
        { "CPX",  0x080000C, 0xe0, 1, PutAll },
        { "CPY",  0x080000C, 0xc0, 1, PutAll },
        { "DEC",  0x000006C, 0x00, 3, PutAll },
        { "DEX",  0x0000001, 0xca, 0, PutAll },
        { "DEY",  0x0000001, 0x88, 0, PutAll },
        { "EOR",  0x080A26C, 0x40, 0, PutAll },
        { "INC",  0x000006c, 0x00, 4, PutAll },
        { "INX",  0x0000001, 0xe8, 0, PutAll },
        { "INY",  0x0000001, 0xc8, 0, PutAll },
        { "JMP",  0x0000808, 0x4c, 6, PutJMP },
        { "JSR",  0x0000008, 0x20, 7, PutAll },
        { "LDA",  0x080A26C, 0xa0, 0, PutAll },
        { "LDX",  0x080030C, 0xa2, 1, PutAll },
        { "LDY",  0x080006C, 0xa0, 1, PutAll },
        { "LSR",  0x000006F, 0x42, 1, PutAll },
        { "NOP",  0x0000001, 0xea, 0, PutAll },
        { "ORA",  0x080A26C, 0x00, 0, PutAll },
        { "PHA",  0x0000001, 0x48, 0, PutAll },
        { "PHP",  0x0000001, 0x08, 0, PutAll },
        { "PLA",  0x0000001, 0x68, 0, PutAll },
        { "PLP",  0x0000001, 0x28, 0, PutAll },
        { "ROL",  0x000006F, 0x22, 1, PutAll },
        { "ROR",  0x000006F, 0x62, 1, PutAll },
        { "RTI",  0x0000001, 0x40, 0, PutAll },
        { "RTS",  0x0000001, 0x60, 0, PutAll },
        { "SBC",  0x080A26C, 0xe0, 0, PutAll },
        { "SEC",  0x0000001, 0x38, 0, PutAll },
        { "SED",  0x0000001, 0xf8, 0, PutAll },
        { "SEI",  0x0000001, 0x78, 0, PutAll },
        { "STA",  0x000A26C, 0x80, 0, PutAll },
        { "STX",  0x000010c, 0x82, 1, PutAll },
        { "STY",  0x000002c, 0x80, 1, PutAll },
        { "TAX",  0x0000001, 0xaa, 0, PutAll },
        { "TAY",  0x0000001, 0xa8, 0, PutAll },
        { "TSX",  0x0000001, 0xba, 0, PutAll },
        { "TXA",  0x0000001, 0x8a, 0, PutAll },
        { "TXS",  0x0000001, 0x9a, 0, PutAll },
        { "TYA",  0x0000001, 0x98, 0, PutAll }
    }
};

/* Instruction table for the 6502 with illegal instructions */
/* Instruction table for the 65SC02 */
static const struct {
    unsigned Count;
    InsDesc  Ins[66];
} InsTab65SC02 = {
    sizeof (InsTab65SC02.Ins) / sizeof (InsTab65SC02.Ins[0]),
    {
        { "ADC",  0x080A66C, 0x60, 0, PutAll },
        { "AND",  0x080A66C, 0x20, 0, PutAll },
        { "ASL",  0x000006e, 0x02, 1, PutAll },
        { "BCC",  0x0020000, 0x90, 0, PutPCRel8 },
        { "BCS",  0x0020000, 0xb0, 0, PutPCRel8 },
        { "BEQ",  0x0020000, 0xf0, 0, PutPCRel8 },
        { "BIT",  0x0A0006C, 0x00, 2, PutAll },
        { "BMI",  0x0020000, 0x30, 0, PutPCRel8 },
        { "BNE",  0x0020000, 0xd0, 0, PutPCRel8 },
        { "BPL",  0x0020000, 0x10, 0, PutPCRel8 },
        { "BRA",  0x0020000, 0x80, 0, PutPCRel8 },
        { "BRK",  0x0000001, 0x00, 0, PutAll },
        { "BVC",  0x0020000, 0x50, 0, PutPCRel8 },
        { "BVS",  0x0020000, 0x70, 0, PutPCRel8 },
        { "CLC",  0x0000001, 0x18, 0, PutAll },
        { "CLD",  0x0000001, 0xd8, 0, PutAll },
        { "CLI",  0x0000001, 0x58, 0, PutAll },
        { "CLV",  0x0000001, 0xb8, 0, PutAll },
        { "CMP",  0x080A66C, 0xc0, 0, PutAll },
        { "CPX",  0x080000C, 0xe0, 1, PutAll },
        { "CPY",  0x080000C, 0xc0, 1, PutAll },
        { "DEA",  0x0000001, 0x00, 3, PutAll },   /* == DEC */
        { "DEC",  0x000006F, 0x00, 3, PutAll },
        { "DEX",  0x0000001, 0xca, 0, PutAll },
        { "DEY",  0x0000001, 0x88, 0, PutAll },
        { "EOR",  0x080A66C, 0x40, 0, PutAll },
        { "INA",  0x0000001, 0x00, 4, PutAll },   /* == INC */
        { "INC",  0x000006f, 0x00, 4, PutAll },
        { "INX",  0x0000001, 0xe8, 0, PutAll },
        { "INY",  0x0000001, 0xc8, 0, PutAll },
        { "JMP",  0x0010808, 0x4c, 6, PutAll },
        { "JSR",  0x0000008, 0x20, 7, PutAll },
        { "LDA",  0x080A66C, 0xa0, 0, PutAll },
        { "LDX",  0x080030C, 0xa2, 1, PutAll },
        { "LDY",  0x080006C, 0xa0, 1, PutAll },
        { "LSR",  0x000006F, 0x42, 1, PutAll },
        { "NOP",  0x0000001, 0xea, 0, PutAll },
        { "ORA",  0x080A66C, 0x00, 0, PutAll },
        { "PHA",  0x0000001, 0x48, 0, PutAll },
        { "PHP",  0x0000001, 0x08, 0, PutAll },
        { "PHX",  0x0000001, 0xda, 0, PutAll },
        { "PHY",  0x0000001, 0x5a, 0, PutAll },
        { "PLA",  0x0000001, 0x68, 0, PutAll },
        { "PLP",  0x0000001, 0x28, 0, PutAll },
        { "PLX",  0x0000001, 0xfa, 0, PutAll },
        { "PLY",  0x0000001, 0x7a, 0, PutAll },
        { "ROL",  0x000006F, 0x22, 1, PutAll },
        { "ROR",  0x000006F, 0x62, 1, PutAll },
        { "RTI",  0x0000001, 0x40, 0, PutAll },
        { "RTS",  0x0000001, 0x60, 0, PutAll },
        { "SBC",  0x080A66C, 0xe0, 0, PutAll },
        { "SEC",  0x0000001, 0x38, 0, PutAll },
        { "SED",  0x0000001, 0xf8, 0, PutAll },
        { "SEI",  0x0000001, 0x78, 0, PutAll },
        { "STA",  0x000A66C, 0x80, 0, PutAll },
        { "STX",  0x000010c, 0x82, 1, PutAll },
        { "STY",  0x000002c, 0x80, 1, PutAll },
        { "STZ",  0x000006c, 0x04, 5, PutAll },
        { "TAX",  0x0000001, 0xaa, 0, PutAll },
        { "TAY",  0x0000001, 0xa8, 0, PutAll },
        { "TRB",  0x000000c, 0x10, 1, PutAll },
        { "TSB",  0x000000c, 0x00, 1, PutAll },
        { "TSX",  0x0000001, 0xba, 0, PutAll },
        { "TXA",  0x0000001, 0x8a, 0, PutAll },
        { "TXS",  0x0000001, 0x9a, 0, PutAll },
        { "TYA",  0x0000001, 0x98, 0, PutAll }
    }
};

/* Instruction table for the 65C02 */
/* Instruction table for the 4510 */
/* Instruction table for the 65816 */
/* Instruction table for the SWEET16 pseudo CPU */
/* Instruction table for the HuC6280 (the CPU used in the PC engine) */
/* An array with instruction tables */
static const InsTable* InsTabs[CPU_COUNT] = {
    (const InsTable*) &InsTabNone,
    (const InsTable*) &InsTab6502,
    (const InsTable*) &InsTab65SC02,
};
const InsTable* InsTab = (const InsTable*) &InsTab6502;

/* Table to build the effective 65xx opcode from a base opcode and an
** addressing mode. (The value in the table is ORed with the base opcode)
*/
static unsigned char EATab[12][AM65I_COUNT] = {
    {   /* Table 0 */
        0x00, 0x00, 0x05, 0x0D, 0x0F, 0x15, 0x1D, 0x1F,
        0x00, 0x19, 0x12, 0x00, 0x07, 0x11, 0x17, 0x01,
        0x00, 0x00, 0x00, 0x03, 0x13, 0x09, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 1 */
        0x08, 0x08, 0x04, 0x0C, 0x00, 0x14, 0x1C, 0x00,
        0x14, 0x1C, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x00
    },
    {   /* Table 2 */
        0x00, 0x00, 0x24, 0x2C, 0x0F, 0x34, 0x3C, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 3 */
        0x3A, 0x3A, 0xC6, 0xCE, 0x00, 0xD6, 0xDE, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 4 */
        0x1A, 0x1A, 0xE6, 0xEE, 0x00, 0xF6, 0xFE, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 5 */
        0x00, 0x00, 0x60, 0x98, 0x00, 0x70, 0x9E, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 6 */
        0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x90, 0x00
    },
    {   /* Table 7 (Subroutine opcodes) */
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 8 */
        0x00, 0x40, 0x01, 0x41, 0x00, 0x09, 0x49, 0x00,
        0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 9 */
        0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x30, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 10 (NOPs) */
        0xea, 0x00, 0x04, 0x0c, 0x00, 0x14, 0x1c, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
        0x00, 0x00, 0x00, 0x00
    },
    {   /* Table 11 (LAX) */
        0x08, 0x08, 0x04, 0x0C, 0x00, 0x14, 0x1C, 0x00,
        0x14, 0x1C, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x80, 0x00
    },
};

/* Table to build the effective SWEET16 opcode from a base opcode and an
** addressing mode.
*/
/* Table that encodes the additional bytes for each 65xx instruction */
unsigned char ExtBytes[AM65I_COUNT] = {
    0,          /* Implicit */
    0,          /* Accu */
    1,          /* Direct */
    2,          /* Absolute */
    3,          /* Absolute long */
    1,          /* Direct,X */
    2,          /* Absolute,X */
    3,          /* Absolute long,X */
    1,          /* Direct,Y */
    2,          /* Absolute,Y */
    1,          /* (Direct) */
    2,          /* (Absolute) */
    1,          /* [Direct] */
    1,          /* (Direct),Y */
    1,          /* [Direct],Y */
    1,          /* (Direct,X) */
    2,          /* (Absolute,X) */
    1,          /* Relative short */
    2,          /* Relative long */
    1,          /* r,s */
    1,          /* (r,s),y */
    1,          /* Immidiate accu */
    1,          /* Immidiate index */
    1,          /* Immidiate byte */
    2,          /* Blockmove (65816) */
    7,          /* Block transfer (HuC6280) */
    2,          /* Absolute Indirect long */
    2,          /* Immidiate word */
};

/*****************************************************************************/
/*                   Handler functions for 6502 derivates                    */
/*****************************************************************************/



static int EvalEA (const InsDesc* Ins, EffAddr* A)
/* Evaluate the effective address. All fields in A will be valid after calling
** this function. The function returns true on success and false on errors.
*/
{
    /* Get the set of possible addressing modes */
    GetEA (A);

    /* From the possible addressing modes, remove the ones that are invalid
    ** for this instruction or CPU.
    */
    A->AddrModeSet &= Ins->AddrMode;

    /* If we have an expression, check it and remove any addressing modes that
    ** are too small for the expression size. Since we have to study the
    ** expression anyway, do also replace it by a simpler one if possible.
    */
    if (A->Expr) {
        ExprDesc ED;
        ED_Init (&ED);

        /* Study the expression */
        StudyExpr (A->Expr, &ED);

        /* Simplify it if possible */
        A->Expr = SimplifyExpr (A->Expr, &ED);

        if (ED.AddrSize == ADDR_SIZE_DEFAULT) {
            /* We don't know how big the expression is. If the instruction
            ** allows just one addressing mode, assume this as address size
            ** for the expression. Otherwise assume the default address size
            ** for data.
            */
            if ((A->AddrModeSet & ~AM65_ALL_ZP) == 0) {
                ED.AddrSize = ADDR_SIZE_ZP;
            } else if ((A->AddrModeSet & ~AM65_ALL_ABS) == 0) {
                ED.AddrSize = ADDR_SIZE_ABS;
            } else if ((A->AddrModeSet & ~AM65_ALL_FAR) == 0) {
                ED.AddrSize = ADDR_SIZE_FAR;
            } else {
                ED.AddrSize = DataAddrSize;
                /* If the default address size of the data segment is unequal
                ** to zero page addressing, but zero page addressing is 
                ** allowed by the instruction, mark all symbols in the 
                ** expression tree. This mark will be checked at end of 
                ** assembly, and a warning is issued, if a zero page symbol
                ** was guessed wrong here.
                */
                if (ED.AddrSize > ADDR_SIZE_ZP && (A->AddrModeSet & AM65_SET_ZP)) {
                    ExprGuessedAddrSize (A->Expr, ADDR_SIZE_ZP);
                }
            }
        }

        /* Check the size */
        switch (ED.AddrSize) {

            case ADDR_SIZE_ABS:
                A->AddrModeSet &= ~AM65_SET_ZP;
                break;

            case ADDR_SIZE_FAR:
                A->AddrModeSet &= ~(AM65_SET_ZP | AM65_SET_ABS);
                break;
        }

        /* Free any resource associated with the expression desc */
        ED_Done (&ED);
    }

    /* Check if we have any adressing modes left */
    if (A->AddrModeSet == 0) {
        Error ("Illegal addressing mode");
        return 0;
    }
    A->AddrMode    = BitFind (A->AddrModeSet);
    A->AddrModeBit = (0x01UL << A->AddrMode);

    /* If the instruction has a one byte operand and immediate addressing is
    ** allowed but not used, check for an operand expression in the form
    ** <label or >label, where label is a far or absolute label. If found,
    ** emit a warning. This warning protects against a typo, where the '#'
    ** for the immediate operand is omitted.
    */
    if (A->Expr && (Ins->AddrMode & AM65_ALL_IMM)                &&
        (A->AddrModeSet & (AM65_DIR | AM65_ABS | AM65_ABS_LONG)) &&
        ExtBytes[A->AddrMode] == 1) {

        /* Found, check the expression */
        ExprNode* Left = A->Expr->Left;
        if ((A->Expr->Op == EXPR_BYTE0 || A->Expr->Op == EXPR_BYTE1) &&
            Left->Op == EXPR_SYMBOL                                  &&
            GetSymAddrSize (Left->V.Sym) != ADDR_SIZE_ZP) {

            /* Output a warning */
            Warning (1, "Suspicious address expression");
        }
    }

    /* Build the opcode */
    A->Opcode = Ins->BaseCode | EATab[Ins->ExtCode][A->AddrMode];

    /* If feature force_range is active, and we have immediate addressing mode,
    ** limit the expression to the maximum possible value.
    */
    if (A->AddrMode == AM65I_IMM_ACCU || A->AddrMode == AM65I_IMM_INDEX ||
        A->AddrMode == AM65I_IMM_IMPLICIT || A->AddrMode == AM65I_IMM_IMPLICIT_WORD) {
        if (ForceRange && A->Expr) {
            A->Expr = MakeBoundedExpr (A->Expr, ExtBytes[A->AddrMode]);
        }
    }

    /* Success */
    return 1;
}



static void EmitCode (EffAddr* A)
/* Output code for the data in A */
{
    /* Check how many extension bytes are needed and output the instruction */
    switch (ExtBytes[A->AddrMode]) {

        case 0:
            Emit0 (A->Opcode);
            break;

        case 1:
            Emit1 (A->Opcode, A->Expr);
            break;

        case 2:
            Emit2 (A->Opcode, A->Expr);
            break;

        case 3:
            /* Far argument */
            Emit3 (A->Opcode, A->Expr);
            break;

        default:
            Internal ("Invalid operand byte count: %u", ExtBytes[A->AddrMode]);

    }
}



static void PutPCRel8 (const InsDesc* Ins)
/* Handle branches with a 8 bit distance */
{
    EmitPCRel (Ins->BaseCode, GenBranchExpr (2), 1);
}














static void PutJMP (const InsDesc* Ins)
/* Handle the jump instruction for the 6502. Problem is that these chips have
** a bug: If the address crosses a page, the upper byte gets not corrected and
** the instruction will fail. The PutJmp function will add a linker assertion
** to check for this case and is otherwise identical to PutAll.
*/
{
    EffAddr A;

    /* Evaluate the addressing mode used */
    if (EvalEA (Ins, &A)) {

        /* Check for indirect addressing */
        if (A.AddrModeBit & AM65_ABS_IND) {

            /* Compare the low byte of the expression to 0xFF to check for
            ** a page cross. Be sure to use a copy of the expression otherwise
            ** things will go weird later.
            */
            ExprNode* E = GenNE (GenByteExpr (CloneExpr (A.Expr)), 0xFF);

            /* Generate the message */
            unsigned Msg = GetStringId ("\"jmp (abs)\" across page border");

            /* Generate the assertion */
            AddAssertion (E, ASSERT_ACT_WARN, Msg);
        }

        /* No error, output code */
        EmitCode (&A);
    }
}




static void PutAll (const InsDesc* Ins)
/* Handle all other instructions */
{
    EffAddr A;

    /* Evaluate the addressing mode used */
    if (EvalEA (Ins, &A)) {
        /* No error, output code */
        EmitCode (&A);
    }
}




/*****************************************************************************/
/*                       Handler functions for SWEET16                       */
/*****************************************************************************/





/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



static int CmpName (const void* Key, const void* Instr)
/* Compare function for bsearch */
{
    return strcmp ((const char*)Key, ((const InsDesc*) Instr)->Mnemonic);
}



void SetCPU (cpu_t NewCPU)
/* Set a new CPU */
{
    /* Make sure the parameter is correct */
    CHECK (NewCPU < CPU_COUNT);

    /* Check if we have support for the new CPU, if so, use it */
    if (NewCPU != CPU_UNKNOWN && InsTabs[NewCPU]) {
        CPU = NewCPU;
        InsTab = InsTabs[CPU];
    } else {
        Error ("CPU not supported");
    }
}



cpu_t GetCPU (void)
/* Return the current CPU */
{
    return CPU;
}



int FindInstruction (const StrBuf* Ident)
/* Check if Ident is a valid mnemonic. If so, return the index in the
** instruction table. If not, return -1.
*/
{
    unsigned I;
    const InsDesc* ID;
    char Key[sizeof (ID->Mnemonic)];

    /* Shortcut for the "none" CPU: If there are no instructions to search
    ** for, bail out early.
    */
    if (InsTab->Count == 0) {
        /* Not found */
        return -1;
    }

    /* Make a copy, and uppercase that copy */
    I = 0;
    while (I < SB_GetLen (Ident)) {
        /* If the identifier is longer than the longest mnemonic, it cannot
        ** be one.
        */
        if (I >= sizeof (Key) - 1) {
            /* Not found, no need for further action */
            return -1;
        }
        Key[I] = toupper ((unsigned char)SB_AtUnchecked (Ident, I));
        ++I;
    }
    Key[I] = '\0';

    /* Search for the key */
    ID = bsearch (Key, InsTab->Ins, InsTab->Count, sizeof (InsDesc), CmpName);
    if (ID == 0) {
        /* Not found */
        return -1;
    } else {
        /* Found, return the entry */
        return ID - InsTab->Ins;
    }
}



void HandleInstruction (unsigned Index)
/* Handle the mnemonic with the given index */
{
    /* Safety check */
    PRECONDITION (Index < InsTab->Count);

    /* Skip the mnemonic token */
    NextTok ();

    /* Call the handler */
    InsTab->Ins[Index].Emit (&InsTab->Ins[Index]);
}
