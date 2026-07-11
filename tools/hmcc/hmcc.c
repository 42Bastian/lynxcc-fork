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
 * hmcc.c
 * Last change 11-29-2012 by Osman Celimli
 ****************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "instfx.h"
#include "music.h"
#include "common.h"
/****************************************************************
 * Text Tables
 ****************************************************************/
char ERR_Usage[]= "Usage:\nhmcc $dest_addr sfx_file.txt\nhmcc $dest_addr outfile.mus inst_file.txt trk_1.txt [trk_2.txt] [trk_3.txt] [trk_4.txt]\n";
char ERR_InvBase[]= "Invalid Base Address\n";

char ERR_NoOpen[]= "ERROR- Can't Open Source File: ";

char STA_SFXMode[]= "Sound Effects Mode...\n";
char STA_SFXSave[]= "Saving sfx binary to : ";

char ERR_NoSFXeq[]= "ERROR- Couldn't make sfx equates file.\n";
char ERR_BadSFXbin[]= "ERROR- Couldn't generate instrument/sfx binary block.\n";
char ERR_BadSFXfile[]= "ERROR- Couldn't write instrument/sfx output file.\n";

char STA_MUSMode[]= "Music Mode...\n";
char STA_MUSSave[]= "Saving music binary to : ";
char STA_MUSSize[]= "The current music binary size is : ";

char ERR_BadMUSbin[]= "ERROR- Couldn't generate music binary block.\n";
char ERR_BadMUSfile[]= "ERROR- Couldn't write music output file.\n";

/****************************************************************
 * int main(int argc, char **argv)
 * Possible argument formats:
 *  hmcc $dest_addr sfx_file.txt
 *  hmcc $dest_addr inst_file.txt trk_1.txt [trk_2.txt] ...
 ****************************************************************/
int main(int argc, char **argv)
{
	/*Vars*/
	int tracks = 0;
	int baseAddress = 0;
	int baseSFXAddress = 0;
	u16p8 rBaseAddress;

	FILE *instfx_source = NULL;

	char *instfx_bin = NULL;
	char *instfx_filename = NULL;
	FILE *instfx_binf = NULL;

	FILE *mus_source = NULL;

	char *mus_bin = NULL;
	FILE *mus_binf = NULL;
	int ix;

	/****
	 * First, some usage here and there...
	 ****/
	if((argc < 3) || (argc > 8))
	{
		fprintf(stderr,"%s",ERR_Usage);
		return 0;
	}
	if(argc > 3)
	{
		fprintf(stdout,"%s",STA_MUSMode);
		tracks = argc - 4;
	}
	else fprintf(stdout,"%s",STA_SFXMode);

	/*Invalid Base Addres*/
	if(parse16p8(&rBaseAddress,argv[1]) || rBaseAddress.decByte)
	{
		fprintf(stderr,"%s",ERR_InvBase);
		return 0;
	}
	baseAddress = ((rBaseAddress.loByte & 0xFF) | ((rBaseAddress.hiByte << 8) & 0xFF00));
	if(!tracks) baseSFXAddress = baseAddress;
	else baseSFXAddress = baseAddress + 16;

	/****
	 * Alright, Now let's go parse some SFX!
	 ****/
	ix = tracks ? 1 : 0;
	if((instfx_source = fopen(argv[2 + ix],"r")) == NULL)
	{
		fprintf(stderr,"%s %s\n",ERR_NoOpen,argv[2 + ix]);
		goto cleanup;
	}
	if(InstFX_ParseSrc(!tracks,instfx_source,argv[2 + ix])) goto cleanup;

	/****
	 * Then take care of the music sheets...
	 ****/
	if(tracks)
	{
		for(ix = 0; ix < tracks; ix++)
		{
			if((mus_source = fopen(argv[4 + ix],"r")) == NULL)
			{
				fprintf(stderr,"%s %s\n",ERR_NoOpen,argv[4 + ix]);
				goto cleanup;
			}
			if(mus_ParseSrc(mus_source,argv[4 + ix])) goto cleanup;
		}
	}

	/****
	 * Build the instrument/sfx binary table, and equates if in SFX mode.
	 ****/
	 if(!tracks && InstFX_MakeEquates(baseSFXAddress,argv[2]))
	 {
		 fprintf(stderr,"%s",ERR_NoSFXeq);
		 goto cleanup;
	 }
	 if((instfx_bin = InstFX_BuildBin(!tracks,baseSFXAddress)) == NULL)
	 {
		 fprintf(stderr,"%s",ERR_BadSFXbin);
		 goto cleanup;
	 }
	 else if(!tracks)
	 {
		if((instfx_filename = altFileExt(argv[2],"sfx")) == NULL)
		{
			fprintf(stderr,"%s",ERR_BadSFXbin);
			goto cleanup;
		}
		fprintf(stdout,"%s%s\n",STA_SFXSave,instfx_filename);
		if((instfx_binf = fopen(instfx_filename,"wb")) == NULL)
		{
			fprintf(stderr,"%s",ERR_BadSFXfile);
			goto cleanup;
		}
		fwrite(instfx_bin,1,instfx_binSize,instfx_binf);
		fclose(instfx_binf);
		/*All done with the SFX bin...*/
		goto cleanup;
	 }

	/****
	 * Then finally the music binary building.
	 ****/
	if(tracks)
	{
		/* Build the Binary */
		if((mus_bin = mus_BuildBin(baseAddress)) == NULL)
		{
			fprintf(stderr,"%s",ERR_BadMUSbin);
			goto cleanup;
		}
		/* Copy over the SFX Binary */
		memcpy(mus_bin + MUS_HDR_LENGTH,instfx_bin,instfx_binSize);
		/* Write it */
		fprintf(stdout,"%s%d bytes\n",STA_MUSSize,mus_binSize);
		fprintf(stdout,"%s%s\n",STA_MUSSave,argv[2]);
		if((mus_binf = fopen(argv[2],"wb")) == NULL)
		{
			fprintf(stderr,"%s",ERR_BadMUSfile);
			goto cleanup;
		}
		fwrite(mus_bin,1,mus_binSize,mus_binf);
		fclose(mus_binf);
	}

	/****
	 * Done!
	 ****/
cleanup:
	fflush(stdout);
	fflush(stderr);
	if(instfx_filename != NULL) free(instfx_filename);
	if(instfx_bin != NULL) free(instfx_bin);
	if(mus_bin != NULL) free(mus_bin);

	InstFX_FreeTable();
	mus_FreeTable();
	return 0;
}
