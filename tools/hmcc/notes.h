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
 * notes.h
 * Last change 01-25-2012 by Osman Celimli
 ****************************************************************/

/****************************************************************
 * Function Prototypes
 ****************************************************************/
short Note_FreqtoDiv(double freq, double period);
double Note_StrtoFreq(char *noteString);
