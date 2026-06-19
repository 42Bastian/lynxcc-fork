/*****************************************************************************/
/*                                                                           */
/*                                   cpu.c                                   */
/*                                                                           */
/*                            CPU specifications                             */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2003-2011, Ullrich von Bassewitz                                      */
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



/* common */
#include "addrsize.h"
#include "check.h"
#include "cpu.h"
#include "strutil.h"



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* CPU used */
cpu_t CPU = CPU_UNKNOWN;

/* Table with target names */
const char* CPUNames[CPU_COUNT] = {
    "none",
    "6502",
    "65SC02",
};

/* Tables with CPU instruction sets */
const unsigned CPUIsets[CPU_COUNT] = {
    CPU_ISET_NONE,
    CPU_ISET_6502,
    CPU_ISET_6502 | CPU_ISET_65SC02,
};



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



int ValidAddrSizeForCPU (unsigned char AddrSize)
/* Check if the given address size is valid for the current CPU */
{
    switch (AddrSize) {
        case ADDR_SIZE_DEFAULT:
            /* Always supported */
            return 1;

        case ADDR_SIZE_ZP:
            /* Always supported */
            return 1;

        case ADDR_SIZE_ABS:
            /* Always supported */
            return 1;

        case ADDR_SIZE_FAR:
            /* Supported by "none" only */
            return (CPU == CPU_NONE);

        case ADDR_SIZE_LONG:
            /* "none" supports all sizes */
            return (CPU == CPU_NONE);

        default:
            FAIL ("Invalid address size");
            /* NOTREACHED */
            return 0;
    }
}



cpu_t FindCPU (const char* Name)
/* Find a CPU by name and return the target id. CPU_UNKNOWN is returned if
** the given name is no valid target.
*/
{
    unsigned I;

    /* Check all CPU names */
    for (I = 0; I < CPU_COUNT; ++I) {
        if (StrCaseCmp (CPUNames [I], Name) == 0) {
            return (cpu_t)I;
        }
    }

    /* Not found */
    return CPU_UNKNOWN;
}
