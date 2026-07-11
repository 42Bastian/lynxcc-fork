/*
 * HandyMusic script compiler (hmcc) - ported into the Lynx Game Development
 * SDK (lynxcc) from Osman Celimli's HandyMusic v1.40cx+ "HMCC" tool.
 *
 * Original work (c) Osman Celimli.  HandyMusic grant, verbatim from the
 * HandyMusic Programmer's Manual: "HandyMusic is completely free to use and
 * modify in your own projects."
 *
 * This is a derived port for lynxcc: the Win32 file I/O and CRT debug
 * scaffolding of the original have been replaced with portable C; the
 * script-compiler logic itself is unchanged.  Not MPL-licensed - see the
 * HandyMusic entry in doc/licenses.html (section 4.5).
 */
/****************************************************************
 * instfx.h
 * Last change 11-29-2012 by Osman Celimli
 ****************************************************************/
/****************************************************************
 * Important Defines
 ****************************************************************/
/*See programmer's manual for details on the following*/
#define SFX_STOP	0
#define SFX_WAIT	1
#define SFX_SETWF	2
#define SFX_SETVOL	3
#define SFX_SETFREQ	4
#define SFX_LOOP	5
#define SFX_ENDLOOP	6
/*Note that all the values are in little endian*/
#define SFX_HDR_NOTEOFF	0	/*16, Pointer to Note Off*/
#define SFX_HDR_SHIFT	2	/*16, Shift Register Value*/
#define SFX_HDR_FEEDBI	4	/*16, Feedback + Integrate Flag*/
#define SFX_HDR_VOLI	6	/*8, Signed Starting Volume*/
#define SFX_HDR_VOLADJ	7	/*8.8, Volume Adjust*/
#define SFX_HDR_FREQI	9	/*16, Divider Value*/
#define SFX_HDR_FREQADJ	11	/*16.8, Divider Adjustment Value*/
#define SFX_HDR_LENGTH	14

#define SFX_PARSER_ITSIZE	32
#define SFX_PARSER_BSIZE	256
#define SFX_PARSER_ISSIZE	128

typedef struct instrument
{
 char *instName;
 char *instStream;
 unsigned int priority;
 unsigned int noteOffOff;
 unsigned int scriptLen,bufferLen;
 double tuning;
} instrument;

typedef int (*instCommEntry)(int,struct instrument*);

extern int commentStart;

extern int instfx_binSize;
extern int instfx_pTableOff;
extern int instfx_lTableOff;
extern int instfx_hTableOff;

/****************************************************************
 * Function Prototypes
 ****************************************************************/
int InstFX_ParseSrc(int sfxMode, FILE *infile, char *infileName);
int InstFX_hasGarbage();

char* InstFX_BuildBin(int mode, int baseAddress);
int InstFX_MakeEquates(int baseAddress, char *infileName);

int InstFX_Com_END(int parseMode, instrument *target);
int InstFX_Com_REST(int parseMode, instrument *target);
int InstFX_Com_WAVEFORM(int parseMode, instrument *target);
int InstFX_Com_VOLUME(int parseMode, instrument *target);
int InstFX_Com_FREQUENCY(int parseMode, instrument *target);
int InstFX_Com_LOOP(int parseMode, instrument *target);
int InstFX_Com_ENDLOOP(int parseMode, instrument *target);
int InstFX_Com_NOTEOFF(int parseMode, instrument *target);
int InstFX_Com_TUNING(int parseMode, instrument *target);

int InstFX_AddByte(instrument *target, char val);
instrument* InstFX_GetInst(char *reqInst);
int InstFX_GetInstID(char *reqInst);
int InstFX_ChkInstTable();
void InstFX_FreeTable();
