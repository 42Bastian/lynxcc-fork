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
 * music.c
 * Last change 07-09-2013 by Osman Celimli
 ****************************************************************/
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#include "instfx.h"
#include "music.h"
#include "common.h"
#include "notes.h"
/****************************************************************
 * Globals/Externs
 ****************************************************************/
int currentOffset = 0;
int mus_binSize = 0;
int mus_scanLine = 0;

musStream *musBlockTable = NULL;
int musBlockEntries = 0;
int musBlockTableSize = 0;
blockTag *labelResolveTable = NULL;
int labelResolveEntries = 0;
int labelResolveTableSize = 0;

int mainBlockFound[4]={0,0,0,0};
int mainBlockPriorities[4]={0,0,0,0};
int mainBlockOffsets[4]={0,0,0,0};

char *instrumentName = NULL;
int currentInstrument = 0;
int instrumentIsSet = 0;
int instrumentIsFirst = 0;

int currentBlockSet = 0;
int noteIsOn = 0;

/****************************************************************
 * Text Tables
 ****************************************************************/
char ERR_NoCloseM[]="Error- Cannot close music file.\n";
char ERR_NoAllocM[]="Error- Out of Memory.\n";

char ERR_InvalL[]="Error- Unable to resolve label '";
char ERR_InvalI[]="' in : ";
char ERR_MSyntax[]="Error- Invalid command syntax in :";
char ERR_MGarbage[]="Error- Garbage on line in :";

char ERR_MValueOR[]="Error- Operand out of range.\n";
char ERR_MInstNS[]="Error- Instrument not set\n";
char ERR_MInstUndef[]="Error- Instrument not defined : ";
char ERR_UnexEndM[]="Error- Unexpected end of music block in :";
char ERR_MDoubleMain[]="Error- Second main block defined in : ";
char ERR_MDoubleDef[]="Error- Block already defined with label : ";
char ERR_MExpectBrace[]="Error- Expecting '{'\n";

/* lynxcc port: '\r' added so CRLF scripts tokenize identically to the
 * original Windows text-mode read (where fopen stripped the CR). */
char MUS_WhiteSpace[]=" ,\t\r\n";

/****************************************************************
 * Command Functions and Jump Table
 ****************************************************************/
/* using,u instrument */
int mus_Com_USING(musStream *target)
{
	char *tSubTok;
	int reqInstrument;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;

	if((reqInstrument = InstFX_GetInstID(tSubTok)) < 0)
	{
		fprintf(stderr,"%s%s\n",ERR_MInstUndef,tSubTok);
		return -1;
	}
	if((reqInstrument != currentInstrument) || instrumentIsFirst)
	{
		currentInstrument = reqInstrument;
		instrumentIsFirst = 0;
		instrumentIsSet = 0;
		if(instrumentName != NULL) free(instrumentName);
		if((instrumentName = malloc((strlen(tSubTok) + 1) * sizeof(char))) == NULL)
		{
			fprintf(stderr,"%s",ERR_NoAllocM);
			return -1;
		}
		strcpy(instrumentName,tSubTok);
	}
	return 0;
}

/* priority,pr value */
int mus_Com_PRIORITY(musStream *target)
{
	u16p8 priVal;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&priVal,tSubTok)) return -1;
	if(priVal.decByte | priVal.hiByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}

	success = mus_AddByte(target,MUS_PRIORITY);
	success |= mus_AddByte(target,priVal.loByte);
	return success;
}

/* pan,p attenuation */
int mus_Com_PAN(musStream *target)
{
	u16p8 panVal;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&panVal,tSubTok)) return -1;
	if(panVal.decByte | panVal.hiByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}

	success = mus_AddByte(target,MUS_PANNING);
	success |= mus_AddByte(target,panVal.loByte);
	return success;
}

/* pitch,pt adjustment */
int mus_Com_PITCH(musStream *target)
{
	u16p8 pitchVal;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&pitchVal,tSubTok)) return -1;

	success = mus_AddByte(target,MUS_FREQ_ADJUST);
	success |= mus_AddByte(target,pitchVal.loByte);
	success |= mus_AddByte(target,pitchVal.hiByte);
	success |= mus_AddByte(target,pitchVal.decByte);
	return success;
}

/* loop,l count */
int mus_Com_LOOP(musStream *target)
{
	u16p8 loopCount;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&loopCount,tSubTok)) return -1;
	/*Only 8-Bits for loop, but negatives are valid for infinites*/
	if(loopCount.decByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}

	success = mus_AddByte(target,MUS_LOOP);
	success |= mus_AddByte(target,loopCount.loByte);
	instrumentIsSet = 0;
	return success;
}

/* endloop,el */
int mus_Com_ENDLOOP(musStream *target)
{
	return mus_AddByte(target,MUS_ENDLOOP);
}

/* sample,smp number */
int mus_Com_SAMPLE(musStream *target)
{
	u16p8 sampleNum;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&sampleNum,tSubTok)) return -1;
	if(sampleNum.decByte | sampleNum.hiByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}

	success = mus_AddByte(target,MUS_SAMPLE);
	success |= mus_AddByte(target,sampleNum.loByte);
	return success;
}

/* sfx,s ticks [divider] */
int mus_Com_SFX(musStream *target)
{
	u16p8 sfxDelay,sfxDivider;
	char *tSubTok;
	short split = 0;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&sfxDelay,tSubTok)) return -1;
	if(sfxDelay.decByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}
	/* Assume divider of zero if not given */
	sfxDivider.loByte = 0;
	sfxDivider.hiByte = 0;
	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) != NULL)
	{
		if(tSubTok[0] != ';')
		{
			if(parse16p8(&sfxDivider,tSubTok)) return -1;
			if(sfxDivider.decByte)
			{
				fprintf(stderr,"%s",ERR_MValueOR);
				return -1;
			}
		}
		else commentStart = -1;
	}
	
	noteIsOn = 0;
	if(instrumentIsSet) success = mus_AddByte(target,MUS_NOTE_SON);
	else
	{
		success = mus_AddByte(target,MUS_NOTE_ON);
		success |= mus_AddByte(target,(char)(currentInstrument & 0xFF));
		instrumentIsSet = 1;
	}

	if(sfxDelay.hiByte)
	{
		split = (sfxDelay.hiByte << 8) | sfxDelay.loByte;
		split -= 255;
		sfxDelay.hiByte = (split >> 8) & 0xFF;
		sfxDelay.loByte = split & 0xFF;
		success = mus_AddByte(target,sfxDivider.loByte);
		success |= mus_AddByte(target,sfxDivider.hiByte);
		success |= mus_AddByte(target,(char)0xFF);
		success |= mus_AddByte(target,MUS_WAIT);
		success |= mus_AddByte(target,sfxDelay.loByte);
		success |= mus_AddByte(target,sfxDelay.hiByte);
	}
	else
	{
		success = mus_AddByte(target,sfxDivider.loByte);
		success |= mus_AddByte(target,sfxDivider.hiByte);
		success |= mus_AddByte(target,sfxDelay.loByte);
	}
	return success;
}

/* call,c blockLabel */
int mus_Com_CALL(musStream *target)
{
	char *tSubTok;
	int success = 0;
	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;

	success = mus_AddByte(target,MUS_CALL);
	success |= mus_AddCallRef(tSubTok,currentOffset,mus_scanLine);
	success |= mus_AddByte(target,0);
	success |= mus_AddByte(target,0);
	instrumentIsSet = 0;
	return success;
}

/* return,rt */
int mus_Com_RETURN(musStream *target)
{
	return mus_AddByte(target,MUS_RETURN);
}

/* break,b */
int mus_Com_BREAK(musStream *target)
{
	return mus_AddByte(target,MUS_BREAK);
}

/* rest,r ticks */
int mus_Com_REST(musStream *target)
{
	u16p8 restDelay;
	char *tSubTok;
	int split = 0;
	int success = 0;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&restDelay,tSubTok)) return -1;
	if(restDelay.decByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}

	if(noteIsOn)
	{
		noteIsOn = 0;
		if(restDelay.hiByte)
		{
			split = ((restDelay.hiByte << 8) & 0xFF00) | (restDelay.loByte & 0xFF);
			split -= 255;
			restDelay.hiByte = (split >> 8) & 0xFF;
			restDelay.loByte = split & 0xFF;
			success = mus_AddByte(target,MUS_NOTE_OFF);
			success |= mus_AddByte(target,(char)0xFF);
			success |= mus_AddByte(target,MUS_WAIT);
			success |= mus_AddByte(target,restDelay.loByte);
			success |= mus_AddByte(target,restDelay.hiByte);
		}
		else
		{
			success = mus_AddByte(target,MUS_NOTE_OFF);
			success |= mus_AddByte(target,restDelay.loByte);
		}
	}
	else
	{
		success = mus_AddByte(target,MUS_WAIT);
		success |= mus_AddByte(target,restDelay.loByte);
		success |= mus_AddByte(target,restDelay.hiByte);
	}
	return success;
}

/* wait,w ticks */
int mus_Com_WAIT(musStream *target)
{
	u16p8 restDelay;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&restDelay,tSubTok)) return -1;
	if(restDelay.decByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}

	success = mus_AddByte(target,MUS_WAIT);
	success |= mus_AddByte(target,restDelay.loByte);
	success |= mus_AddByte(target,restDelay.hiByte);
	return success;
}

/* end,e */
int mus_Com_END(musStream *target)
{
	int success;
	success = mus_AddByte(target,MUS_PRIORITY);
	success |= mus_AddByte(target,0);
	return success;
}

#define MUS_NUMCOMMANDS 14
char musComm_LongNames[][10]={"using","priority",
	"pan","pitch","loop","endloop",
	"sample","sfx","call","return",
	"break",
	"rest","wait","end"};
char musComm_ShortNames[][3]={"u","pr",
	"p","pt","l","el",
	"smp","s","c","rt",
	"b",
	"r","w","e"};
musCommEntry musComm_Dest[]={mus_Com_USING,mus_Com_PRIORITY,
	mus_Com_PAN,mus_Com_PITCH,mus_Com_LOOP,mus_Com_ENDLOOP,
	mus_Com_SAMPLE,mus_Com_SFX,mus_Com_CALL,mus_Com_RETURN,
	mus_Com_BREAK,
	mus_Com_REST,mus_Com_WAIT,mus_Com_END};
/****************************************************************
 * Parsing Functions
 ****************************************************************/
/****************************************************************
 * int mus_ParseSrc(FILE *infile, char *infileName)
 * Parse the given input music track, adding its blocks to
 * the current tables.
 * Returns zero on success, and -1 on any failure.
 * The file is closed after completion.
 ****************************************************************/
int mus_ParseSrc(FILE *infile, char *infileName)
{
	musStream tStream;
	char *inbuf,*tComm,*tSubTok;
	int parsingBlock = 0;
	int success = -1;
	int commChk,tPri,ix;

	tStream.blockName = NULL;
	tStream.blockStream = NULL;
	instrumentName = NULL;

	mus_scanLine = 0;
	instrumentIsFirst = -1;
	instrumentIsSet = 0;
	noteIsOn = 0;

	/* Ready the linebuffers */
	if((inbuf = malloc(SFX_PARSER_BSIZE)) == NULL)
	{
		fprintf(stderr,"%s",ERR_NoAllocM);
		goto closeUp;
	}
	
	/* Parse the File... */
	while(1)
	{
		mus_scanLine++;
		commentStart = 0;
		/*Are we at the end?*/
		if(fgets(inbuf,SFX_PARSER_BSIZE,infile) == NULL)
		{
			success = 0;
			break;
		}
		/*Tokenize the current line*/
		tComm = strtok(inbuf,MUS_WhiteSpace);

		/*Are we already parsing a block?*/
		if((tComm != NULL) && parsingBlock)
		{
			/*Was it a comment?*/
			if(tComm[0] == ';') continue;
			/*Have we found the first curlybrace yet?*/
			if(parsingBlock == 1)
			{
				if(strcmp(tComm,"{") == 0) parsingBlock = 2;
				else
				{
					fprintf(stderr,"%s",ERR_MExpectBrace);
					break;
				}
			}
			/* Alright, let's interpret some commands */
			else
			{
				for(commChk = (MUS_NUMCOMMANDS-1); commChk >= 0; commChk--)
				{
					if((strcmp(tComm,musComm_LongNames[commChk]) == 0) ||
						(strcmp(tComm,musComm_ShortNames[commChk]) == 0)) break;
				}
				/* If we couldn't find a match, try a note or '}' */
				if(commChk < 0)
				{
					/* Block End? */
					if(strcmp(tComm,"}") == 0)
					{
						if(mus_AddBlock(&tStream))
						{
							fprintf(stderr,"%s",ERR_NoAllocM);
							goto closeUp;
						}
						tStream.bufferLen = 0;
						tStream.scriptLen = 0;
						parsingBlock = 0;
					}
					/* Note? */
					else if(mus_tryAddNote(tComm,&tStream))
					{
						success = -1;
						break;
					}
				}
				/* Must be a command */
				else
				{
					if((*musComm_Dest[commChk])(&tStream))
					{
						success = -1;
						break;
					}
				}
			}
			/*Any garbage?*/
			if(InstFX_hasGarbage())
			{
				fprintf(stderr,"%s %s,%d\n",ERR_MGarbage,infileName,mus_scanLine);
				break;
			}
		}
		/*No? Have we found a define yet?*/
		else if(tComm != NULL)
		{
			/*Skip the line if it's a comment*/
			if(tComm[0] == ';') continue;

			tPri = -1;
			/*See if we can get a number here... */
			if((tSubTok = strtok(NULL,MUS_WhiteSpace)) != NULL)
			{
				if(sscanf(tSubTok,"%d",&tPri) == 1)
				{
					if((tPri < 0) || (tPri > 255))
					{
						fprintf(stderr,"%s",ERR_MValueOR);
						success = -1;
						break;
					}
					if(mainBlockFound[currentBlockSet])
					{
						fprintf(stderr,"%s%s,%s",ERR_MDoubleMain,infileName,mus_scanLine);
						success = -1;
						break;
					}
					/*Alright, here's our main block for this file*/
					mainBlockFound[currentBlockSet] = -1;
					mainBlockPriorities[currentBlockSet] = tPri;
					mainBlockOffsets[currentBlockSet] = currentOffset;
				}
			}
			/*Is there any garbage at the end of the line?*/
			if(InstFX_hasGarbage())
			{
				fprintf(stderr,"%s %s,%d\n",ERR_MGarbage,infileName,mus_scanLine);
				success = -1;
				break;
			}
			/*Does a block with this name already exist?*/
			for(ix = 0; ix < musBlockEntries; ix++)
			{
				if(strcmp(musBlockTable[ix].blockName,tComm) == 0)
				{
					fprintf(stderr,"%s%s\n",ERR_MDoubleDef,tComm);
					success = -1;
					goto preCloseUp;
				}
			}

			/*Okay, let's start parsing that block!*/
			tStream.blockName = malloc((strlen(tComm) + 1) * sizeof(char));
			strcpy(tStream.blockName,tComm);
			tStream.blockStream = malloc(MUS_PARSER_ISCRIPTSIZE * sizeof(char));
			tStream.bufferLen = MUS_PARSER_ISCRIPTSIZE;
			tStream.scriptLen = 0;
			parsingBlock = 1;
			instrumentIsSet = 0;
			noteIsOn = 0;
			if((tStream.blockName == NULL) || (tStream.blockStream == NULL))
			{
				fprintf(stderr,"%s",ERR_NoAllocM);
				goto closeUp;
			}
		}
	}
preCloseUp:
	if(success) fprintf(stderr,"%s %s,%d\n",ERR_MSyntax,infileName,mus_scanLine);
	if(parsingBlock)
	{
		fprintf(stderr,"%s %s,%d\n",ERR_UnexEndM,infileName,mus_scanLine);
		success = -1;
	}
closeUp:
	if(inbuf != NULL) free(inbuf);
	if(tStream.blockName != NULL) free(tStream.blockName);
	if(tStream.blockStream != NULL) free(tStream.blockStream);
	if(instrumentName != NULL) free(instrumentName);
	if(infile != NULL) 
	{
		if(fclose(infile) == EOF)
		{
			fprintf(stderr,"%s",ERR_NoCloseM);
			success = -1;
		}
	}
	currentBlockSet++;
	return success;
}
/****************************************************************
 * int mus_tryAddNote(musStream *target)
 * Attempt to interpret the given token as a note, and if it
 * is one, parse the rest of the line as a note command.
 * Returns 0 on success and -1 on failure.
 ****************************************************************/
int mus_tryAddNote(char *noteToken, musStream *target)
{
	double noteFreq = Note_StrtoFreq(noteToken);
	instrument *cInstrument;
	u16p8 sepDivider,noteDelay;
	int success = 0;
	int split;
	short divider;
	char *tSubTok = NULL;

	/* Attempt to parse note */
	if(noteFreq < 0.0) return -1;
	if(instrumentName == NULL)
	{
		fprintf(stderr,"%s",ERR_MInstNS);
		return -1;
	}
	if((cInstrument = InstFX_GetInst(instrumentName)) == NULL)
	{
		fprintf(stderr,"%s%s\n",ERR_MInstUndef,cInstrument);
		return -1;
	}
	divider = Note_FreqtoDiv(noteFreq,(cInstrument->tuning));
	sepDivider.loByte = divider & 0xFF;
	sepDivider.hiByte = (divider >> 8) & 0xFF;

	noteIsOn = 1;
	if(instrumentIsSet) success = mus_AddByte(target,MUS_NOTE_SON);
	else
	{
		success |= mus_AddByte(target,MUS_NOTE_ON);
		success |= mus_AddByte(target,(char)(currentInstrument & 0xFF));
		instrumentIsSet = 1;
	}

	/* Now the delay */
	if((tSubTok = strtok(NULL,MUS_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&noteDelay,tSubTok)) return -1;
	if(noteDelay.decByte)
	{
		fprintf(stderr,"%s",ERR_MValueOR);
		return -1;
	}
	if(noteDelay.hiByte)
	{
		split = ((noteDelay.hiByte) << 8 & 0xFF00) | (noteDelay.loByte & 0xFF);
		split -= 255;
		noteDelay.hiByte = (split >> 8) & 0xFF;
		noteDelay.loByte = split & 0xFF;
		success |= mus_AddByte(target,sepDivider.loByte);
		success |= mus_AddByte(target,sepDivider.hiByte);
		success |= mus_AddByte(target,(char)0xFF);
		success |= mus_AddByte(target,MUS_WAIT);
		success |= mus_AddByte(target,noteDelay.loByte);
		success |= mus_AddByte(target,noteDelay.hiByte);
	}
	else
	{
		success |= mus_AddByte(target,sepDivider.loByte);
		success |= mus_AddByte(target,sepDivider.hiByte);
		success |= mus_AddByte(target,noteDelay.loByte);
	}
	return success;
}

/****************************************************************
 * Music Data Management Functions
 ****************************************************************/
/****************************************************************
 * int mus_AddByte(instrument *target, char val)
 * Add the given value to the music block. Will return zero
 * on success and -1 if any problems occur.
 ****************************************************************/
int mus_AddByte(musStream *target, char val)
{
	int newBufLen = target->bufferLen + MUS_PARSER_ISCRIPTSIZE;
	char *t_stream;

	if(target->scriptLen == target->bufferLen)
	{
		if((t_stream = realloc(target->blockStream,newBufLen*sizeof(char))) == NULL)
		{
			fprintf(stderr,"%s",ERR_NoAllocM);
			return -1;
		}
		target->blockStream = t_stream;
		target->bufferLen = newBufLen;
	}
	target->blockStream[target->scriptLen++] = val;
	currentOffset++;
	return 0;
}
/****************************************************************
 * int mus_AddBlock(musStream *target)
 * Attempt to add the given music block to the block table,
 * should be done once block parsing is complete.
 * Returns zero on success and -1 on failure.
 ****************************************************************/
int mus_AddBlock(musStream *target)
{
	musStream *tempTable = NULL;
	
	if((musBlockEntries + 1) > musBlockTableSize)
	{
		if((tempTable = realloc(musBlockTable,(musBlockTableSize + MUS_PARSER_ITABLESIZE) * sizeof(musStream))) == NULL)
		{
			fprintf(stderr,"%s",ERR_NoAllocM);
			return -1;
		}
		musBlockTable = tempTable;
		musBlockTableSize += MUS_PARSER_ITABLESIZE;
	}
	musBlockTable[musBlockEntries].blockName = target->blockName;
	musBlockTable[musBlockEntries].blockStream = target->blockStream;
	musBlockTable[musBlockEntries].bufferLen = target->bufferLen;
	musBlockTable[musBlockEntries].scriptLen = target->scriptLen;
	musBlockEntries++;

	target->blockName = NULL;
	target->blockStream = NULL;
	return 0;
}
/****************************************************************
 * int mus_AddCallRef(char *targetName, int targetOffset, int targetLine)
 * Attempt to add a call reference to the resolve table.
 * Returns zero on success and -1 on failure.
 ****************************************************************/
int mus_AddCallRef(char *targetName, int targetOffset, int targetLine)
{
	blockTag *tempTable = NULL;

	/* Ensure resolve table has space */
	if((labelResolveEntries + 1) > labelResolveTableSize)
	{
		if((tempTable = realloc(labelResolveTable,(labelResolveTableSize + MUS_PARSER_RTABLESIZE) * sizeof(blockTag))) == NULL)
		{
			fprintf(stderr,"%s",ERR_NoAllocM);
			return -1;
		}
		labelResolveTable = tempTable;
		labelResolveTableSize += MUS_PARSER_RTABLESIZE;
	}
	/* Copy over the resolve info */
	labelResolveTable[labelResolveEntries].blockName = malloc((strlen(targetName) + 1) * sizeof(char));
	if(labelResolveTable[labelResolveEntries].blockName == NULL)
	{
		fprintf(stderr,"%s",ERR_NoAllocM);
		return -1;
	}
	strcpy(labelResolveTable[labelResolveEntries].blockName,targetName);
	labelResolveTable[labelResolveEntries].offset = targetOffset;
	labelResolveTable[labelResolveEntries].sourceLine = targetLine;
	labelResolveEntries++;
	return 0;
}
/****************************************************************
 * void mus_FreeTable()
 ****************************************************************/
void mus_FreeTable()
{
	int relOff = 0;

	if(musBlockTable != NULL)
	{
		while(relOff < musBlockEntries)
		{
			free(musBlockTable[relOff].blockName);
			free(musBlockTable[relOff].blockStream);
			relOff++;
		}
		free(musBlockTable);
	}
	relOff = 0;
	if(labelResolveTable != NULL)
	{
		while(relOff < labelResolveEntries)
		{
			free(labelResolveTable[relOff].blockName);
			relOff++;
		}
		free(labelResolveTable);
	}
}

/****************************************************************
 * Building Functions
 ****************************************************************/
/****************************************************************
 * char* mus_BuildBin(int baseAddress)
 * Attempt to build the music binary and resolve the CALL / RETURN
 * addresses encountered during parsing. Returns NULL if the
 * operation failed, and the completeted binary if the build
 * was successful (exluding instrument data which must 
 * be copied in after). "mus_binSize" is also set to the size
 * of the binary.
 ****************************************************************/
char* mus_BuildBin(int baseAddress)
{
	char *musBin = NULL;
	int reqBinSize,blockAddress;
	int ix,iy;

	/* Get required binary size */
	reqBinSize = instfx_binSize + MUS_HDR_LENGTH;
	for(ix = 0; ix < musBlockEntries; ix++) reqBinSize += musBlockTable[ix].scriptLen;
	if((musBin = malloc(reqBinSize)) == NULL)
	{
		fprintf(stderr,"%s",ERR_NoAllocM);
		return NULL;
	}
	mus_binSize = reqBinSize;

	/* Copy Blocks */
	iy = instfx_binSize + MUS_HDR_LENGTH;
	for(ix = 0; ix < musBlockEntries; ix++)
	{
		memcpy(musBin + iy,musBlockTable[ix].blockStream,musBlockTable[ix].scriptLen);
		iy += musBlockTable[ix].scriptLen;
	}

	/* Resolve the CALL / RETURN addresses */
	for(ix = 0; ix < labelResolveEntries; ix++)
	{
		blockAddress = baseAddress + instfx_binSize + MUS_HDR_LENGTH;
		for(iy = 0; iy < musBlockEntries; iy++)
		{
			if(strcmp(labelResolveTable[ix].blockName,musBlockTable[iy].blockName) == 0) break;
			blockAddress += musBlockTable[iy].scriptLen;
		}
		/* Couldn't resolve label, error out */
		if(iy == musBlockEntries)
		{
			fprintf(stderr,"%s%s%s%d\n",ERR_InvalL,labelResolveTable[ix].blockName,ERR_InvalI,labelResolveTable[ix].sourceLine);
			free(musBin);
			return NULL;
		}
		/* Stamp & Go */
		musBin[labelResolveTable[ix].offset + instfx_binSize + MUS_HDR_LENGTH] = blockAddress & 0xFF;
		musBin[labelResolveTable[ix].offset + 1 + instfx_binSize + MUS_HDR_LENGTH] = (blockAddress >> 8) & 0xFF;
	}

	/* Add Priorities & Pointers */
	for(ix = 0; ix < 4; ix++)
	{
		musBin[ix + MUS_HDR_PRI] = mainBlockPriorities[ix] & 0xFF;
		musBin[ix + MUS_HDR_SCRLO] = (mainBlockOffsets[ix] + baseAddress + instfx_binSize + MUS_HDR_LENGTH) & 0xFF;
		musBin[ix + MUS_HDR_SCRHI] = ((mainBlockOffsets[ix] + baseAddress + instfx_binSize + MUS_HDR_LENGTH) >> 8) & 0xFF;
	}
	musBin[MUS_HDR_INSTLL] = (baseAddress + MUS_HDR_LENGTH) & 0xFF;
	musBin[MUS_HDR_INSTLH] = ((baseAddress + MUS_HDR_LENGTH) >> 8) & 0xFF;
	musBin[MUS_HDR_INSTHL] = (instfx_hTableOff + baseAddress + MUS_HDR_LENGTH) & 0xFF;
	musBin[MUS_HDR_INSTHH] = ((instfx_hTableOff + baseAddress + MUS_HDR_LENGTH) >> 8) & 0xFF;
	return musBin;
}

