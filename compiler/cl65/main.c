/*****************************************************************************/
/*                                                                           */
/*                                  main.c                                   */
/*                                                                           */
/*             Main module for the cl65 compile-and-link utility             */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1999-2013, Ullrich von Bassewitz                                      */
/*                Roemerstrasse 52                                           */
/*                D-70794 Filderstadt                                        */
/* EMail:         uz@cc65.org                                                */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/



/* Check out if we have a spawn() function on the system, or if we must use
** our own.
*/
#if defined(_WIN32)
#  define HAVE_SPAWN 1
#else
#  define NEED_SPAWN 1
#endif

/* GCC strictly follows http://c-faq.com/ansi/constmismatch.html and issues an
** 'incompatible pointer type' warning - that can't be suppressed via #pragma.
** The spawnvp() prototype of MinGW (http://www.mingw.org/) differs from the
** one of MinGW-w64 (http://mingw-w64.sourceforge.net/) regarding constness.
** So there's no alternative to actually distinguish these environments :-(
*/
#define SPAWN_ARGV_CONST_CAST
#if defined(__MINGW32__)
#  include <_mingw.h>
#  if !defined(__MINGW64_VERSION_MAJOR)
#    undef  SPAWN_ARGV_CONST_CAST
#    define SPAWN_ARGV_CONST_CAST (const char* const *)
#  endif
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#if defined(HAVE_SPAWN)
#  include <process.h>
#endif

/* common */
#include "attrib.h"
#include "cmdline.h"
#include "filetype.h"
#include "fname.h"
#include "mmodel.h"
#include "searchpath.h"
#include "strbuf.h"
#include "strutil.h"
#include "target.h"
#include "version.h"
#include "xmalloc.h"

/* cl65 */
#include "global.h"
#include "error.h"



/*****************************************************************************/
/*                                   Data                                    */
/*****************************************************************************/



/* Struct that describes a command */
typedef struct CmdDesc CmdDesc;
struct CmdDesc {
    char*       Name;           /* The command name */

    unsigned    ArgCount;       /* Count of arguments */
    unsigned    ArgMax;         /* Maximum count of arguments */
    char**      Args;           /* The arguments */

    unsigned    FileCount;      /* Count of files to translate */
    unsigned    FileMax;        /* Maximum count of files */
    char**      Files;          /* The files */
};

/* Command descriptors for the different programs */
static CmdDesc CC65 = { 0, 0, 0, 0, 0, 0, 0 };
static CmdDesc CA65 = { 0, 0, 0, 0, 0, 0, 0 };
static CmdDesc LD65 = { 0, 0, 0, 0, 0, 0, 0 };

/* Variables controlling the steps we're doing */
static int DoLink       = 1;
static int DoAssemble   = 1;

/* The name of the output file, NULL if none given */
static const char* OutputName = 0;

/* The name of the linker configuration file if given */
static const char* LinkerConfig = 0;

/* The name of the first input file. This will be used to construct the
** executable file name if no explicit name is given.
*/
static const char* FirstInput = 0;

/* The names of the files for dependency generation */
static const char* DepName = 0;
static const char* FullDepName = 0;

/* Name of the target specific runtime library */
static char* TargetLib   = 0;
static int   NoTargetLib = 0;

/* SDK auto-library resolution (design/LYNX_CL65_AUTOLIBS_DESIGN.md).
** By default cl65 appends the whole SDK library set named by the manifest
** lib/lynx-sdklibs.list (dependents first, core last) instead of only the core
** target library, so a program links whatever subsystems it references with no
** -l flags. NoSDKLibs restores the core-only behaviour; SDKLibsName overrides
** the manifest file.
*/
static int         NoSDKLibs   = 0;
static const char* SDKLibsName = 0;

/* The fixed name of the SDK library manifest, searched for in the lib/ path. */
#define SDKLIBS_MANIFEST "lynx-sdklibs.list"



/*****************************************************************************/
/*                Include the system specific spawn function                 */
/*****************************************************************************/



#if defined(NEED_SPAWN)
#  if defined(_AMIGA)
#    include "spawn-amiga.inc"
#  else
#    include "spawn-unix.inc"
#  endif
#endif



/*****************************************************************************/
/*                        Credential functions                               */
/*****************************************************************************/



static void DisableAssembling (void)
{
    DoAssemble = 0;
}



static void DisableLinking (void)
{
    DoLink = 0;
}



static void DisableAssemblingAndLinking (void)
{
    DisableAssembling ();
    DisableLinking ();
}



/*****************************************************************************/
/*                        Command structure handling                         */
/*****************************************************************************/



static char* CmdAllocArg (const char* Arg, unsigned Len)
/* Alloc (potentially quoted) argument */
{
    char* Alloc;

/* The Microsoft docs say on spawnvp():
** Spaces embedded in strings may cause unexpected behavior; for example,
** passing _spawn the string "hi there" will result in the new process getting
** two arguments, "hi" and "there". If the intent was to have the new process
** open a file named "hi there", the process would fail. You can avoid this by
** quoting the string: "\"hi there\"".
*/
#if defined(_WIN32)
    /* Quote argument if it contains space(s) */
    if (memchr (Arg, ' ', Len)) {
        Alloc = xmalloc (Len + 3);
        Alloc[0] = '"';
        memcpy (Alloc + 1, Arg, Len);
        Alloc[Len + 1] = '"';
        Alloc[Len + 2] = '\0';
    } else
#endif
    {
        Alloc = xmalloc (Len + 1);
        memcpy (Alloc, Arg, Len);
        Alloc[Len] = '\0';
    }
    return Alloc;
}



static void CmdExpand (CmdDesc* Cmd)
/* Expand the argument vector */
{
    unsigned NewMax  = Cmd->ArgMax + 10;
    char**       NewArgs = xmalloc (NewMax * sizeof (char*));
    memcpy (NewArgs, Cmd->Args, Cmd->ArgMax * sizeof (char*));
    xfree (Cmd->Args);
    Cmd->Args   = NewArgs;
    Cmd->ArgMax = NewMax;
}



static void CmdAddArg (CmdDesc* Cmd, const char* Arg)
/* Add a new argument to the command */
{
    /* Expand the argument vector if needed */
    if (Cmd->ArgCount >= Cmd->ArgMax) {
        CmdExpand (Cmd);
    }

    /* Add a copy of the new argument, allow a NULL pointer */
    if (Arg) {
        Cmd->Args[Cmd->ArgCount++] = CmdAllocArg (Arg, strlen (Arg));
    } else {
        Cmd->Args[Cmd->ArgCount++] = 0;
    }
}



static void CmdAddArg2 (CmdDesc* Cmd, const char* Arg1, const char* Arg2)
/* Add a new argument pair to the command */
{
    CmdAddArg (Cmd, Arg1);
    CmdAddArg (Cmd, Arg2);
}



static void CmdAddArgList (CmdDesc* Cmd, const char* ArgList)
/* Add a list of arguments separated by commas */
{
    const char* Arg = ArgList;
    const char* P   = Arg;

    while (1) {
        if (*P == '\0' || *P == ',') {

            /* End of argument, add it */
            unsigned Len = P - Arg;

            /* Expand the argument vector if needed */
            if (Cmd->ArgCount >= Cmd->ArgMax) {
                CmdExpand (Cmd);
            }

            /* Add the new argument */
            Cmd->Args[Cmd->ArgCount++] = CmdAllocArg (Arg, Len);

            /* If the argument was terminated by a comma, skip it, otherwise
            ** we're done.
            */
            if (*P == ',') {
                /* Start over at next char */
                Arg = ++P;
            } else {
                break;
            }
        } else {
            /* Skip other chars */
            ++P;
        }
    }

}



static void CmdDelArgs (CmdDesc* Cmd, unsigned LastValid)
/* Remove all arguments with an index greater than LastValid */
{
    while (Cmd->ArgCount > LastValid) {
        Cmd->ArgCount--;
        xfree (Cmd->Args [Cmd->ArgCount]);
        Cmd->Args [Cmd->ArgCount] = 0;
    }
}



static void CmdAddFile (CmdDesc* Cmd, const char* File)
/* Add a new file to the command */
{
    /* Expand the file vector if needed */
    if (Cmd->FileCount == Cmd->FileMax) {
        unsigned NewMax   = Cmd->FileMax + 10;
        char**   NewFiles = xmalloc (NewMax * sizeof (char*));
        memcpy (NewFiles, Cmd->Files, Cmd->FileMax * sizeof (char*));
        xfree (Cmd->Files);
        Cmd->Files   = NewFiles;
        Cmd->FileMax = NewMax;
    }

    /* If the file name is not NULL (which is legal and is used to terminate
    ** the file list), check if the file name does already exist in the file
    ** list and print a warning if so. Regardless of the search result, add
    ** the file.
    */
    if (File) {
        unsigned I;
        for (I = 0; I < Cmd->FileCount; ++I) {
            if (strcmp (Cmd->Files[I], File) == 0) {
                /* Duplicate file */
                Warning ("Duplicate file in argument list: '%s'", File);
                /* No need to search further */
                break;
            }
        }

        /* Add the file */
        Cmd->Files [Cmd->FileCount++] = xstrdup (File);
    } else {
        /* Add a NULL pointer */
        Cmd->Files [Cmd->FileCount++] = 0;
    }
}



static void CmdInit (CmdDesc* Cmd, const char* Path, const char* Name)
/* Initialize the command using the given path and name of the executable */
{
    char* FullName;

    FullName = (char*) xmalloc (strlen (Path) + strlen (Name) + 1);
    strcpy (FullName, Path);
    strcat (FullName, Name);

    /* Remember the command */
    Cmd->Name = xstrdup (FullName);

    /* Use the command name as first argument */
    CmdAddArg (Cmd, FullName);

    xfree (FullName);
}



static void CmdSetOutput (CmdDesc* Cmd, const char* File)
/* Set the output file in a command desc */
{
    CmdAddArg2 (Cmd, "-o", File);
}



static void CmdPrint (CmdDesc* Cmd, FILE* F)
/* Output the command line encoded in the command desc */
{
    unsigned I;
    for (I = 0; I < Cmd->ArgCount && Cmd->Args[I] != 0; ++I) {
        fprintf (F, "%s ", Cmd->Args[I]);
    }
}



/*****************************************************************************/
/*                              Target handling                              */
/*****************************************************************************/



static void SetTargetFiles (void)
/* Set the target system files */
{
    /* Get a pointer to the system name and its length */
    const char* TargetName = GetTargetName (Target);
    unsigned    TargetNameLen = strlen (TargetName);

    /* Set the library file */
    TargetLib = xmalloc (TargetNameLen + 4 + 1);
    memcpy (TargetLib, TargetName, TargetNameLen);
    strcpy (TargetLib + TargetNameLen, ".lib");
}



static char* FindSDKManifest (void)
/* Locate the SDK library manifest. Uses the explicit --sdk-libs name if given,
** otherwise searches the same lib/ path ld65 uses (LD65_LIB, $CC65_HOME/lib, the
** compiled-in default, and the WinBin ../lib fallback). Returns a malloced path
** or NULL if no manifest was found.
*/
{
    char* Path;
    SearchPaths* LibPaths;

    /* An explicit manifest is used verbatim */
    if (SDKLibsName) {
        return xstrdup (SDKLibsName);
    }

    /* Build the lib search path the same way ld65 builds LibDefaultPath */
    LibPaths = NewSearchPath ();
    AddSearchPath (LibPaths, "");
    AddSearchPathFromEnv (LibPaths, "LD65_LIB");
    AddSubSearchPathFromEnv (LibPaths, "CC65_HOME", "lib");
#if defined(LD65_LIB) && !defined(_WIN32)
    AddSearchPath (LibPaths, STRINGIZE (LD65_LIB));
#endif
    AddSubSearchPathFromWinBin (LibPaths, "lib");

    Path = SearchFile (LibPaths, SDKLIBS_MANIFEST);
    return Path;
}



static int LibAlreadyListed (const char* Lib)
/* Return true if an archive with the same base name as Lib is already on the
** linker file list (a user-supplied -l/.lib argument), so the manifest entry
** can be skipped and the user's pin/override wins.
*/
{
    unsigned I;
    const char* Base = FindName (Lib);
    for (I = 0; I < LD65.FileCount; ++I) {
        if (LD65.Files[I] != 0 && strcmp (FindName (LD65.Files[I]), Base) == 0) {
            return 1;
        }
    }
    return 0;
}



static int AppendSDKLibs (void)
/* Append the SDK library set named by the manifest to the linker command line,
** in listed order (dependents first, core last), skipping any archive the user
** already pinned. Return true if the manifest was found and used, false if no
** manifest exists (so the caller falls back to the core target library).
*/
{
    char* Path;
    FILE* F;
    char  Line[256];

    Path = FindSDKManifest ();
    if (Path == 0) {
        /* No manifest: degrade to today's core-only behaviour */
        return 0;
    }

    F = fopen (Path, "r");
    if (F == 0) {
        /* An explicit --sdk-libs that cannot be opened is a hard error;
        ** a missing auto-discovered manifest just falls back.
        */
        if (SDKLibsName) {
            Error ("Cannot open SDK library manifest '%s': %s",
                   Path, strerror (errno));
        }
        xfree (Path);
        return 0;
    }

    while (fgets (Line, sizeof (Line), F) != 0) {
        char* B = Line;
        char* E;

        /* Trim leading and trailing whitespace */
        while (*B == ' ' || *B == '\t') {
            ++B;
        }
        E = B + strlen (B);
        while (E > B && (E[-1] == '\n' || E[-1] == '\r' ||
                         E[-1] == ' '  || E[-1] == '\t')) {
            *--E = '\0';
        }

        /* Skip blank lines and comments */
        if (*B == '\0' || *B == '#') {
            continue;
        }

        /* Append unless the user already named this archive */
        if (!LibAlreadyListed (B)) {
            CmdAddArg (&LD65, B);
        }
    }

    fclose (F);
    xfree (Path);
    return 1;
}



/*****************************************************************************/
/*                               Subprocesses                                */
/*****************************************************************************/



static void ExecProgram (CmdDesc* Cmd)
/* Execute a subprocess with the given name/parameters. Exit on errors. */
{
    int Status;

    /* If in debug mode, output the command line we will execute */
    if (Debug) {
        printf ("Executing: ");
        CmdPrint (Cmd, stdout);
        printf ("\n");
    }

    /* Call the program */
    Status = spawnvp (P_WAIT, Cmd->Name, SPAWN_ARGV_CONST_CAST Cmd->Args);

    /* Check the result code */
    if (Status < 0) {
        /* Error executing the program */
        Error ("Cannot execute '%s': %s", Cmd->Name, strerror (errno));
    } else if (Status != 0) {
        /* Called program had an error */
        exit (Status);
    }
}



static void Link (void)
/* Link the resulting executable */
{
    unsigned I;

    /* Since linking is always the final step, if we have an output file name
    ** given, set it here. If we don't have an explicit output name given,
    ** try to build one from the name of the first input file.
    */
    if (OutputName) {

        CmdSetOutput (&LD65, OutputName);

    } else if (FirstInput && FindExt (FirstInput)) {  /* Only if ext present! */

        char* Output = MakeFilename (FirstInput, "");
        CmdSetOutput (&LD65, Output);
        xfree (Output);

    }

    /* If we have a linker config file given, add it to the command line.
    ** Otherwise the linker uses the (fixed) Lynx configuration by default.
    */
    if (LinkerConfig) {
        CmdAddArg2 (&LD65, "-C", LinkerConfig);
    }

    /* Add all object files as parameters */
    for (I = 0; I < LD65.FileCount; ++I) {
        CmdAddArg (&LD65, LD65.Files [I]);
    }

    /* Add the SDK libraries unless disabled. By default the whole SDK set
    ** (from the manifest, dependents first, core last) is appended so the
    ** subsystems a program references link with no -l flags; --no-sdk-libs
    ** (and the manifest-absent fallback) append only the core target library.
    */
    if (!NoTargetLib)
    {
        /* Determine which target (core) library is needed */
        SetTargetFiles ();

        if (NoSDKLibs || !AppendSDKLibs ()) {
            if (TargetLib) {
                CmdAddArg (&LD65, TargetLib);
            }
        }
    }

    /* Terminate the argument list with a NULL pointer */
    CmdAddArg (&LD65, 0);

    /* Call the linker */
    ExecProgram (&LD65);
}



static void AssembleFile (const char* File, unsigned ArgCount)
/* Common routine to assemble a file. Will be called by Assemble() and
** AssembleIntermediate(). Adds options common for both routines and
** assembles the file. Will remove excess arguments after assembly.
*/
{
    /* Check if this is the last processing step */
    if (DoLink) {
        /* We're linking later. Add the output file of the assembly
        ** the the file list of the linker. The name of the output
        ** file is that of the input file with ".s" replaced by ".o".
        */
        char* ObjName = MakeFilename (File, ".o");
        CmdAddFile (&LD65, ObjName);
        xfree (ObjName);
    } else {
        /* This is the final step. If an output name is given, set it */
        if (OutputName) {
            CmdSetOutput (&CA65, OutputName);
        }
    }

    /* Add the file as argument for the assembler */
    CmdAddArg (&CA65, File);

    /* Add a NULL pointer to terminate the argument list */
    CmdAddArg (&CA65, 0);

    /* Run the assembler */
    ExecProgram (&CA65);

    /* Remove the excess arguments */
    CmdDelArgs (&CA65, ArgCount);
}



static void AssembleIntermediate (const char* SourceFile)
/* Assemble an intermediate file which was generated by a previous processing
** step with SourceFile as input. The -dep options won't be added and
** the intermediate assembler file is removed after assembly.
*/
{
    /* Generate the name of the assembler output file from the source file
    ** name. It's the same name with the extension replaced by ".s"
    */
    char* AsmName = MakeFilename (SourceFile, ".s");

    /* Assemble the intermediate assembler file */
    AssembleFile (AsmName, CA65.ArgCount);

    /* Remove the input file */
    if (remove (AsmName) < 0) {
        Warning ("Cannot remove temporary file '%s': %s",
                 AsmName, strerror (errno));
    }

    /* Free the assembler file name which was allocated from the heap */
    xfree (AsmName);
}



static void Assemble (const char* File)
/* Assemble the given file */
{
    /* Remember the current assembler argument count */
    unsigned ArgCount = CA65.ArgCount;

    /* We aren't assembling an intermediate file, but one requested by the
    ** user. So add a few options here if they were given on the command
    ** line.
    */
    if (DepName && *DepName) {
        CmdAddArg2 (&CA65, "--create-dep", DepName);
    }
    if (FullDepName && *FullDepName) {
        CmdAddArg2 (&CA65, "--create-full-dep", FullDepName);
    }

    /* Use the common routine */
    AssembleFile (File, ArgCount);
}



static void Compile (const char* File)
/* Compile the given file */
{
    /* Remember the current compiler argument count */
    unsigned ArgCount = CC65.ArgCount;

    /* Check if this is the final step */
    if (DoAssemble) {
        /* We will assemble this file later. If a dependency file is to be
        ** generated, set the dependency target to be the final object file,
        ** not the intermediate assembler file. But beware: There may be an
        ** output name specified for the assembler.
        */
        if (DepName || FullDepName) {
            /* Was an output name for the assembler specified? */
            if (!DoLink && OutputName) {
                /* Use this name as the dependency target */
                CmdAddArg2 (&CC65, "--dep-target", OutputName);
            } else {
                /* Use the object file name as the dependency target */
                char* ObjName = MakeFilename (File, ".o");
                CmdAddArg2 (&CC65, "--dep-target", ObjName);
                xfree (ObjName);
            }
        }
    } else {
        /* If we won't assemble, this is the final step. In this case, set
        ** the output name if it was given.
        */
        if (OutputName) {
            CmdSetOutput (&CC65, OutputName);
        }
    }

    /* Add the file as argument for the compiler */
    CmdAddArg (&CC65, File);

    /* Add a NULL pointer to terminate the argument list */
    CmdAddArg (&CC65, 0);

    /* Run the compiler */
    ExecProgram (&CC65);

    /* Remove the excess arguments */
    CmdDelArgs (&CC65, ArgCount);

    /* If this is not the final step, assemble the generated file, then
    ** remove it
    */
    if (DoAssemble) {
        /* Assemble the intermediate file and remove it */
        AssembleIntermediate (File);
    }
}



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



static void Usage (void)
/* Print usage information and exit */
{
    printf ("Usage: %s [options] file [...]\n"
            "Short options:\n"
            "  -c\t\t\t\tCompile and assemble, but don't link\n"
            "  -d\t\t\t\tDebug mode\n"
            "  -g\t\t\t\tAdd debug info\n"
            "  -h\t\t\t\tHelp (this text)\n"
            "  -l name\t\t\tCreate an assembler listing file\n"
            "  -m name\t\t\tCreate a map file\n"
            "  -o name\t\t\tName the output file\n"
            "  -r\t\t\t\tEnable register variables\n"
            "  -t sys\t\t\tSet the target system\n"
            "  -u sym\t\t\tForce an import of symbol 'sym'\n"
            "  -v\t\t\t\tVerbose mode\n"
            "  -vm\t\t\t\tVerbose map file\n"
            "  -C name\t\t\tUse linker config file\n"
            "  -Cl\t\t\t\tMake local variables static\n"
            "  -D sym[=defn]\t\t\tDefine a preprocessor symbol\n"
            "  -E\t\t\t\tStop after the preprocessing stage\n"
            "  -I dir\t\t\tSet a compiler include directory path\n"
            "  -L path\t\t\tSpecify a library search path\n"
            "  -Ln name\t\t\tCreate a VICE label file\n"
            "  -O\t\t\t\tOptimize code\n"
            "  -Oi\t\t\t\tOptimize code, inline runtime functions\n"
            "  -Or\t\t\t\tOptimize code, honour the register keyword\n"
            "  -Os\t\t\t\tOptimize code, inline known C functions\n"
            "  -S\t\t\t\tCompile, but don't assemble and link\n"
            "  -T\t\t\t\tInclude source as comment\n"
            "  -V\t\t\t\tPrint the version number\n"
            "  -W name[,...]\t\t\tSuppress compiler warnings\n"
            "  -Wa options\t\t\tPass options to the assembler\n"
            "  -Wc options\t\t\tPass options to the compiler\n"
            "  -Wl options\t\t\tPass options to the linker\n"
            "\n"
            "Long options:\n"
            "  --add-source\t\t\tInclude source as comment\n"
            "  --all-cdecl\t\t\tMake functions default to __cdecl__\n"
            "  --asm-args options\t\tPass options to the assembler\n"
            "  --asm-define sym[=v]\t\tDefine an assembler symbol\n"
            "  --asm-include-dir dir\t\tSet an assembler include directory\n"
            "  --bin-include-dir dir\t\tSet an assembler binary include directory\n"
            "  --bss-name seg\t\tSet the name of the BSS segment\n"
            "  --cc-args options\t\tPass options to the compiler\n"
            "  --cfg-path path\t\tSpecify a config file search path\n"
            "  --check-stack\t\t\tGenerate stack overflow checks\n"
            "  --code-name seg\t\tSet the name of the CODE segment\n"
            "  --codesize x\t\t\tAccept larger code by factor x\n"
            "  --config name\t\t\tUse linker config file\n"
            "  --cpu type\t\t\tSet cpu type (65C02, 65SC02)\n"
            "  --create-dep name\t\tCreate a make dependency file\n"
            "  --create-full-dep name\tCreate a full make dependency file\n"
            "  --data-name seg\t\tSet the name of the DATA segment\n"
            "  --debug\t\t\tDebug mode\n"
            "  --debug-info\t\t\tAdd debug info\n"
            "  --feature name\t\tSet an emulation feature\n"
            "  --force-import sym\t\tForce an import of symbol 'sym'\n"
            "  --help\t\t\tHelp (this text)\n"
            "  --include-dir dir\t\tSet a compiler include directory path\n"
            "  --ld-args options\t\tPass options to the linker\n"
            "  --lib-path path\t\tSpecify a library search path\n"
            "  --listing name\t\tCreate an assembler listing file\n"
            "  --list-bytes n\t\tNumber of bytes per assembler listing line\n"
            "  --mapfile name\t\tCreate a map file\n"
            "  --no-sdk-libs\t\t\tLink only the core SDK library, not the optional set\n"
            "  --no-target-lib\t\tDon't link the target library\n"
            "  --obj file\t\t\tLink this object file\n"
            "  --obj-path path\t\tSpecify an object file search path\n"
            "  --print-target-path\t\tPrint the target file path\n"
            "  --register-space b\t\tSet space available for register variables\n"
            "  --register-vars\t\tEnable register variables\n"
            "  --rodata-name seg\t\tSet the name of the RODATA segment\n"
            "  --sdk-libs file\t\tUse an alternative SDK library manifest\n"
            "  --signed-chars\t\tDefault characters are signed\n"
            "  --start-addr addr\t\tSet the default start address\n"
            "  --static-locals\t\tMake local variables static\n"
            "  --target sys\t\t\tSet the target system\n"
            "  --version\t\t\tPrint the version number\n"
            "  --verbose\t\t\tVerbose mode\n",
            ProgName);
}



static void OptAddSource (const char* Opt attribute ((unused)),
                          const char* Arg attribute ((unused)))
/* Strict source code as comments to the generated asm code */
{
    CmdAddArg (&CC65, "-T");
}


static void OptAllCDecl  (const char* Opt attribute ((unused)),
                          const char* Arg attribute ((unused)))
/* Make functions default to __cdecl__ */
{
    CmdAddArg (&CC65, "--all-cdecl");
}



static void OptAsmArgs (const char* Opt attribute ((unused)), const char* Arg)
/* Pass arguments to the assembler */
{
    CmdAddArgList (&CA65, Arg);
}



static void OptAsmDefine (const char* Opt attribute ((unused)), const char* Arg)
/* Define an assembler symbol (assembler) */
{
    CmdAddArg2 (&CA65, "-D", Arg);
}



static void OptAsmIncludeDir (const char* Opt attribute ((unused)), const char* Arg)
/* Include directory (assembler) */
{
    CmdAddArg2 (&CA65, "-I", Arg);
}



static void OptBinIncludeDir (const char* Opt attribute ((unused)), const char* Arg)
/* Binary include directory (assembler) */
{
    CmdAddArg2 (&CA65, "--bin-include-dir", Arg);
}



static void OptBssName (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --bss-name option */
{
    CmdAddArg2 (&CC65, "--bss-name", Arg);
}



static void OptCCArgs (const char* Opt attribute ((unused)), const char* Arg)
/* Pass arguments to the compiler */
{
    CmdAddArgList (&CC65, Arg);
}



static void OptCfgPath (const char* Opt attribute ((unused)), const char* Arg)
/* Config file search path (linker) */
{
    CmdAddArg2 (&LD65, "--cfg-path", Arg);
}



static void OptCheckStack (const char* Opt attribute ((unused)),
                           const char* Arg attribute ((unused)))
/* Handle the --check-stack option */
{
    CmdAddArg (&CC65, "--check-stack");
}



static void OptCodeName (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --code-name option */
{
    CmdAddArg2 (&CC65, "--code-name", Arg);
}



static void OptCodeSize (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --codesize option */
{
    CmdAddArg2 (&CC65, "--codesize", Arg);
}



static void OptConfig (const char* Opt attribute ((unused)), const char* Arg)
/* Config file (linker) */
{
    if (LinkerConfig) {
        Error ("Cannot specify -C/--config twice");
    }
    LinkerConfig = Arg;
}



static void OptCreateDep (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --create-dep option */
{
    /* Add the file name to the compiler */
    CmdAddArg2 (&CC65, "--create-dep", Arg);

    /* Remember the file name for the assembler */
    DepName = Arg;
}



static void OptCreateFullDep (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --create-full-dep option */
{
    /* Add the file name to the compiler */
    CmdAddArg2 (&CC65, "--create-full-dep", Arg);

    /* Remember the file name for the assembler */
    FullDepName = Arg;
}



static void OptDataName (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --data-name option */
{
    CmdAddArg2 (&CC65, "--data-name", Arg);
}



static void OptDebug (const char* Opt attribute ((unused)),
                      const char* Arg attribute ((unused)))
/* Debug mode (compiler and cl65 utility) */
{
    CmdAddArg (&CC65, "-d");
    Debug = 1;
}



static void OptDebugInfo (const char* Opt attribute ((unused)),
                          const char* Arg attribute ((unused)))
/* Debug Info - add to compiler and assembler */
{
    CmdAddArg (&CC65, "-g");
    CmdAddArg (&CA65, "-g");
}



static void OptFeature (const char* Opt attribute ((unused)), const char* Arg)
/* Emulation features for the assembler */
{
    CmdAddArg2 (&CA65, "--feature", Arg);
}



static void OptForceImport (const char* Opt attribute ((unused)), const char* Arg)
/* Emulation features for the assembler */
{
    CmdAddArg2 (&LD65, "-u", Arg);
}



static void OptHelp (const char* Opt attribute ((unused)),
                     const char* Arg attribute ((unused)))
/* Print help - cl65 */
{
    Usage ();
    exit (EXIT_SUCCESS);
}



static void OptIncludeDir (const char* Opt attribute ((unused)), const char* Arg)
/* Include directory (compiler) */
{
    CmdAddArg2 (&CC65, "-I", Arg);
}



static void OptLdArgs (const char* Opt attribute ((unused)), const char* Arg)
/* Pass arguments to the linker */
{
    CmdAddArgList (&LD65, Arg);
}



static void OptLibPath (const char* Opt attribute ((unused)), const char* Arg)
/* Library search path (linker) */
{
    CmdAddArg2 (&LD65, "--lib-path", Arg);
}



static void OptListBytes (const char* Opt attribute ((unused)), const char* Arg)
/* Set the maximum number of bytes per asm listing line */
{
    CmdAddArg2 (&CA65, "--list-bytes", Arg);
}



static void OptListing (const char* Opt attribute ((unused)), const char* Arg)
/* Create an assembler listing */
{
    CmdAddArg2 (&CA65, "-l", Arg);
}



static void OptMapFile (const char* Opt attribute ((unused)), const char* Arg)
/* Create a map file */
{
    /* Create a map file (linker) */
    CmdAddArg2 (&LD65, "-m", Arg);
}



static void OptCPU (const char* Opt, const char* Arg)
/* Handle the --cpu option */
{
    /* Accepted for backwards compatibility only. The toolchain always uses the
    ** 65SC02, so the argument is validated but otherwise ignored. Only the CPUs
    ** that ever shipped in a Lynx (the 65C02 family) are allowed.
    */
    if (StrCaseCmp (Arg, "65C02") != 0 && StrCaseCmp (Arg, "65SC02") != 0) {
        Error ("Invalid argument for %s: '%s'", Opt, Arg);
    }
}



static void OptTarget (const char* Opt attribute ((unused)), const char* Arg)
/* Set the target system */
{
    /* Accepted for backwards compatibility only. The toolchain always targets
    ** the Atari Lynx, so any other target is rejected and "lynx" is a no-op.
    */
    if (StrCaseCmp (Arg, "lynx") != 0) {
        Error ("No such target system: '%s' (only 'lynx' is supported)", Arg);
    }
}



static void OptNoTargetLib (const char* Opt attribute ((unused)),
                            const char* Arg attribute ((unused)))
/* Disable the target library */
{
    NoTargetLib = 1;
}



static void OptNoSDKLibs (const char* Opt attribute ((unused)),
                          const char* Arg attribute ((unused)))
/* Append only the core target library, not the optional SDK set */
{
    NoSDKLibs = 1;
}



static void OptSDKLibs (const char* Opt attribute ((unused)), const char* Arg)
/* Use an alternative SDK library manifest */
{
    SDKLibsName = Arg;
}



static void OptObj (const char* Opt attribute ((unused)), const char* Arg)
/* Object file follows (linker) */
{
    CmdAddArg2 (&LD65, "--obj", Arg);
}



static void OptObjPath (const char* Opt attribute ((unused)), const char* Arg)
/* Object file search path (linker) */
{
    CmdAddArg2 (&LD65, "--obj-path", Arg);
}



static void OptPrintTargetPath (const char* Opt attribute ((unused)),
                                const char* Arg attribute ((unused)))
/* Print the target file path */
{
    char* TargetPath;

    SearchPaths* TargetPaths = NewSearchPath ();
    AddSubSearchPathFromEnv (TargetPaths, "CC65_HOME", "target");
#if defined(CL65_TGT) && !defined(_WIN32)
    AddSearchPath (TargetPaths, STRINGIZE (CL65_TGT));
#endif
    AddSubSearchPathFromWinBin (TargetPaths, "target");

    TargetPath = GetSearchPath (TargetPaths, 0);
    while (*TargetPath) {
        if (*TargetPath == ' ') {
            /* Escape spaces */
            putchar ('\\');
        }
        putchar (*TargetPath++);
    }
    putchar ('\n');
    exit (EXIT_SUCCESS);
}



static void OptRegisterSpace (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --register-space option */
{
    CmdAddArg2 (&CC65, "--register-space", Arg);
}



static void OptRegisterVars (const char* Opt attribute ((unused)),
                             const char* Arg attribute ((unused)))
/* Handle the --register-vars option */
{
    CmdAddArg (&CC65, "-r");
}



static void OptRodataName (const char* Opt attribute ((unused)), const char* Arg)
/* Handle the --rodata-name option */
{
    CmdAddArg2 (&CC65, "--rodata-name", Arg);
}



static void OptSignedChars (const char* Opt attribute ((unused)),
                            const char* Arg attribute ((unused)))
/* Make default characters signed */
{
    CmdAddArg (&CC65, "-j");
}



static void OptStartAddr (const char* Opt attribute ((unused)), const char* Arg)
/* Set the default start address */
{
    CmdAddArg2 (&LD65, "-S", Arg);
}



static void OptStaticLocals (const char* Opt attribute ((unused)),
                             const char* Arg attribute ((unused)))
/* Place local variables in static storage */
{
    CmdAddArg (&CC65, "-Cl");
}



static void OptVerbose (const char* Opt attribute ((unused)),
                        const char* Arg attribute ((unused)))
/* Verbose mode (compiler, assembler, linker) */
{
    CmdAddArg (&CC65, "-v");
    CmdAddArg (&CA65, "-v");
    CmdAddArg (&LD65, "-v");
}



static void OptVersion (const char* Opt attribute ((unused)),
                        const char* Arg attribute ((unused)))
/* Print version number */
{
    fprintf (stderr, "%s V%s\n", ProgName, GetVersionAsString ());
    exit(EXIT_SUCCESS);
}



int main (int argc, char* argv [])
/* Utility main program */
{
    /* Program long options */
    static const LongOpt OptTab[] = {
        { "--add-source",        0, OptAddSource      },
        { "--all-cdecl",         0, OptAllCDecl       },
        { "--asm-args",          1, OptAsmArgs        },
        { "--asm-define",        1, OptAsmDefine      },
        { "--asm-include-dir",   1, OptAsmIncludeDir  },
        { "--bin-include-dir",   1, OptBinIncludeDir  },
        { "--bss-name",          1, OptBssName        },
        { "--cc-args",           1, OptCCArgs         },
        { "--cfg-path",          1, OptCfgPath        },
        { "--check-stack",       0, OptCheckStack     },
        { "--code-name",         1, OptCodeName       },
        { "--codesize",          1, OptCodeSize       },
        { "--config",            1, OptConfig         },
        { "--cpu",               1, OptCPU            },
        { "--create-dep",        1, OptCreateDep      },
        { "--create-full-dep",   1, OptCreateFullDep  },
        { "--data-name",         1, OptDataName       },
        { "--debug",             0, OptDebug          },
        { "--debug-info",        0, OptDebugInfo      },
        { "--feature",           1, OptFeature        },
        { "--force-import",      1, OptForceImport    },
        { "--help",              0, OptHelp           },
        { "--include-dir",       1, OptIncludeDir     },
        { "--ld-args",           1, OptLdArgs         },
        { "--lib-path",          1, OptLibPath        },
        { "--listing",           1, OptListing        },
        { "--list-bytes",        1, OptListBytes      },
        { "--mapfile",           1, OptMapFile        },
        { "--no-sdk-libs",       0, OptNoSDKLibs      },
        { "--no-target-lib",     0, OptNoTargetLib    },
        { "--obj",               1, OptObj            },
        { "--obj-path",          1, OptObjPath        },
        { "--print-target-path", 0, OptPrintTargetPath},
        { "--register-space",    1, OptRegisterSpace  },
        { "--register-vars",     0, OptRegisterVars   },
        { "--rodata-name",       1, OptRodataName     },
        { "--sdk-libs",          1, OptSDKLibs        },
        { "--signed-chars",      0, OptSignedChars    },
        { "--start-addr",        1, OptStartAddr      },
        { "--static-locals",     0, OptStaticLocals   },
        { "--target",            1, OptTarget         },
        { "--verbose",           0, OptVerbose        },
        { "--version",           0, OptVersion        },
    };

    char* CmdPath;
    unsigned I;

    /* Initialize the cmdline module */
    InitCmdLine (&argc, &argv, "cl65");

    /* Initialize the command descriptors */
    if (argc == 0) {
        CmdPath = xstrdup ("");
    } else {
        char* Ptr;
        CmdPath = xstrdup (argv[0]);
        Ptr = strrchr (CmdPath, '/');
        if (Ptr == 0) {
            Ptr = strrchr (CmdPath, '\\');
        }
        if (Ptr == 0) {
            *CmdPath = '\0';
        } else {
            *(Ptr + 1) = '\0';
        }
    }
    CmdInit (&CC65, CmdPath, "cc65");
    CmdInit (&CA65, CmdPath, "ca65");
    CmdInit (&LD65, CmdPath, "ld65");
    xfree (CmdPath);

    /* Our default target is the Lynx instead of "none" -
    ** the only target with library support in this tree.
    */
    Target = TGT_LYNX;

    /* Check the parameters */
    I = 1;
    while (I < ArgCount) {

        /* Get the argument */
        const char* Arg = ArgVec[I];

        /* Check for an option */
        if (Arg [0] == '-') {

            switch (Arg [1]) {

                case '-':
                    LongOption (&I, OptTab, sizeof(OptTab)/sizeof(OptTab[0]));
                    break;

                case 'C':
                    if (Arg[2] == 'l' && Arg[3] == '\0') {
                        /* Make local variables static */
                        OptStaticLocals (Arg, 0);
                    } else {
                        /* Specify linker config file */
                        OptConfig (Arg, GetArg (&I, 2));
                    }
                    break;

                case 'D':
                    /* Define a preprocessor symbol (compiler) */
                    CmdAddArg2 (&CC65, "-D", GetArg (&I, 2));
                    break;

                case 'I':
                    /* Include directory (compiler) */
                    OptIncludeDir (Arg, GetArg (&I, 2));
                    break;

                case 'L':
                    if (Arg[2] == 'n' && Arg[3] == '\0') {
                        /* VICE label file (linker) */
                        CmdAddArg2 (&LD65, "-Ln", GetArg (&I, 3));
                    } else {
                        /* Library search path (linker) */
                        OptLibPath (Arg, GetArg (&I, 2));
                    }
                    break;

                case 'O':
                    /* Optimize code (compiler, also covers -Oi and others) */
                    CmdAddArg (&CC65, Arg);
                    break;

                case 'S':
                    /* Dont assemble and link the created files */
                    DisableAssemblingAndLinking ();
                    break;

                case 'T':
                    /* Include source as comment (compiler) */
                    OptAddSource (Arg, 0);
                    break;

                case 'V':
                    /* Print version number */
                    OptVersion (Arg, 0);
                    break;

                case 'E':
                    /* Forward -E to compiler */
                    CmdAddArg (&CC65, Arg);  
                    DisableAssemblingAndLinking ();
                    break;

                case 'W':
                    if (Arg[2] == 'a' && Arg[3] == '\0') {
                        /* -Wa: Pass options to assembler */
                        OptAsmArgs (Arg, GetArg (&I, 3));
                    } else if (Arg[2] == 'c' && Arg[3] == '\0') {
                        /* -Wc: Pass options to compiler */
                        /* Remember -Wc sub arguments in cc65 arg struct */ 
                        OptCCArgs (Arg, GetArg (&I, 3));
                    } else if (Arg[2] == 'l' && Arg[3] == '\0') {
                        /* -Wl: Pass options to linker */
                        OptLdArgs (Arg, GetArg (&I, 3));
                    } else {
                        /* Anything else: Suppress warnings (compiler) */
                        CmdAddArg2 (&CC65, "-W", GetArg (&I, 2));
                    }
                    break;

                case 'c':
                    /* Don't link the resulting files */
                    DisableLinking ();
                    break;

                case 'd':
                    /* Debug mode (compiler) */
                    OptDebug (Arg, 0);
                    break;

                case 'g':
                    /* Debugging - add to compiler and assembler */
                    OptDebugInfo (Arg, 0);
                    break;

                case 'h':
                case '?':
                    /* Print help - cl65 */
                    OptHelp (Arg, 0);
                    break;

                case 'j':
                    /* Default characters are signed */
                    OptSignedChars (Arg, 0);
                    break;

                case 'l':
                    /* Create an assembler listing */
                    OptListing (Arg, GetArg (&I, 2));
                    break;

                case 'm':
                    /* Create a map file (linker) */
                    OptMapFile (Arg, GetArg (&I, 2));
                    break;

                case 'o':
                    /* Name the output file */
                    OutputName = GetArg (&I, 2);
                    break;

                case 'r':
                    /* Enable register variables */
                    OptRegisterVars (Arg, 0);
                    break;

                case 't':
                    /* Set the target system */
                    OptTarget (Arg, GetArg (&I, 2));
                    break;

                case 'u':
                    /* Force an import (linker) */
                    OptForceImport (Arg, GetArg (&I, 2));
                    break;

                case 'v':
                    if (Arg [2] == 'm') {
                        /* Verbose map file (linker) */
                        CmdAddArg (&LD65, "-vm");
                    } else {
                        /* Verbose mode (compiler, assembler, linker) */
                        OptVerbose (Arg, 0);
                    }
                    break;

                default:
                    UnknownOption (Arg);
            }
        } else {

            /* Remember the first file name */
            if (FirstInput == 0) {
                FirstInput = Arg;
            }

            /* Determine the file type by the extension */
            switch (GetFileType (Arg)) {

                case FILETYPE_C:
                    /* Compile the file */
                    Compile (Arg);
                    break;

                case FILETYPE_ASM:
                    /* Assemble the file */
                    if (DoAssemble) {
                        Assemble (Arg);
                    }
                    break;

                case FILETYPE_OBJ:
                case FILETYPE_LIB:
                    /* Add to the linker files */
                    CmdAddFile (&LD65, Arg);
                    break;

                default:
                    Error ("Don't know what to do with '%s'", Arg);

            }

        }

        /* Next argument */
        ++I;
    }

    /* Check if we had any input files */
    if (FirstInput == 0) {
        Warning ("No input files");
    }

    /* Link the given files if requested and if we have any */
    if (DoLink && LD65.FileCount > 0) {
        Link ();
    }

    /* Return an apropriate exit code */
    return EXIT_SUCCESS;
}
