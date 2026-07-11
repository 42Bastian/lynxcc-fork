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
 * common.c
 * Last change 03-14-2012 by Osman Celimli
 ****************************************************************/
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
/****************************************************************
 * Primary Functions
 ****************************************************************/
/****************************************************************
 * int parse16p8(struct u16p8 &dest, char *src)
 * Parse an input string and convert it to 16.8 format.
 * Source can be in hex ($xx.xx), decimal (xx.x), or binary (%xx.x)
 * If any error occurs during parsing, a -1 is returned.
 * Proper operation will yield a zero.
 ****************************************************************/
int parse16p8(struct u16p8 *dest, char *src)
{
	int upperWord = 0,lowerWord = 0;
	int scanRes;

	/* Bail if the source or dest are null... */
	if(src == NULL) return -1;
	if(dest == NULL) return -1;
	dest->loByte = 0;
	dest->hiByte = 0;
	dest->decByte = 0;

	/* Is our source in hex format? */
	if(src[0] == '$') scanRes = sscanf(src+1,"%x.%x",&upperWord,&lowerWord);
	/* What about binary? */
	else if(src[0] == '%')
	{
		for(scanRes = 0; scanRes < (signed int)strlen(src);scanRes++) if(src[scanRes] == '.') break;
		upperWord = parseBinString(src+1);
		lowerWord = parseBinString(src+scanRes+1);

		if(upperWord < 0) return -1;
		if(lowerWord < 0) lowerWord = 0;
	}
	/* Or decimal? */
	else{
		scanRes = sscanf(src,"%d.%d",&upperWord,&lowerWord);
		if(src[0] == '-'){
			if(!((scanRes == 1) || ((scanRes == 2) && (lowerWord == 0)))) upperWord -= 1;
		}
	}
	if(upperWord > 65535) upperWord = 65535;
	else if(upperWord < -32768) upperWord = -32768;
	if(lowerWord > 255) lowerWord = 255;

	if(scanRes == 0) return -1;

	dest->loByte = (char)(upperWord & 0xFF);
	dest->hiByte = (char)((upperWord & 0xFF00) >> 8);
	if(scanRes == 1) dest->decByte = (char)0;
	else
	{
		if(upperWord < 0) dest->decByte = (char)((256-lowerWord) & 0xFF);
		else dest->decByte = (char)(lowerWord & 0xFF);
	}
	return 0;
}

/****************************************************************
 * int parseBinString(char *src)
 * Convert a string of up to sixteen zeroes and ones to an unsigned
 * binary value. For example, "1111" would yield 15. Values larger
 * than 16-bits will be clamped to 65535. Any errors
 * encountered during parsing will yield a negative value.
 * Terminations can occur on a null terminator or '.'
 ****************************************************************/
int parseBinString(char *src)
{
	int tWord,i;

	if(src == NULL) return -1;

	tWord = 0;
	for(i=0; ;i++)
	{
		if(src[i] == 0 || src[i] == '.') break;
		else if(src[i] == '1') tWord = ((tWord << 1) | 1);
		else if(src[i] == '0') tWord = (tWord << 1);
		else
		{
			tWord = -1;
			break;
		}
	}
	if(i > 15) tWord = 65535;
	return tWord;
}

/****************************************************************
 * char* altFileExt(char *inFileName,char *newExt)
 * Creates a new filename string which contains the given
 * new extension in place of the original. If the original
 * file has no extension, the new extension is simply appended.
 * Will return NULL if anything went wrong.
 ****************************************************************/
char* altFileExt(char *inFileName,char *newExt)
{
	char *newFileName = NULL;
	int dotOff,strSz,extOff,exi;
	int extSz = strlen(newExt);
	for(dotOff = strlen(inFileName)-1; dotOff > -1; dotOff--)
	{
		if(inFileName[dotOff] == '.') break;
		else if(inFileName[dotOff] == '.' || inFileName[dotOff] == '.')
		{
			dotOff = -1;
			break;
		}
	}
	if(dotOff < 0)
	{
		strSz = strlen(inFileName)+strlen(newExt)+2;
		dotOff = strlen(inFileName)-1;
	}
	else strSz = dotOff+strlen(newExt)+2;
	extOff = dotOff+1;

	if((newFileName = malloc(sizeof(char)*strSz)) == NULL) return NULL;

	for(exi = 0; exi < dotOff; exi++) newFileName[exi] = inFileName[exi];
	newFileName[exi] = '.';
	for(exi = 0; exi < (extSz+1); exi++) newFileName[extOff+exi] = newExt[exi];

	return newFileName;
}
