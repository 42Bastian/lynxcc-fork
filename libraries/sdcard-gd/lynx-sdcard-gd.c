/*
** SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
**
** lynx-sdcard-gd.c - driver for the RetroHQ Lynx SD / GD flash cartridge MCU
** (design/LYNX_SDCARD_GD_API_DESIGN.md). Ported from the RetroHQ menu loader's
** LynxSD.c (SainT / GadgetUK); the wire protocol is byte-for-byte the same.
**
** Communication is a byte FIFO over two cartridge data ports, gated by bit 4
** of the Mikey IODAT register. Multi-byte arguments are little-endian; path
** strings are sent NUL-terminated.
*/

#include <lynx/sdcard-gd.h>

/* --------------------------------------------------------------------------
** MCU command bytes (0-based, written first before any argument bytes).
*/
typedef enum {
    ECmd_OpenDir = 0,
    ECmd_ReadDir,
    ECmd_OpenFile,
    ECmd_GetSize,
    ECmd_Seek,
    ECmd_Read,
    ECmd_Write,
    ECmd_Close,
    ECmd_ProgramFile,
    ECmd_ClearBlocks,
    ECmd_LowPowerMode
} ECommandByte;

/* --------------------------------------------------------------------------
** Registers and the FIFO ready/busy flag.
*/
#define AUXMASK 0x10
#define IODAT ((volatile unsigned char *) 0xFD8B)
#define IODIR ((volatile unsigned char *) 0xFD8A)
#define CART1 ((volatile unsigned char *) 0xFCB3)   /* FIFO write side */
#define CART0 ((volatile unsigned char *) 0xFCB2)   /* FIFO read side  */

/* Large-card (512-block, A19 via aux pin) modifiers. */
#define PROG_512  0x10      /* OR into ProgramFile block-size byte  */
#define CLEAR_512 0x8000    /* OR into ClearBlocks block-count word */

/* --------------------------------------------------------------------------
** Low-level FIFO byte transfer.
*/

static void __fastcall__ WriteByte (unsigned char byte)
{
    while (*IODAT & AUXMASK);    /* wait until the MCU FIFO can accept a byte */
    *CART1 = byte;
}

static void __fastcall__ WriteBytes (const unsigned char *pBuf, unsigned int nSize)
{
    while (nSize--) {
        WriteByte (*pBuf++);
    }
}

static void __fastcall__ WriteWord (unsigned int word)
{
    WriteBytes ((const unsigned char *) &word, 2);
}

static void __fastcall__ WriteDword (unsigned long dword)
{
    WriteBytes ((const unsigned char *) &dword, 4);
}

static unsigned char ReadByte (void)
{
    while (!(*IODAT & AUXMASK)); /* wait for a byte in the read FIFO */
    return *CART0;
}

static void __fastcall__ ReadBytes (unsigned char *pOut, unsigned int size)
{
    while (size--) {
        *pOut++ = ReadByte ();
    }
}

static unsigned long ReadDword (void)
{
    unsigned long nDword;
    ReadBytes ((unsigned char *) &nDword, 4);
    return nDword;
}

static void __fastcall__ WriteString (const char *pStr)
{
    char c;
    do {
        c = *pStr++;
        WriteByte (c);
    } while (c);
}

/* --------------------------------------------------------------------------
** Public API.
*/

void sdcard_gd_init (void)
{
    *IODIR = 0;         /* all input */
    *CART1 = 0xaa;      /* wake the MCU comms state machine */
}

void sdcard_gd_lowpower (void)
{
    WriteByte (ECmd_LowPowerMode);  /* power the SD card down, no response */
}

FRESULT __fastcall__ sdcard_gd_opendir (const char *pDir)
{
    WriteByte (ECmd_OpenDir);
    WriteString (pDir);
    return (FRESULT) ReadByte ();
}

FRESULT __fastcall__ sdcard_gd_readdir (SFileInfo *pInfo)
{
    FRESULT res;
    WriteByte (ECmd_ReadDir);
    res = (FRESULT) ReadByte ();
    if (res == FR_OK) {
        ReadBytes ((unsigned char *) pInfo, sizeof (SFileInfo));
    }
    return res;
}

FRESULT __fastcall__ sdcard_gd_open (const char *pFile)
{
    WriteByte (ECmd_OpenFile);
    WriteString (pFile);
    return (FRESULT) ReadByte ();
}

FRESULT __fastcall__ sdcard_gd_read (void *pBuffer, unsigned int nSize)
{
    WriteByte (ECmd_Read);
    WriteWord (nSize);
    ReadBytes ((unsigned char *) pBuffer, nSize);
    return (FRESULT) ReadByte ();   /* trailing status byte */
}

FRESULT __fastcall__ sdcard_gd_write (const void *pBuffer, unsigned int nSize)
{
    WriteByte (ECmd_Write);
    WriteWord (nSize);
    WriteBytes ((const unsigned char *) pBuffer, nSize);
    return (FRESULT) ReadByte ();
}

FRESULT sdcard_gd_close (void)
{
    WriteByte (ECmd_Close);
    return (FRESULT) ReadByte ();
}

FRESULT __fastcall__ sdcard_gd_seek (unsigned long nSeekPos)
{
    WriteByte (ECmd_Seek);
    WriteDword (nSeekPos);
    return (FRESULT) ReadByte ();
}

unsigned long sdcard_gd_size (void)
{
    WriteByte (ECmd_GetSize);
    return ReadDword ();
}

FRESULT __fastcall__ sdcard_gd_program (unsigned int  nStartBlock,
                                        unsigned char nBlockSize,
                                        unsigned int  nBlockCount,
                                        unsigned char b512BlockCard)
{
    if (b512BlockCard) {
        nBlockSize |= PROG_512;
    }

    WriteByte (ECmd_ProgramFile);
    WriteWord (nStartBlock);
    WriteByte (nBlockSize);
    WriteWord (nBlockCount);
    return (FRESULT) ReadByte ();   /* blocks until programming completes */
}

FRESULT __fastcall__ sdcard_gd_clear (unsigned int  nStartBlock,
                                      unsigned int  nBlocks,
                                      unsigned char b512BlockCard)
{
    if (b512BlockCard) {
        nBlocks |= CLEAR_512;
    }

    WriteByte (ECmd_ClearBlocks);
    WriteWord (nStartBlock);
    WriteWord (nBlocks);
    return (FRESULT) ReadByte ();   /* blocks until erasing completes */
}
