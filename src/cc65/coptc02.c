/*****************************************************************************/
/*                                                                           */
/*                                 coptc02.h                                 */
/*                                                                           */
/*                       65C02 specific optimizations                        */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2001-2012, Ullrich von Bassewitz                                      */
/*                Roeerstrasse 52                                            */
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



#include <string.h>

/* common */
#include "target.h"

/* cc65 */
#include "codeent.h"
#include "codeinfo.h"
#include "error.h"
#include "coptc02.h"



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/*****************************************************************************/
/*                             Helper functions                              */
/*****************************************************************************/



static int IsSuzyHwAddr (const CodeEntry* E)
/* Return true if the entry's argument is a constant address within the Lynx
** Suzy hardware register range $FC00-$FCFF. Suzy breaks if one instruction
** performs two Suzy accesses (hardware spec ch. 3.1.2; LYNX_CODEGEN_DESIGN.md
** §2.2, LYNX_TGI_DESIGN.md §5), so read-modify-write opcodes such as TRB/TSB
** must never target this range. Only checked for the Lynx target.
*/
{
    return Target == TGT_LYNX               &&
           (E->Flags & CEF_NUMARG) != 0     &&
           E->Num >= 0xFC00 && E->Num <= 0xFCFF;
}



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



unsigned Opt65C02Ind (CodeSeg* S)
/* Try to use the indirect addressing mode where possible */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        /* Get next entry */
        CodeEntry* E = CS_GetEntry (S, I);

        /* Check for addressing mode indirect indexed Y where Y is zero.
        ** Note: All opcodes that are available as (zp),y are also available
        ** as (zp), so we can ignore the actual opcode here.
        */
        if (E->AM == AM65_ZP_INDY && E->RI->In.RegY == 0) {

            /* Replace it by indirect addressing mode */
            CodeEntry* X = NewCodeEntry (E->OPC, AM65_ZP_IND, E->Arg, 0, E->LI);
            CS_InsertEntry (S, X, I+1);
            CS_DelEntry (S, I);

            /* We had changes */
            ++Changes;

        }

        /* Next entry */
        ++I;

    }

    /* Return the number of changes made */
    return Changes;
}



unsigned Opt65C02BitOps (CodeSeg* S)
/* Use special bit op instructions of the C02 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        CodeEntry* L[3];

        /* Get next entry */
        L[0] = CS_GetEntry (S, I);

        /* Check for the sequence */
        if (L[0]->OPC == OP65_LDA                               &&
            (L[0]->AM == AM65_ZP || L[0]->AM == AM65_ABS)       &&
            !CS_RangeHasLabel (S, I+1, 2)                       &&
            CS_GetEntries (S, L+1, I+1, 2)                      &&
            (L[1]->OPC == OP65_AND || L[1]->OPC == OP65_ORA)    &&
            CE_IsConstImm (L[1])                                &&
            L[2]->OPC == OP65_STA                               &&
            L[2]->AM == L[0]->AM                                &&
            strcmp (L[2]->Arg, L[0]->Arg) == 0                  &&
            !IsSuzyHwAddr (L[0])                                &&
            !RegAUsed (S, I+3)) {

            char Buf[32];
            CodeEntry* X;

            /* Cycle-cost model guard (section 2.7, consumer 1): only perform
            ** the rewrite if the replacement (lda #imm + trb/tsb) is no slower
            ** than the original load/modify/store triple. On the 65SC02 it is
            ** always strictly faster, but checking instead of assuming keeps the
            ** transform correct should the timing model or the matched pattern
            ** ever change.
            */
            opc_t NewMemOp = (L[1]->OPC == OP65_AND)? OP65_TRB : OP65_TSB;
            unsigned OldCycles = CE_GetCycles (L[0]) +
                                 CE_GetCycles (L[1]) +
                                 CE_GetCycles (L[2]);
            unsigned NewCycles = GetInsnCycles (OP65_LDA, AM65_IMM) +
                                 GetInsnCycles (NewMemOp, L[0]->AM);
            if (NewCycles > OldCycles) {
                /* Would be slower - leave it alone */
                ++I;
                continue;
            }

            /* Use TRB for AND and TSB for ORA */
            if (L[1]->OPC == OP65_AND) {

                /* LDA #XX */
                sprintf (Buf, "$%02X", (int) ((~L[1]->Num) & 0xFF));
                X = NewCodeEntry (OP65_LDA, AM65_IMM, Buf, 0, L[1]->LI);
                CS_InsertEntry (S, X, I+3);

                /* TRB */
                X = NewCodeEntry (OP65_TRB, L[0]->AM, L[0]->Arg, 0, L[0]->LI);
                CS_InsertEntry (S, X, I+4);

            } else {

                /* LDA #XX */
                sprintf (Buf, "$%02X", (int) L[1]->Num);
                X = NewCodeEntry (OP65_LDA, AM65_IMM, Buf, 0, L[1]->LI);
                CS_InsertEntry (S, X, I+3);

                /* TSB */
                X = NewCodeEntry (OP65_TSB, L[0]->AM, L[0]->Arg, 0, L[0]->LI);
                CS_InsertEntry (S, X, I+4);
            }

            /* Delete the old stuff */
            CS_DelEntries (S, I, 3);

            /* We had changes */
            ++Changes;
        }

        /* Next entry */
        ++I;

    }

    /* Return the number of changes made */
    return Changes;
}



unsigned Opt65C02Stores (CodeSeg* S)
/* Use STZ where possible */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        /* Get next entry */
        CodeEntry* E = CS_GetEntry (S, I);

        /* Check for a store with a register value of zero and an addressing
        ** mode available with STZ.
        */
        if (((E->OPC == OP65_STA && E->RI->In.RegA == 0) ||
             (E->OPC == OP65_STX && E->RI->In.RegX == 0) ||
             (E->OPC == OP65_STY && E->RI->In.RegY == 0))       &&
            (E->AM == AM65_ZP  || E->AM == AM65_ABS ||
             E->AM == AM65_ZPX || E->AM == AM65_ABSX)) {

            /* Replace by STZ */
            CodeEntry* X = NewCodeEntry (OP65_STZ, E->AM, E->Arg, 0, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* Delete the old stuff */
            CS_DelEntry (S, I);

            /* We had changes */
            ++Changes;
        }

        /* Next entry */
        ++I;

    }

    /* Return the number of changes made */
    return Changes;
}



unsigned Opt65C02StackOps (CodeSeg* S)
/* Speed-biased pass (section 2.7, consumer 2): inline the "jsr incsp1" and
** "jsr incsp2" C-stack drops. These are tiny leaf routines, so the call and
** return overhead (jsr + rts = 12 cycles) dwarfs the body. On a speed-biased
** build (this pass is gated on CodeSizeFactor > 100) inlining the exact runtime
** body is a clear win at the cost of a handful of bytes - a case where size and
** speed genuinely diverge. The cycle-cost model decides: the rewrite only fires
** when the modelled inline body is faster than the modelled call. The emitted
** instructions are byte-for-byte the bodies of libsrc/runtime/incsp1.s and
** incsp2.s (minus the call/return), so correctness follows from the runtime.
*/
{
    unsigned Changes = 0;
    unsigned I;

    /* Modelled cost of the calls (jsr + leaf body + rts) versus the inline
    ** bodies on their fall-through paths. Computed from the cycle model so the
    ** decision tracks the table rather than hand-copied numbers.
    */
    unsigned Jsr   = GetInsnCycles (OP65_JSR, AM65_ABS);
    unsigned Rts   = GetInsnCycles (OP65_RTS, AM65_IMP);
    unsigned IncZp = GetInsnCycles (OP65_INC, AM65_ZP);
    unsigned Bne   = GetInsnCycles (OP65_BNE, AM65_BRA);
    unsigned Beq   = GetInsnCycles (OP65_BEQ, AM65_BRA);
    unsigned Bra   = GetInsnCycles (OP65_BRA, AM65_BRA);

    /* incsp1: call = jsr + (inc sp + bne + rts); inline = inc sp + bne */
    unsigned Call1   = Jsr + IncZp + Bne + Rts;
    unsigned Inline1 = IncZp + Bne;
    /* incsp2: call = jsr + (inc + beq + inc + beq + rts);
    ** inline = inc + beq + inc + beq + bra
    */
    unsigned Call2   = Jsr + IncZp + Beq + IncZp + Beq + Rts;
    unsigned Inline2 = IncZp + Beq + IncZp + Beq + Bra;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        CodeEntry* E = CS_GetEntry (S, I);
        CodeEntry* P = CS_GetNextEntry (S, I);
        CodeEntry* X;
        CodeLabel* L;

        /* We need a following instruction to act as the branch target. */
        if (P != 0 && CE_IsCallTo (E, "incsp1") && Inline1 < Call1) {

            /* inc sp */
            X = NewCodeEntry (OP65_INC, AM65_ZP, "sp", 0, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* bne L (skip the high-byte carry) */
            L = CS_GenLabel (S, P);
            X = NewCodeEntry (OP65_BNE, AM65_BRA, L->Name, L, E->LI);
            CS_InsertEntry (S, X, I+2);

            /* inc sp+1 */
            X = NewCodeEntry (OP65_INC, AM65_ZP, "sp+1", 0, E->LI);
            CS_InsertEntry (S, X, I+3);

            /* Delete the call; its labels move forward onto the inc sp */
            CS_DelEntry (S, I);

            I += 2;             /* skip the generated body (plus loop ++I) */
            ++Changes;

        } else if (P != 0 && CE_IsCallTo (E, "incsp2") && Inline2 < Call2) {

            /* Emit the exact incsp2.s body. The two carry targets and the
            ** continuation are created bottom-up so every branch target exists
            ** before the branch that references it; each insert goes to I+1, so
            ** the later-in-memory entry must be inserted first.
            **
            **      inc sp / beq @L1 / inc sp / beq @L2 / bra L
            ** @L1: inc sp
            ** @L2: inc sp+1
            ** L:   <next insn>
            */
            CodeLabel* LA;      /* @L1 */
            CodeLabel* LB;      /* @L2 */

            /* @L2: inc sp+1 */
            X = NewCodeEntry (OP65_INC, AM65_ZP, "sp+1", 0, E->LI);
            CS_InsertEntry (S, X, I+1);
            LB = CS_GenLabel (S, X);

            /* @L1: inc sp */
            X = NewCodeEntry (OP65_INC, AM65_ZP, "sp", 0, E->LI);
            CS_InsertEntry (S, X, I+1);
            LA = CS_GenLabel (S, X);

            /* bra L */
            L = CS_GenLabel (S, P);
            X = NewCodeEntry (OP65_BRA, AM65_BRA, L->Name, L, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* beq @L2 */
            X = NewCodeEntry (OP65_BEQ, AM65_BRA, LB->Name, LB, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* inc sp */
            X = NewCodeEntry (OP65_INC, AM65_ZP, "sp", 0, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* beq @L1 */
            X = NewCodeEntry (OP65_BEQ, AM65_BRA, LA->Name, LA, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* inc sp */
            X = NewCodeEntry (OP65_INC, AM65_ZP, "sp", 0, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* Delete the call; its labels move forward onto the first inc sp */
            CS_DelEntry (S, I);

            I += 6;             /* skip the generated body (plus loop ++I) */
            ++Changes;

        }

        /* Next entry */
        ++I;

    }

    /* Return the number of changes made */
    return Changes;
}
