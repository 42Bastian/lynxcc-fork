/*****************************************************************************/
/*                                                                           */
/*                               sdcard-gd.h                                 */
/*                                                                           */
/*              RetroHQ Lynx SD / GD flash cartridge interface               */
/*                                                                           */
/*                                                                           */
/* Opt-in driver for talking to the microcontroller (MCU) on the RetroHQ     */
/* Lynx SD / GD flash cartridge (design/LYNX_SDCARD_GD_API_DESIGN.md). The   */
/* MCU exposes an SD card behind a small byte protocol over two cartridge    */
/* I/O registers: this API lets a program list directories, stream files,    */
/* reprogram the cart ROM window with a new game image, and power the card   */
/* down.                                                                     */
/*                                                                           */
/* This is specific to the RetroHQ SD/GD cart (the "_gd_" family). A second  */
/* flash-cart family, if added, gets its own header and library.            */
/*                                                                           */
/* Protocol facts worth knowing:                                             */
/*  - Every transfer is gated by bit 4 of IODAT ($FD8B): the write side      */
/*    blocks while the MCU FIFO is busy, the read side blocks until a byte   */
/*    is ready. Calls therefore busy-wait on hardware and never time out.    */
/*  - Multi-byte arguments are little-endian; path strings are sent          */
/*    NUL-terminated.                                                        */
/*  - sdcard_gd_read() streams the payload first, then a trailing status     */
/*    byte, so the whole buffer is always transferred.                       */
/*  - sdcard_gd_program() and sdcard_gd_clear() block on the MCU until the   */
/*    flash operation finishes; the returned status is the completion        */
/*    signal and can take a while for large ROMs.                            */
/*                                                                           */
/* Names use the SDK-native sdcard_gd_ prefix. The original RetroHQ          */
/* LynxSD_* names are provided as deprecated aliases at the end of this      */
/* header (define LYNX_NO_SDCARD_GD_COMPAT to remove them).                  */
/*                                                                           */
/*****************************************************************************/



#ifndef _SDCARD_GD_H
#define _SDCARD_GD_H



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* Result codes returned as the MCU status byte (FatFs style). */
typedef enum {
    FR_OK = 0,          /* 0 - success                          */
    FR_DISK_ERR,        /* 1 - low-level I/O error              */
    FR_NOT_READY,       /* 2 - card / MCU not ready             */
    FR_NO_FILE,         /* 3 - path not found                  */
    FR_NOT_OPENED,      /* 4 - operation needs an open file/dir */
    FR_NOT_ENABLED,     /* 5 - subsystem disabled              */
    FR_NO_FILESYSTEM    /* 6 - no valid FAT filesystem         */
} FRESULT;

/* Directory entry returned by sdcard_gd_readdir(). The wrapper reads
** sizeof(SFileInfo) bytes straight from the MCU, so the field order and
** sizes are fixed by the wire format (22 bytes; cc65 does not pad structs).
*/
typedef struct {
    unsigned long   fsize;      /* File size in bytes           */
    unsigned int    fdate;      /* FAT last-modified date       */
    unsigned int    ftime;      /* FAT last-modified time       */
    unsigned char   fattrib;    /* Attribute bits (AM_*)        */
    char            fname[13];  /* 8.3 name, NUL-terminated     */
} SFileInfo;

/* fattrib bits (a subset of the FAT attribute byte). */
#define AM_RDO  0x01    /* Read only        */
#define AM_HID  0x02    /* Hidden           */
#define AM_SYS  0x04    /* System           */
#define AM_VOL  0x08    /* Volume label     */
#define AM_LFN  0x0F    /* LFN entry (mask) */
#define AM_DIR  0x10    /* Directory        */
#define AM_ARC  0x20    /* Archive          */
#define AM_MASK 0x3F    /* Mask of defined bits */



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



void sdcard_gd_init (void);
/* Wake the cart MCU comms (set IODIR to input, send the $AA magic byte).
** Call once before any other sdcard_gd_ function.
*/

void sdcard_gd_lowpower (void);
/* Power the SD card down and enter low-power mode. Typically the last call
** before launching a freshly programmed ROM. Sends no response.
*/

FRESULT __fastcall__ sdcard_gd_opendir (const char* pDir);
/* Open the directory at pDir (DOS-style path, '/' separators, relative to the
** card root). Follow with repeated sdcard_gd_readdir() calls.
*/

FRESULT __fastcall__ sdcard_gd_readdir (SFileInfo* pInfo);
/* Read the next entry from the directory opened by sdcard_gd_opendir() into
** *pInfo. Returns FR_OK while entries remain; any other result ends the walk
** (and *pInfo is left unchanged).
*/

FRESULT __fastcall__ sdcard_gd_open (const char* pFile);
/* Open a file for reading/writing (one file open at a time). */

FRESULT sdcard_gd_close (void);
/* Close the currently open file, flushing any buffered writes. */

FRESULT __fastcall__ sdcard_gd_seek (unsigned long nSeekPos);
/* Set the read/write position of the open file to an absolute byte offset. */

unsigned long sdcard_gd_size (void);
/* Return the size in bytes of the currently open file. */

FRESULT __fastcall__ sdcard_gd_read (void* pBuffer, unsigned int nSize);
/* Read nSize bytes from the open file into pBuffer. The payload is always
** fully transferred; the return value is the trailing status byte.
*/

FRESULT __fastcall__ sdcard_gd_write (const void* pBuffer, unsigned int nSize);
/* Write nSize bytes from pBuffer to the open file. */

FRESULT __fastcall__ sdcard_gd_program (unsigned int  nStartBlock,
                                        unsigned char nBlockSize,
                                        unsigned int  nBlockCount,
                                        unsigned char b512BlockCard);
/* Program nBlockCount blocks of nBlockSize (in 256-byte units: 1/2/4/8) into
** cart ROM starting at nStartBlock, sourced from the current position of the
** open file. Set b512BlockCard non-zero for a 512-block card (A19 via aux
** pin). Blocks until the flash completes; the returned status is the result.
*/

FRESULT __fastcall__ sdcard_gd_clear (unsigned int  nStartBlock,
                                      unsigned int  nBlocks,
                                      unsigned char b512BlockCard);
/* Erase nBlocks cart-ROM blocks starting at nStartBlock, no source file. Set
** b512BlockCard non-zero for a 512-block card. Blocks until the erase
** completes; the returned status is the result.
*/



/*****************************************************************************/
/*                     Deprecated RetroHQ LynxSD_* aliases                   */
/*****************************************************************************/



/* The original RetroHQ menu code uses LynxSD_*-prefixed names. They are kept
** here as thin aliases so existing community code compiles unchanged. New
** code should use the sdcard_gd_* names. Define LYNX_NO_SDCARD_GD_COMPAT
** before including this header to remove the aliases.
*/
#ifndef LYNX_NO_SDCARD_GD_COMPAT
#define LynxSD_Init             sdcard_gd_init
#define LynxSD_LowPowerMode     sdcard_gd_lowpower
#define LynxSD_OpenDir          sdcard_gd_opendir
#define LynxSD_ReadDir          sdcard_gd_readdir
#define LynxSD_OpenFile         sdcard_gd_open
#define LynxSD_CloseFile        sdcard_gd_close
#define LynxSD_SeekFile         sdcard_gd_seek
#define LynxSD_GetFileSize      sdcard_gd_size
#define LynxSD_ReadFile         sdcard_gd_read
#define LynxSD_WriteFile        sdcard_gd_write
#define LynxSD_ProgramROMFromFile sdcard_gd_program
#define LynxSD_ClearROMBlocks   sdcard_gd_clear
#endif



/* End of sdcard-gd.h */
#endif
