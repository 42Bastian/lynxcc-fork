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
 * notes.c
 * Last change 09-25-2012 by Osman Celimli
 ****************************************************************/
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "notes.h"
/****************************************************************
 * Tables
 ****************************************************************/
char Err_NoteParse[] = "Error- note parse failure.\n";

/****************************************************************
 * Note Tables
 ****************************************************************/
/*Note Textual Equivalence*/
char Note_BaseText[][3]={"cb",			"bs","c.","cs","db",
						"d.","ds","eb",	"e.","fb","es","f.",
						"fs","gb","g.",	"gs","ab","a.",
						"as","bb","b."};
/*Base Offset from Octave*/
int Note_BaseOffset[]=	{11,			0,0,1,1,
						2,3,3,			4,4,5,5,
						6,6,7,			8,8,9,
						10,10,11};
/*Inter-Octave Adjustment*/
int Note_OctOffset[]=	{-1,			1,0,0,0,
						0,0,0,			0,0,0,0,
						0,0,0,			0,0,0,
						0,0,0,0};
#define Note_RTableSize (sizeof(Note_BaseText)/sizeof(Note_BaseText[0]))/sizeof(char)

/****************************************************************
 * int Note_FreqtoDiv(double freq, double period)
 * Calculates the final 10-Bit divider value used in the Lynx
 * from the given base frequency and waveform period.
 ****************************************************************/
short Note_FreqtoDiv(double freq, double period)
{
	double Fp = freq*period;
	short div;

	if(Fp > 1000000.0) div = 0; 
	else if(Fp > 3906.25) div = (short)(1000000/Fp)-1;  
	else if(Fp > 1953.13) div = (short)(500000/Fp)+128; 
	else if(Fp > 976.563) div = (short)(250000/Fp)+256; 
	else if(Fp > 488.281) div = (short)(125000/Fp)+384; 
	else if(Fp > 244.141) div = (short)(62500/Fp)+512; 
	else if(Fp > 122.07) div = (short)(31250/Fp)+640; 
	else if(Fp > 61.0352) div = (short)(15625/Fp)+768; 
	else div = 1023;
	return div;
}

/****************************************************************
 * double Note_StrtoFreq(char *noteString)
 * Converts a note string to an integer frequency.
 * Uses a three characer format arranged as : nXo
 * n = Note (a,g,f,etc)
 * X = Flat (b) or Sharp (s)
 * o = Octave (0-9)
 * 
 * Using A4,440Hz
 ****************************************************************/
double Note_StrtoFreq(char *noteString)
{
	char noteBuf[3]={0};
	double note_freq;
	int diff_note;
	int base_octave=-1,base_note=-1;
	int err=-1;
	int nidx;

	/*Parse the Note*/
	noteBuf[0]=noteString[0];
	noteBuf[1]=noteString[1];
	if(sscanf(noteString+2,"%d",&base_octave) == 1)
	{
		for(nidx=(Note_RTableSize-1);nidx > -1;nidx--)
		{
			if(!strcmp(Note_BaseText[nidx],noteBuf))
			{
				base_note=Note_BaseOffset[nidx];
				base_octave+=Note_OctOffset[nidx];
				err=0;
				break;
			}
		}
	}

	/*Any problems?*/
	if(err)
	{
		fprintf(stderr,"%s",Err_NoteParse);
		return -1.0;
	}

	diff_note = (12*base_octave+base_note)-(12*4+9);

	/* Frequency is calculated relative to A4,440 using:
	 * f=2^(((12*diff_octave)+diff_note)/12)*400
	 */
	note_freq = pow(2.0,(((double)diff_note)/12.0))*440.0;
	if(note_freq < 0.0) note_freq = 0.0;
	return note_freq;
}

