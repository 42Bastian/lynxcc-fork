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
 * instfx.c
 * Last change 07-09-2013 by Osman Celimli
 ****************************************************************/
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "instfx.h"
/****************************************************************
 * Globals/Externs
 ****************************************************************/
int commentStart = 0;

int instfx_binSize = 0;
int instfx_pTableOff = 0;
int instfx_lTableOff = 0;
int instfx_hTableOff = 0;

instrument *instTable = NULL;
int tableLen = 0;
int tableBufLen = 0;

/****************************************************************
 * Text Tables
 ****************************************************************/
char ERR_NoClose[]="Error- Cannot close instrument/sfx file.\n";
char ERR_NoAlloc[]="Error- Out of Memory.\n";

char ERR_BadHeader[]="Error- Unable to parse instrument/sfx header in : ";
char ERR_DuplicateF[]="Error- Instrument '";
char ERR_DuplicateE[]="' already exists :";

char ERR_Garbage[]="Error- Garbage on line in :";
char ERR_UnexEnd[]="Error- Unexpected end of instrument in :";
char ERR_CSyntax[]="Error- Invalid command syntax in :";
char ERR_InvalF[]="Error- Invalid command '";
char ERR_InvalE[]="' in :";

char ERR_CEndHead[]="Error- Can't END in instrument header.\n";
char ERR_CRestHead[]="Error- Can't REST in instrument header.\n";
char ERR_CLoopHead[]="Error- Can't LOOP in instrument header.\n";
char ERR_CENoteHead[]="Error- Can't set NOTE END in instrument header.\n";
char ERR_CTuneHead[]="Error- Can only set TUNING in instrument header.\n";
char ERR_CValueOR[]="Error- Operand out of range.\n";

char ERR_NoInstr[]="Error- No instruments/sfx found in source.\n";

char STA_SfxCnt[]=" instruments/sfx were found\n";
char STA_SfxSiz[]="The current instrument/sfx table is: ";

char ERR_SfxEqu[]="Error- Couldn't create equate file for sfx.\n";

/* lynxcc port: '\r' added so CRLF scripts tokenize identically to the
 * original Windows text-mode read (where fopen stripped the CR). */
char SFX_WhiteSpace[]=" ,\t\r\n";

/****************************************************************
 * Command Functions and Jump Table
 ****************************************************************/
/* end, e */
int InstFX_Com_END(int parseMode, instrument *target)
{
	if(parseMode != 2)
	{
		fprintf(stderr,"%s",ERR_CEndHead);
		return -1;
	}
	return InstFX_AddByte(target,SFX_STOP);
}

/* rest, r [delay]*/
int InstFX_Com_REST(int parseMode, instrument *target)
{
	u16p8 restDelay;
	char *tSubTok;
	int success;

	if(parseMode != 2)
	{
		fprintf(stderr,"%s",ERR_CRestHead);
		return -1;
	}
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&restDelay,tSubTok)) return -1;

	if(restDelay.hiByte || restDelay.decByte)
	{
		fprintf(stderr,"%s",ERR_CValueOR);
		return -1;
	}

	success = InstFX_AddByte(target,SFX_WAIT);
	success |= InstFX_AddByte(target,restDelay.loByte);
	return success;
}

/* waveform, w [feedback] [shift]*/
int InstFX_Com_WAVEFORM(int parseMode, instrument *target)
{
	u16p8 shSettings,fbSettings;
	char *tSubTok;
	int success;

	/*Always get the new feedback settings*/
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&fbSettings,tSubTok)) return -1;

	/*But the shift register will be defaulted to zero if nothing is given*/
	shSettings.decByte = 0;
	shSettings.loByte = 0;
	shSettings.hiByte = 0;
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) != NULL)
	{
		if(tSubTok[0] != ';')
		{
			if(parse16p8(&shSettings,tSubTok)) return -1;
		}
		else commentStart = -1;
	}

	if(fbSettings.decByte || shSettings.decByte)
	{
		fprintf(stderr,"%s",ERR_CValueOR);
		return -1;
	}

	/*Pre or Post header waveform?*/
	if(parseMode == 2)
	{
		success = InstFX_AddByte(target,SFX_SETWF);
		success |= InstFX_AddByte(target,shSettings.loByte);
		success |= InstFX_AddByte(target,shSettings.hiByte);
		success |= InstFX_AddByte(target,fbSettings.loByte);
		success |= InstFX_AddByte(target,fbSettings.hiByte);
	}
	else
	{
		success = 0;
		target->instStream[SFX_HDR_SHIFT] = shSettings.loByte;
		target->instStream[SFX_HDR_SHIFT + 1] = shSettings.hiByte;
		target->instStream[SFX_HDR_FEEDBI] = fbSettings.loByte;
		target->instStream[SFX_HDR_FEEDBI + 1] = fbSettings.hiByte;
	}
	return success;
}

/* volume, v [amplitude] [adjustment]*/
int InstFX_Com_VOLUME(int parseMode, instrument *target)
{
	u16p8 volume,adjustment;
	char *tSubTok;
	int success;

	/*Always fetch the new volume*/
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&volume,tSubTok)) return -1;

	/*But the volume adjustment is optional (defaulted to zero)*/
	adjustment.decByte = 0;
	adjustment.loByte = 0;
	adjustment.hiByte = 0;
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) != NULL)
	{
		if(tSubTok[0] != ';')
		{
			if(parse16p8(&adjustment,tSubTok)) return -1;
		}
		else commentStart = -1;
	}

	if(volume.decByte || volume.hiByte)
	{
		fprintf(stderr,"%s",ERR_CValueOR);
		return -1;
	}

	/*Pre or Post header volume?*/
	if(parseMode == 2)
	{
		success = InstFX_AddByte(target,SFX_SETVOL);
		success |= InstFX_AddByte(target,volume.loByte);
		success |= InstFX_AddByte(target,adjustment.loByte);
		success |= InstFX_AddByte(target,adjustment.decByte);
	}
	else
	{
		success = 0;
		target->instStream[SFX_HDR_VOLI] = volume.loByte;
		target->instStream[SFX_HDR_VOLADJ] = adjustment.loByte;
		target->instStream[SFX_HDR_VOLADJ + 1] = adjustment.decByte;
	}
	return success;
}

/* frequency, f [divider] [adjustment]*/
int InstFX_Com_FREQUENCY(int parseMode, instrument *target)
{
	u16p8 divider,adjustment;
	char *tSubTok;
	int success;

	/*Always fetch the new divider*/
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&divider,tSubTok)) return -1;

	/*But the divider adjustment is optional (defaulted to zero)*/
	adjustment.decByte = 0;
	adjustment.loByte = 0;
	adjustment.hiByte = 0;
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) != NULL)
	{
		if(tSubTok[0] != ';')
		{
			if(parse16p8(&adjustment,tSubTok)) return -1;
		}
		else commentStart = -1;
	}

	/*No divider decimals*/
	if(divider.decByte)
	{
		fprintf(stderr,"%s",ERR_CValueOR);
		return -1;
	}

	/*Pre or Post header frequency?*/
	if(parseMode == 2)
	{
		success = InstFX_AddByte(target,SFX_SETFREQ);
		success |= InstFX_AddByte(target,divider.loByte);
		success |= InstFX_AddByte(target,divider.hiByte);
		success |= InstFX_AddByte(target,adjustment.loByte);
		success |= InstFX_AddByte(target,adjustment.hiByte);
		success |= InstFX_AddByte(target,adjustment.decByte);
	}
	else
	{
		success = 0;
		target->instStream[SFX_HDR_FREQI] = divider.loByte;
		target->instStream[SFX_HDR_FREQI + 1] = divider.hiByte;
		target->instStream[SFX_HDR_FREQADJ] = adjustment.loByte;
		target->instStream[SFX_HDR_FREQADJ + 1] = adjustment.hiByte;
		target->instStream[SFX_HDR_FREQADJ + 2] = adjustment.decByte;
	}
	return success;
}

/* loop, l [count]*/
int InstFX_Com_LOOP(int parseMode, instrument *target)
{
	u16p8 loopCount;
	char *tSubTok;
	int success;

	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) return -1;
	if(parse16p8(&loopCount,tSubTok)) return -1;

	/*Only 8-Bits for loop, but negatives are valid for infinites*/
	if(loopCount.decByte)
	{
		fprintf(stderr,"%s",ERR_CValueOR);
		return -1;
	}

	/*No loops in headers*/
	if(parseMode != 2)
	{
		fprintf(stderr,"%s",ERR_CLoopHead);
		return -1;
	}
	success = InstFX_AddByte(target,SFX_LOOP);
	success |= InstFX_AddByte(target,loopCount.loByte);
	return success;
}

/* endloop, el */
int InstFX_Com_ENDLOOP(int parseMode, instrument *target)
{
	/*No ending loops in the header either*/
	if(parseMode != 2)
	{
		fprintf(stderr,"%s",ERR_CLoopHead);
		return -1;
	}

	return InstFX_AddByte(target,SFX_ENDLOOP);
}

/* noteoff, n */
int InstFX_Com_NOTEOFF(int parseMode, instrument *target)
{
	/*No ending the note in the header*/
	if(parseMode != 2)
	{
		fprintf(stderr,"%s",ERR_CENoteHead);
		return -1;
	}
	/*We're going to set the header value after the block is built, so just this for now...*/
	target->noteOffOff = target->scriptLen;
	return 0;
}

/* tuning, t */
int InstFX_Com_TUNING(int parseMode, instrument *target)
{
	char *tSubTok;
	double tuning;

	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) return -1;
	if(sscanf(tSubTok,"%lf",&tuning) == 0) return -1;
	if(tuning < 0.0)
	{
		fprintf(stderr,"%s",ERR_CValueOR);
		return -1;
	}

	/*No tuning outside of the header*/
	if(parseMode != 1)
	{
		fprintf(stderr,"%s",ERR_CTuneHead);
		return -1;
	}

	target->tuning = tuning;
	return 0;
}

#define SFX_NUMCOMMANDS 9
char instComm_LongNames[][10]={"end","rest","waveform","volume","frequency",
	"loop","endloop","noteoff","tuning"};
char instComm_ShortNames[][3]={"e","r","w","v","f","l","el","n","t"};
instCommEntry instComm_Dest[]={InstFX_Com_END,InstFX_Com_REST,InstFX_Com_WAVEFORM,
	InstFX_Com_VOLUME,InstFX_Com_FREQUENCY,InstFX_Com_LOOP,InstFX_Com_ENDLOOP,
	InstFX_Com_NOTEOFF,InstFX_Com_TUNING};

/****************************************************************
 * Parsing Functions
 ****************************************************************/
/****************************************************************
 * int InstFX_ParseSrc(int sfxMode, FILE *infile, char *infileName)
 * Generate the Instrument or Sound Effect table from the given
 * input file. Will return 0 if the operation is sucessful, and
 * -1 if a problem was encountered.
 * The file is closed after the operation is completed.
 ****************************************************************/
int InstFX_ParseSrc(int sfxMode, FILE *infile, char *infileName)
{
	int success = -1;
	int scanLine = 0;
	int parseMode = 0;
	int scanRes,commChk;
	char *inbuf,*tComm,*tSubTok;
	instrument tempInst;

	/*Ready the linebuffers*/
	if((inbuf = malloc(SFX_PARSER_BSIZE)) == NULL)
	{
		fprintf(stderr,"%s",ERR_NoAlloc);
		goto closeUp;
	}
	/*Get some table entries ready*/
	if(InstFX_ChkInstTable()) goto closeUp;

	/*Parse the File...*/
	while(1)
	{
		scanLine++;
		commentStart = 0;
		/*Are we at the end?*/
		if(fgets(inbuf,SFX_PARSER_BSIZE,infile) == NULL)
		{
			success = 0;
			break;
		}
		/*Tokenize the current line*/
		tComm = strtok(inbuf,SFX_WhiteSpace);

		/*Are we parsing an instrument?*/
		if((tComm != NULL) && parseMode)
		{
			/*Ok, so let's get the current command*/
			for(commChk = (SFX_NUMCOMMANDS-1); commChk >= 0; commChk--)
			{
				if((strcmp(tComm,instComm_ShortNames[commChk]) == 0) ||
					(strcmp(tComm,instComm_LongNames[commChk]) == 0)) break;
			}
			/*Wait, was it a comment?*/
			if((commChk < 0) && (tComm[0] == ';')) continue;
			/*Or something else?*/
			else if(commChk < 0 )
			{
				/*Parse Mode Switch?*/
				if(strcmp(tComm,"{") == 0)
				{
					/*Looks like we're now in post-header mode*/
					parseMode = 2;
				}
				/*Block End?*/
				else if(strcmp(tComm,"}") == 0)
				{
					/*Done with the instrument, copy & cleanup*/
					tableLen++;
					if(InstFX_ChkInstTable()) break;
					parseMode = 0;

					instTable[tableLen-1].bufferLen = tempInst.bufferLen;
					instTable[tableLen-1].instName = tempInst.instName;
					instTable[tableLen-1].instStream = tempInst.instStream;
					instTable[tableLen-1].noteOffOff = tempInst.noteOffOff;
					instTable[tableLen-1].priority = tempInst.priority;
					instTable[tableLen-1].scriptLen = tempInst.scriptLen;
					instTable[tableLen-1].tuning = tempInst.tuning;
				}
				/*Otherwise it's definitely a bad command*/
				else
				{
					fprintf(stderr,"%s%s%s %s,%d\n",ERR_InvalF,tComm,
						ERR_InvalE,infileName,scanLine);
					break;
				}

				/*Any garbage?*/
				if(InstFX_hasGarbage())
				{
					fprintf(stderr,"%s %s,%d\n",ERR_Garbage,infileName,scanLine);
					break;
				}
				continue;
			}
			/*Alright, let's call the sub-command*/
			scanRes = (*instComm_Dest[commChk])(parseMode,&tempInst);

			/*Did it work properly?*/
			if(scanRes)
			{
				fprintf(stderr,"%s %s,%d\n",ERR_CSyntax,infileName,scanLine);
				break;
			}
			/*Any garbage?*/
			if(InstFX_hasGarbage())
			{
				fprintf(stderr,"%s %s,%d\n",ERR_Garbage,infileName,scanLine);
				break;
			}
		}
		/*No... so is there an instrument define here?*/
		else if(tComm != NULL)
		{
			/*Skip the line if it's a comment*/
			if(tComm[0] == ';') continue;
			/*See if we can get a number here... But only in SFX mode.*/
			else if(sfxMode && (((tSubTok = strtok(NULL,SFX_WhiteSpace)) == NULL) ||
				((scanRes = sscanf(tSubTok,"%d",&(tempInst.priority))) == 0)))
			{
				fprintf(stderr,"%s %s,%d\n",ERR_BadHeader,infileName,scanLine);
				break;
			}
			/*Is there any garbage at the end of the line?*/
			else if(InstFX_hasGarbage())
			{
				fprintf(stderr,"%s %s,%d\n",ERR_Garbage,infileName,scanLine);
				break;
			}
			/*If there's only a string and a decimal, we've found an instrument*/
			else
			{
				/*Has it already been defined?*/
				if(InstFX_GetInstID(tComm) != -1)
				{
					fprintf(stderr,"%s%s%s %s,%d\n",ERR_DuplicateF,tComm,
						ERR_DuplicateE,infileName,scanLine);
					break;
				}

				/*Nope, so let's get the temporary ready*/
				if((tempInst.instName = malloc(strlen(tComm) + 1)) == NULL)
				{
					fprintf(stderr,"%s",ERR_NoAlloc);
					break;
				}
				strcpy(tempInst.instName,tComm);
				tempInst.priority = tempInst.priority & 0xFF;
				if((tempInst.instStream = malloc(SFX_PARSER_ISSIZE)) == NULL)
				{
					fprintf(stderr,"%s",ERR_NoAlloc);
					break;
				}
				tempInst.bufferLen = SFX_PARSER_ISSIZE;
				tempInst.scriptLen = SFX_HDR_LENGTH;
				tempInst.noteOffOff = SFX_HDR_LENGTH;
				tempInst.tuning = 2.0;	/*Default tuning is for a square pulse*/
				memset(tempInst.instStream,0,SFX_HDR_LENGTH);

				/*Now parse the instrument data*/
				parseMode = 1;
			}
		}
	}

	/* We're done, clean up and release the file */
closeUp:
	/*Were we still parsing an instrument?*/
	if(parseMode)
	{
		free(tempInst.instName);
		free(tempInst.instStream);
		fprintf(stderr,"%s %s,%d\n",ERR_UnexEnd,infileName,scanLine);
		success = -1;
	}
	if(inbuf != NULL) free(inbuf);
	if(infile != NULL) 
	{
		if(fclose(infile) == EOF)
		{
			fprintf(stderr,"%s",ERR_NoClose);
			success = -1;
		}
	}
	return success;
}
/****************************************************************
 * int InstFX_hasGarbage()
 * Check for garbage at the end of the current line. Returns 0
 * if there is either nothing or a comment at the end of the
 * line, or -1 if garbage was detected.
 ****************************************************************/
int InstFX_hasGarbage()
{
	char *tSubTok = NULL;
	if((tSubTok = strtok(NULL,SFX_WhiteSpace)) != NULL)
	{
		if((tSubTok[0] != ';') && !commentStart) return -1;
	}
	return 0;
}

/****************************************************************
 * Instrument Data Management Functions
 ****************************************************************/
/****************************************************************
 * int InstFX_AddByte(instrument *target, char val)
 * Add the given value to the instrument's data stream. Will
 * return zero on success and -1 if any problems occur.
 ****************************************************************/
int InstFX_AddByte(instrument *target, char val)
{
	char *t_stream;

	if(target->scriptLen == target->bufferLen)
	{
		if((t_stream = realloc(target->instStream,(target->bufferLen + SFX_PARSER_ISSIZE)*sizeof(char))) == NULL)
		{
			fprintf(stderr,"%s",ERR_NoAlloc);
			return -1;
		}
		target->instStream = t_stream;
		target->bufferLen += SFX_PARSER_ISSIZE;
	}
	target->instStream[target->scriptLen++] = val;
	return 0;
}
/****************************************************************
 * instrument* InstFX_GetInst(char *reqInst)
 * Get the address of the instrument datastructure from the
 * table. NULL is returned if the instrument cannot be found.
 ****************************************************************/
instrument* InstFX_GetInst(char *reqInst)
{
	int instOff;

	if((instOff = InstFX_GetInstID(reqInst)) == -1) return NULL;
	else return &(instTable[instOff]);
}
/****************************************************************
 * int InstFX_GetInstID(char *reqInst)
 * Get the index of the instrument datastructure from the table.
 * -1 is returned if the instrument cannot be found.
 ****************************************************************/
int InstFX_GetInstID(char *reqInst)
{
	int instOff = 0;

	while(instOff < tableLen)
	{
		if(strcmp(reqInst, instTable[instOff].instName) == 0) return instOff;
		instOff++;
	}
	return -1;
}
/****************************************************************
 * int InstFX_ChkInstTable()
 * Check and Resize the instrument/sfx table if low on available
 * slots, and realloc the table if needed.
 * Will return 0 if the operation is sucessful, and -1 if
 * a problem was encountered.
 ****************************************************************/
int InstFX_ChkInstTable()
{
	int tempBufLen = tableBufLen + SFX_PARSER_ITSIZE;
	instrument *newTable;

	if(tableLen < tableBufLen) return 0;
	else
	{
		if((newTable = realloc(instTable,sizeof(instrument)*tempBufLen)) == NULL)
		{
			fprintf(stderr,"%s",ERR_NoAlloc);
			return -1;
		}
	}
	instTable = newTable;
	tableBufLen = tempBufLen;
	return 0;
}
/****************************************************************
 * void InstFX_FreeTable()
 ****************************************************************/
void InstFX_FreeTable()
{
	int relOff = 0;
	
	if(instTable != NULL)
	{
		while(relOff < tableLen)
		{

			free(instTable[relOff].instName);
			free(instTable[relOff].instStream);
			relOff++;
		}
		free(instTable);
	}
}

/****************************************************************
 * Building Functions
 ****************************************************************/
/****************************************************************
 * char* InstFX_BuildBin(int mode, int baseAddress)
 * Build and return a pointer to the binary instrument/sfx block,
 * will give a NULL value if any errors occur.
 * "mode" indicates sfx (nonzero) or instrument (0) operation.
 * "instfx_binSize" will also be set to the size of the bytestream.
 ****************************************************************/
char* InstFX_BuildBin(int mode, int baseAddress)
{
	char *instBin = NULL;
	int reqBinSz = 0;
	int cInstr,copyOff,tNoteOff;

	/*Maybe we got no instruments?*/
	if(tableLen == 0)
	{
		fprintf(stderr,"%s",ERR_NoInstr);
		return NULL;
	}
	/*No, no... we do. Get the binary ready.*/
	for(cInstr = 0; cInstr < tableLen; cInstr++)
	{
		if(mode) reqBinSz += instTable[cInstr].scriptLen + 3;
		else reqBinSz += instTable[cInstr].scriptLen + 2;
	}
	fprintf(stdout,"%d%s",cInstr,STA_SfxCnt);
	fprintf(stdout,"%s%d bytes\n",STA_SfxSiz,reqBinSz);
	if((instBin = malloc(sizeof(char)*reqBinSz)) == NULL){
		fprintf(stderr,"%s",ERR_NoAlloc);
		return NULL;
	}
	instfx_binSize = reqBinSz;
	if(mode) copyOff = tableLen*3;
	else copyOff = tableLen*2;

	/*Set general table offsets for use in music making*/
	instfx_lTableOff = 0;
	instfx_hTableOff = tableLen;
	instfx_pTableOff = tableLen*2;

	for(cInstr = 0; cInstr < tableLen; cInstr++)
	{
		instBin[cInstr] = (char)((copyOff+baseAddress) & 0xFF);
		instBin[tableLen + cInstr] = (char)(((copyOff+baseAddress) >> 8) & 0xFF);
		if(mode) instBin[tableLen*2 + cInstr] = (char)(instTable[cInstr].priority & 0xFF);
		memcpy(instBin + copyOff,instTable[cInstr].instStream,instTable[cInstr].scriptLen);

		tNoteOff = instTable[cInstr].noteOffOff + copyOff + baseAddress;
		instBin[copyOff] = (char)(tNoteOff & 0xFF);
		instBin[copyOff + 1] = (char)((tNoteOff >> 8) & 0xFF);
		copyOff += instTable[cInstr].scriptLen;
	}
	return instBin;
}

/****************************************************************
 * int InstFX_MakeEquates(int baseAddress, char *infileName)
 * Build and write the equates file for the instrument/sfx block.
 * Will return 0 if the operation was successful, -1 otherwise.
 ****************************************************************/
int InstFX_MakeEquates(int baseAddress, char *infileName)
{
	char *eqFileName = NULL;
	FILE *eqFile = NULL;
	int sfxOff;

	if(((eqFileName = altFileExt(infileName,"equ")) == NULL)
		|| ((eqFile = fopen(eqFileName,"w")) == NULL))
	{
		fprintf(stderr,"%s",ERR_SfxEqu);
		return -1;
	}
	fprintf(eqFile,"; Autogenerated equates for %s\n",infileName);
	fprintf(eqFile,"HandyMusic_NumSFX\tEQU %d\n",tableLen);
	fprintf(eqFile,"HandyMusic_SFX_ATableLo\tEQU %d\n",baseAddress);
	fprintf(eqFile,"HandyMusic_SFX_ATableHi\tEQU %d\n",(tableLen)+baseAddress);
	fprintf(eqFile,"HandyMusic_SFX_PTable\tEQU %d\n\n",(tableLen*2)+baseAddress);

	for(sfxOff = 0; sfxOff < tableLen; sfxOff++)
		fprintf(eqFile,"SFX_%s\tEQU %d\n",instTable[sfxOff].instName,sfxOff);

	fprintf(stdout,"Saving sfx equates to : %s\n",eqFileName);
	fclose(eqFile);
	free(eqFileName);
	return 0;
}

