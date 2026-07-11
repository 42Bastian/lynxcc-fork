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
 * music.h
 * Last change 04-02-2013 by Osman Celimli
 ****************************************************************/
/****************************************************************
 * Important Defines
 ****************************************************************/
/*See programmer's manual for details on the following*/
#define MUS_PRIORITY	0
#define MUS_PANNING		1
#define MUS_NOTE_ON		2
#define MUS_NOTE_OFF	3
#define MUS_FREQ_ADJUST	4
#define MUS_LOOP		5
#define MUS_ENDLOOP		6
#define MUS_WAIT		7
#define MUS_SAMPLE		8
#define MUS_CALL		9
#define MUS_RETURN		10
#define MUS_NOTE_SON	11
#define MUS_BREAK		12
/*Note that all the values are in little endian*/
#define MUS_HDR_PRI		0
#define MUS_HDR_SCRLO	4
#define MUS_HDR_SCRHI	8
#define MUS_HDR_INSTLL	12
#define MUS_HDR_INSTLH	13
#define MUS_HDR_INSTHL	14
#define MUS_HDR_INSTHH	15
#define MUS_HDR_LENGTH	16

#define MUS_PARSER_ISCRIPTSIZE	256
#define MUS_PARSER_ITABLESIZE	32
#define MUS_PARSER_RTABLESIZE	32

typedef struct musStream
{
 char *blockName;
 char *blockStream;
 unsigned int scriptLen,bufferLen;
} musStream;

typedef struct blockTag
{
 int sourceLine;
 char *blockName;
 unsigned short offset;
} blockTag;

typedef int (*musCommEntry)(struct musStream*);

extern int mus_binSize;

/****************************************************************
 * Function Prototypes
 ****************************************************************/
int mus_ParseSrc(FILE *infile, char *infileName);
int mus_tryAddNote(char *noteToken, musStream *target);

char* mus_BuildBin(int baseAddress);

int mus_Com_USING(musStream *target);
int mus_Com_PRIORITY(musStream *target);
int mus_Com_PAN(musStream *target);
int mus_Com_PITCH(musStream *target);
int mus_Com_LOOP(musStream *target);
int mus_Com_ENDLOOP(musStream *target);
int mus_Com_SAMPLE(musStream *target);
int mus_Com_SFX(musStream *target);
int mus_Com_CALL(musStream *target);
int mus_Com_RETURN(musStream *target);
int mus_Com_BREAK(musStream *target);
int mus_Com_REST(musStream *target);
int mus_Com_WAIT(musStream *target);
int mus_Com_END(musStream *target);

int mus_AddByte(musStream *target, char val);
int mus_AddBlock(musStream *target);
int mus_AddCallRef(char *targetName, int targetOffset, int targetLine);
void mus_FreeTable();

