/*****************************************************************************/
/*                                                                           */
/*                              spritesheet.c                                */
/*                                                                           */
/*     Sprite-sheet driver for the sp65 sprite and bitmap utility            */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2026,      the lynxcc authors                                         */
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



#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* common */
#include "chartype.h"
#include "cmdline.h"
#include "print.h"
#include "strbuf.h"
#include "version.h"
#include "xmalloc.h"

/* sp65 */
#include "attr.h"
#include "error.h"
#include "lynxsprite.h"
#include "spritesheet.h"



/*****************************************************************************/
/*                             Helper functions                             */
/*****************************************************************************/



static unsigned GetUnsAttr (const Collection* A, const char* Name, unsigned Default)
/* Return an unsigned numeric attribute, or Default if it is not present */
{
    char        Term;
    unsigned    Val;
    const char* V = GetAttrVal (A, Name);
    if (V == 0) {
        return Default;
    }
    if (sscanf (V, "%u%c", &Val, &Term) != 1) {
        Error ("Invalid value '%s' for sprite-sheet attribute '%s'", V, Name);
    }
    return Val;
}



static int ValidIdentifier (const char* L)
/* Check a C/asm identifier for validity */
{
    if (L == 0) {
        return 0;
    }
    if (*L != '_' && !IsAlpha (*L)) {
        return 0;
    }
    for (++L; *L; ++L) {
        if (*L != '_' && !IsAlNum (*L)) {
            return 0;
        }
    }
    return 1;
}



static int WantAsm (const Collection* A, const char* Name)
/* Decide whether to emit assembler (1) or C (0). An explicit "format"
** attribute wins; otherwise the output file name extension decides.
*/
{
    const char* Dot;
    const char* Format = GetAttrVal (A, "format");
    if (Format != 0) {
        if (strcmp (Format, "asm") == 0) {
            return 1;
        } else if (strcmp (Format, "c") == 0) {
            return 0;
        } else {
            Error ("Sprite sheets support only format=c or format=asm");
        }
    }

    /* Auto-detect from the file name extension */
    Dot = strrchr (Name, '.');
    if (Dot != 0) {
        if (strcmp (Dot, ".s")   == 0 || strcmp (Dot, ".S")   == 0 ||
            strcmp (Dot, ".asm") == 0 || strcmp (Dot, ".inc") == 0 ||
            strcmp (Dot, ".a")   == 0) {
            return 1;
        }
    }
    return 0;
}



/*****************************************************************************/
/*                              Output writers                              */
/*****************************************************************************/



static void WriteByteBlock (FILE* F, const char* Lead, const StrBuf* Data,
                            unsigned BytesPerLine, unsigned Base, int Asm)
/* Write the raw concatenated sprite bytes, BytesPerLine per row */
{
    const char* D    = SB_GetConstBuf (Data);
    unsigned    Size = SB_GetLen (Data);

    while (Size) {
        unsigned I;
        unsigned Chunk = (Size > BytesPerLine)? BytesPerLine : Size;

        fputs (Lead, F);
        for (I = 0; I < Chunk; ++I) {
            unsigned char V = (unsigned char) *D++;
            if (Asm) {
                if (I > 0) {
                    fputc (',', F);
                }
                switch (Base) {
                    case 2:
                        fprintf (F, "%%%u%u%u%u%u%u%u%u",
                                 (V >> 7) & 1, (V >> 6) & 1, (V >> 5) & 1,
                                 (V >> 4) & 1, (V >> 3) & 1, (V >> 2) & 1,
                                 (V >> 1) & 1, (V >> 0) & 1);
                        break;
                    case 10: fprintf (F, "%u", V); break;
                    default: fprintf (F, "$%02X", V); break;
                }
            } else {
                if (Base == 10) {
                    fprintf (F, "%u,", V);
                } else {
                    fprintf (F, "0x%02X,", V);
                }
            }
        }
        fputc ('\n', F);
        Size -= Chunk;
    }
}



static void WriteCSheet (const char* Name, const char* Ident, const StrBuf* Data,
                         const unsigned* Offsets, unsigned Count,
                         unsigned FW, unsigned FH, unsigned Colors,
                         unsigned BytesPerLine, unsigned Base,
                         const char* SrcName)
/* Emit a C header: one data array plus a const pointer table */
{
    unsigned I;
    FILE* F = fopen (Name, "w");
    if (F == 0) {
        Error ("Cannot open output file '%s': %s", Name, strerror (errno));
    }

    fprintf (F,
             "/*\n"
             "** This file was generated by %s %s from\n"
             "** %s (sprite sheet, %u frames of %ux%u, %u colors)\n"
             "*/\n\n",
             ProgName, GetVersionAsString (), SrcName, Count, FW, FH, Colors);

    fprintf (F,
             "#define %s_COUNT        %u\n"
             "#define %s_WIDTH        %u\n"
             "#define %s_HEIGHT       %u\n"
             "#define %s_COLORS       %u\n\n",
             Ident, Count, Ident, FW, Ident, FH, Ident, Colors);

    fprintf (F, "const unsigned char %s_data[] = {\n", Ident);
    WriteByteBlock (F, "    ", Data, BytesPerLine, Base, 0);
    fputs ("};\n\n", F);

    fprintf (F, "const unsigned char* const %s[%s_COUNT] = {\n", Ident, Ident);
    for (I = 0; I < Count; ++I) {
        fprintf (F, "    %s_data + %u,\n", Ident, Offsets[I]);
    }
    fputs ("};\n", F);

    if (fclose (F) != 0) {
        Error ("Error closing output file '%s': %s", Name, strerror (errno));
    }
}



static void WriteAsmSheet (const char* Name, const char* Ident, const StrBuf* Data,
                           const unsigned* Offsets, unsigned Count,
                           unsigned FW, unsigned FH, unsigned Colors,
                           unsigned BytesPerLine, unsigned Base,
                           const char* SrcName)
/* Emit a ca65 header: a .proc with the data and a .word frame table */
{
    unsigned I;
    FILE* F = fopen (Name, "w");
    if (F == 0) {
        Error ("Cannot open output file '%s': %s", Name, strerror (errno));
    }

    fprintf (F,
             ";\n"
             "; This file was generated by %s %s from\n"
             "; %s (sprite sheet, %u frames of %ux%u, %u colors)\n"
             ";\n\n",
             ProgName, GetVersionAsString (), SrcName, Count, FW, FH, Colors);

    fprintf (F,
             ".proc   %s\n"
             "        COUNT  = %u\n"
             "        WIDTH  = %u\n"
             "        HEIGHT = %u\n"
             "        COLORS = %u\n\n",
             Ident, Count, FW, FH, Colors);

    fputs ("data:\n", F);
    WriteByteBlock (F, "        .byte   ", Data, BytesPerLine, Base, 1);

    fputs ("\nframes:\n", F);
    for (I = 0; I < Count; ++I) {
        fprintf (F, "        .word   data + %u\n", Offsets[I]);
    }

    fputs (".endproc\n", F);

    if (fclose (F) != 0) {
        Error ("Error closing output file '%s': %s", Name, strerror (errno));
    }
}



/*****************************************************************************/
/*                                  Driver                                  */
/*****************************************************************************/



void SpriteSheet (const Bitmap* B, const char* ArgList)
/* See spritesheet.h */
{
    static const char* const NameList[] = { "name" };

    Collection* A;
    const char* Name;
    const char* Ident;
    unsigned    FW, FH, Cols, Rows, First, Count, Gap, Margin;
    unsigned    BytesPerLine, Base;
    unsigned    W, H, Colors;
    unsigned    I;
    unsigned*   Offsets;
    StrBuf*     All;

    /* Parse the attribute list (a bare leading value is the file name) */
    A = ParseAttrList (ArgList, NameList, 1);

    Name  = NeedAttrVal (A, "name", "sprite-sheet");
    Ident = GetAttrVal (A, "ident");
    if (!ValidIdentifier (Ident)) {
        Error ("Sprite sheet needs a valid 'ident' for the frame table");
    }

    W      = GetBitmapWidth (B);
    H      = GetBitmapHeight (B);
    Colors = GetBitmapColors (B);

    /* Cell size is mandatory */
    FW = GetUnsAttr (A, "fw", 0);
    FH = GetUnsAttr (A, "fh", 0);
    if (FW == 0 || FH == 0) {
        Error ("Sprite sheet needs non-zero 'fw' and 'fh'");
    }

    Gap    = GetUnsAttr (A, "gap", 0);
    Margin = GetUnsAttr (A, "margin", 0);

    /* Derive the grid from the image if cols/rows were not given. The usable
    ** span is the image minus the two margins; each cell occupies fw(+gap).
    */
    if (W < 2 * Margin + FW || H < 2 * Margin + FH) {
        Error ("Sprite sheet: image %ux%u too small for margin %u and cell %ux%u",
               W, H, Margin, FW, FH);
    }
    Cols = GetUnsAttr (A, "cols", (W - 2 * Margin + Gap) / (FW + Gap));
    Rows = GetUnsAttr (A, "rows", (H - 2 * Margin + Gap) / (FH + Gap));
    if (Cols == 0 || Rows == 0) {
        Error ("Sprite sheet: computed an empty grid (%ux%u)", Cols, Rows);
    }

    /* The grid must fit inside the image */
    if (Margin + Cols * FW + (Cols - 1) * Gap > W ||
        Margin + Rows * FH + (Rows - 1) * Gap > H) {
        Error ("Sprite sheet: %ux%u grid of %ux%u cells (gap %u, margin %u) "
               "does not fit in a %ux%u image",
               Cols, Rows, FW, FH, Gap, Margin, W, H);
    }

    First = GetUnsAttr (A, "first", 0);
    Count = GetUnsAttr (A, "count", (First < Cols * Rows)? Cols * Rows - First : 0);
    if (Count == 0 || First + Count > Cols * Rows) {
        Error ("Sprite sheet: frame range first=%u count=%u exceeds the %u cells",
               First, Count, Cols * Rows);
    }

    BytesPerLine = GetUnsAttr (A, "bytesperline", 16);
    if (BytesPerLine < 1 || BytesPerLine > 64) {
        Error ("Invalid 'bytesperline' for sprite sheet");
    }
    Base = GetUnsAttr (A, "base", 16);

    /* Encode every selected cell and concatenate the blobs, remembering where
    ** each frame starts in the combined buffer.
    */
    Offsets = xmalloc (Count * sizeof (unsigned));
    All     = NewStrBuf ();

    for (I = 0; I < Count; ++I) {
        unsigned K   = First + I;
        unsigned Col = K % Cols;
        unsigned Row = K / Cols;
        unsigned X   = Margin + Col * (FW + Gap);
        unsigned Y   = Margin + Row * (FH + Gap);
        Bitmap* Cell = SliceBitmap (B, X, Y, FW, FH);
        StrBuf* Frame = GenLynxSprite (Cell, A);

        Offsets[I] = SB_GetLen (All);
        SB_Append (All, Frame);

        FreeStrBuf (Frame);
        FreeBitmap (Cell);
    }

    /* Write the combined header */
    if (WantAsm (A, Name)) {
        WriteAsmSheet (Name, Ident, All, Offsets, Count, FW, FH, Colors,
                       BytesPerLine, Base, "sprite sheet");
    } else {
        WriteCSheet (Name, Ident, All, Offsets, Count, FW, FH, Colors,
                     BytesPerLine, Base, "sprite sheet");
    }

    Print (stdout, 1, "Sprite sheet: wrote %u frames of %ux%u to '%s'\n",
           Count, FW, FH, Name);

    FreeStrBuf (All);
    xfree (Offsets);
    FreeAttrList (A);
}
