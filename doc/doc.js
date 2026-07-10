// Shared documentation chrome + behaviour for the lynxcc doc set.
//
// The topbar navigation and page footer are defined ONCE here and injected
// into every page via the <site-nav> and <site-foot> custom elements, so a
// change to the top bar (links, labels, ordering) or the footer only needs to
// be made in this file -- not in all the *.html pages.  Each page carries just
// a <site-nav></site-nav> placeholder near the top of <body> and a
// <site-foot></site-foot> near the end.  See design/DOC_STRUCTURE_DESIGN.md.
//
// Both elements render into the light DOM (no shadow root) so the existing
// doc.css rules for .topbar / .nav / .dropdown / .site-foot apply unchanged.
// The active page is detected from the current filename, so pages need not
// declare which nav entry is "current".

// Canonical top-bar markup (single source of truth), with no active classes.
var TOPBAR_HTML = `<div class="topbar"><a class="brand" href="index.html"><img class="brand-logo" src="logo.svg" alt=""><span>lynxcc</span></a><nav class="nav"><div class="dropdown"><button type="button" class="dropbtn" aria-haspopup="true" aria-expanded="false">Usage</button><div class="dropdown-menu"><a href="intro.html"><span>Introduction</span><span class="tdesc">hello world tutorial</span></a><a href="coding.html"><span>Coding hints</span><span class="tdesc">effective cc65 code</span></a><a href="smc.html"><span>Self-modifying code</span><span class="tdesc">SMC ca65 macros</span></a><a href="using-make.html"><span>Using make</span><span class="tdesc">build with GNU Make</span></a><a href="migrating.html"><span>Migrating</span><span class="tdesc">port from cc65</span></a><a href="samples.html"><span>Sample programs</span><span class="tdesc">annotated examples</span></a></div></div><div class="dropdown"><button type="button" class="dropbtn" aria-haspopup="true" aria-expanded="false">Tools</button><div class="dropdown-menu"><a href="abccc.html"><span>abccc</span><span class="tdesc">tune compiler</span></a><a href="abcrom.html"><span>abcrom</span><span class="tdesc">tune test-ROM</span></a><a href="ar65.html"><span>ar65</span><span class="tdesc">object archiver</span></a><a href="ca65.html"><span>ca65</span><span class="tdesc">macro assembler</span></a><a href="cc65.html"><span>cc65</span><span class="tdesc">C compiler</span></a><a href="cl65.html"><span>cl65</span><span class="tdesc">compile &amp; link</span></a><a href="da65.html"><span>da65</span><span class="tdesc">disassembler</span></a><a href="ld65.html"><span>ld65</span><span class="tdesc">linker</span></a><a href="lnx.html"><span>lnx</span><span class="tdesc">cartridge tool</span></a><a href="sp65.html"><span>sp65</span><span class="tdesc">sprite &amp; bitmap</span></a><a href="sprpck.html"><span>sprpck</span><span class="tdesc">BMP/SPS packer</span></a></div></div><div class="dropdown"><button type="button" class="dropbtn" aria-haspopup="true" aria-expanded="false">Reference</button><div class="dropdown-menu"><a href="funcref.html"><span>Function reference</span><span class="tdesc">standard library</span></a><a href="cart.html"><span>Cart ROM &amp; EEPROM</span><span class="tdesc">image, saves &amp; I/O</span></a><a href="collisions.html"><span>Collision detection</span><span class="tdesc">buffer &amp; depository</span></a><a href="comlynx.html"><span>ComLynx serial</span><span class="tdesc">link-up &amp; UART</span></a><a href="graphics.html"><span>Graphics</span><span class="tdesc">display, double buffering</span></a><a href="lynx_gfx_fonts.html"><span>Graphics fonts</span><span class="tdesc">glyph tables</span></a><a href="joypad.html"><span>Joypad input</span><span class="tdesc">buttons &amp; debouncing</span></a><a href="memory.html"><span>Memory</span><span class="tdesc">layout, stack &amp; heap</span></a><a href="sdcard-gd.html"><span>SD/GD flash cart</span><span class="tdesc">RetroHQ cartridge</span></a><a href="sound.html"><span>Sound</span><span class="tdesc">music engine</span></a><a href="sprites.html"><span>Sprites</span><span class="tdesc">SCBs, types &amp; penpal</span></a></div></div><div class="dropdown"><button type="button" class="dropbtn" aria-haspopup="true" aria-expanded="false">About</button><div class="dropdown-menu"><a href="history.html"><span>History</span><span class="tdesc">authors &amp; credits</span></a><a href="licenses.html"><span>Licenses</span><span class="tdesc">copyright notices</span></a><a href="optlim.html"><span>Optimizations &amp; Limitations</span><span class="tdesc">code-gen &amp; limits</span></a></div></div></nav><button id="docSearchBtn" class="doc-search-btn" aria-label="Search documentation" title="Search  (press / or Ctrl-K)"><span class="ic-search">⌕</span></button><button id="themeToggle" class="theme-toggle" aria-label="Toggle theme" title="Toggle light/dark"><span class="ic-sun">☀</span><span class="ic-moon">☾</span></button></div>`;

// Section-heading search index for the quick-jump palette.  Rows are
// [page, anchor-id, heading-label].  This block is GENERATED from the doc pages
// by `make doc-search-index` (doc/gen-search-index.py) -- do not hand-edit the
// array; edit a heading in its page and regenerate.  See
// design/DOC_SEARCH_DESIGN.md.
// === SEARCH_INDEX — generated by `make doc-search-index`; do not hand-edit ===
var SEARCH_INDEX = [
  ["abccc.html","sect-1","1. Overview"],
  ["abccc.html","sect-2","2. The ABC source language"],
  ["abccc.html","sect-2-1","2.1. Notes, rests and durations"],
  ["abccc.html","sect-2-2","2.2. Inline commands"],
  ["abccc.html","sect-2-3","2.3. Repeats and structure"],
  ["abccc.html","sect-2-4","2.4. Envelopes: AHD and looping"],
  ["abccc.html","sect-3","3. Usage"],
  ["abccc.html","sect-3-1","3.1. Command line"],
  ["abccc.html","sect-3-2","3.2. The build flow"],
  ["abccc.html","sect-4","4. Copyright"],
  ["abcrom.html","sect-1","1. Overview"],
  ["abcrom.html","sect-2","2. How the template patcher works"],
  ["abcrom.html","sect-3","3. Usage"],
  ["abcrom.html","sect-4","4. The template ROM"],
  ["abcrom.html","sect-5","5. Copyright"],
  ["ar65.html","sect-1","1. Overview"],
  ["ar65.html","sect-2","2. Usage"],
  ["ar65.html","sect-3","3. Copyright"],
  ["ca65.html","sect-1","1. Overview"],
  ["ca65.html","sect-1-1","1.1. Design criteria"],
  ["ca65.html","sect-2","2. Usage"],
  ["ca65.html","sect-2-1","2.1. Command line option overview"],
  ["ca65.html","sect-2-2","2.2. Command line options in detail"],
  ["ca65.html","search-paths","3. Search paths"],
  ["ca65.html","sect-4","4. Input format"],
  ["ca65.html","sect-4-1","4.1. Assembler syntax"],
  ["ca65.html","sect-4-6","4.2. Number format"],
  ["ca65.html","sect-4-7","4.3. Conditional assembly"],
  ["ca65.html","sect-5","5. Expressions"],
  ["ca65.html","sect-5-1","5.1. Expression evaluation"],
  ["ca65.html","sect-5-2","5.2. Size of an expression result"],
  ["ca65.html","sect-5-3","5.3. Boolean expressions"],
  ["ca65.html","sect-5-4","5.4. Constant expressions"],
  ["ca65.html","operators","5.5. Available operators"],
  ["ca65.html","sect-6","6. Symbols and labels"],
  ["ca65.html","sect-6-1","6.1. Numeric constants"],
  ["ca65.html","sect-6-2","6.2. Numeric variables"],
  ["ca65.html","sect-6-3","6.3. Standard labels"],
  ["ca65.html","sect-6-4","6.4. Local labels and symbols"],
  ["ca65.html","sect-6-5","6.5. Cheap local labels"],
  ["ca65.html","sect-6-6","6.6. Unnamed labels"],
  ["ca65.html","sect-6-7","6.7. Using macros to define labels and constants"],
  ["ca65.html","sect-6-8","6.8. Symbols and .DEBUGINFO"],
  ["ca65.html","scopes","7. Scopes"],
  ["ca65.html","sect-7-1","7.1. Global scope"],
  ["ca65.html","sect-7-2","7.2. Cheap locals"],
  ["ca65.html","sect-7-3","7.3. Generic nested scopes"],
  ["ca65.html","sect-7-4","7.4. Nested procedures"],
  ["ca65.html","sect-7-5","7.5. Structs, unions and enums"],
  ["ca65.html","scopesyntax","7.6. Explicit scope specification"],
  ["ca65.html","scopesearch","7.7. Scope search order"],
  ["ca65.html","address-sizes","8. Address sizes and memory models"],
  ["ca65.html","sect-8-1","8.1. Address sizes"],
  ["ca65.html","sect-8-2","8.2. Address sizes of segments"],
  ["ca65.html","sect-8-3","8.3. Address sizes of symbols"],
  ["ca65.html","sect-8-4","8.4. Memory models"],
  ["ca65.html","pseudo-variables","9. Pseudo variables"],
  ["ca65.html","sect-9-1","9.1. *"],
  ["ca65.html",".ASIZE","9.2. .ASIZE"],
  ["ca65.html",".CPU","9.3. .CPU"],
  ["ca65.html",".ISIZE","9.4. .ISIZE"],
  ["ca65.html",".PARAMCOUNT","9.5. .PARAMCOUNT"],
  ["ca65.html",".TIME","9.6. .TIME"],
  ["ca65.html",".VERSION","9.7. .VERSION"],
  ["ca65.html","pseudo-functions","10. Pseudo functions"],
  ["ca65.html",".ADDRSIZE","10.1. .ADDRSIZE"],
  ["ca65.html",".BANK","10.2. .BANK"],
  ["ca65.html",".BANKBYTE","10.3. .BANKBYTE"],
  ["ca65.html",".BLANK","10.4. .BLANK"],
  ["ca65.html",".CONCAT","10.5. .CONCAT"],
  ["ca65.html",".CONST","10.6. .CONST"],
  ["ca65.html",".HIBYTE","10.7. .HIBYTE"],
  ["ca65.html",".HIWORD","10.8. .HIWORD"],
  ["ca65.html",".IDENT","10.9. .IDENT"],
  ["ca65.html",".LEFT","10.10. .LEFT"],
  ["ca65.html",".LOBYTE","10.11. .LOBYTE"],
  ["ca65.html",".LOWORD","10.12. .LOWORD"],
  ["ca65.html",".MATCH","10.13. .MATCH"],
  ["ca65.html",".MAX","10.14. .MAX"],
  ["ca65.html",".MID","10.15. .MID"],
  ["ca65.html",".MIN","10.16. .MIN"],
  ["ca65.html",".REFERENCED","10.17. .REF, .REFERENCED"],
  ["ca65.html",".RIGHT","10.18. .RIGHT"],
  ["ca65.html",".SIZEOF","10.19. .SIZEOF"],
  ["ca65.html",".STRAT","10.20. .STRAT"],
  ["ca65.html",".SPRINTF","10.21. .SPRINTF"],
  ["ca65.html",".STRING","10.22. .STRING"],
  ["ca65.html",".STRLEN","10.23. .STRLEN"],
  ["ca65.html",".TCOUNT","10.24. .TCOUNT"],
  ["ca65.html",".XMATCH","10.25. .XMATCH"],
  ["ca65.html","control-commands","11. Control commands"],
  ["ca65.html",".ADDR","11.1. .ADDR"],
  ["ca65.html",".ALIGN","11.2. .ALIGN"],
  ["ca65.html",".ASCIIZ","11.3. .ASCIIZ"],
  ["ca65.html",".ASSERT","11.4. .ASSERT"],
  ["ca65.html",".AUTOIMPORT","11.5. .AUTOIMPORT"],
  ["ca65.html",".BANKBYTES","11.6. .BANKBYTES"],
  ["ca65.html",".BSS","11.7. .BSS"],
  ["ca65.html",".BYTE","11.8. .BYT, .BYTE"],
  ["ca65.html",".CASE","11.9. .CASE"],
  ["ca65.html",".CHARMAP","11.10. .CHARMAP"],
  ["ca65.html",".CODE","11.11. .CODE"],
  ["ca65.html",".CONDES","11.12. .CONDES"],
  ["ca65.html",".CONSTRUCTOR","11.13. .CONSTRUCTOR"],
  ["ca65.html",".DATA","11.14. .DATA"],
  ["ca65.html",".DBYT","11.15. .DBYT"],
  ["ca65.html",".DEBUGINFO","11.16. .DEBUGINFO"],
  ["ca65.html",".DEFINE","11.17. .DEFINE"],
  ["ca65.html",".DELMACRO","11.18. .DELMAC, .DELMACRO"],
  ["ca65.html",".DEFINED","11.19. .DEF, .DEFINED"],
  ["ca65.html",".DEFINEDMACRO","11.20. .DEFINEDMACRO"],
  ["ca65.html",".DESTRUCTOR","11.21. .DESTRUCTOR"],
  ["ca65.html",".DWORD","11.22. .DWORD"],
  ["ca65.html",".ELSE","11.23. .ELSE"],
  ["ca65.html",".ELSEIF","11.24. .ELSEIF"],
  ["ca65.html",".END","11.25. .END"],
  ["ca65.html",".ENDENUM","11.26. .ENDENUM"],
  ["ca65.html",".ENDIF","11.27. .ENDIF"],
  ["ca65.html",".ENDMACRO","11.28. .ENDMAC, .ENDMACRO"],
  ["ca65.html",".ENDPROC","11.29. .ENDPROC"],
  ["ca65.html",".ENDREPEAT","11.30. .ENDREP, .ENDREPEAT"],
  ["ca65.html",".ENDSCOPE","11.31. .ENDSCOPE"],
  ["ca65.html",".ENDSTRUCT","11.32. .ENDSTRUCT"],
  ["ca65.html",".ENDUNION","11.33. .ENDUNION"],
  ["ca65.html",".ENUM","11.34. .ENUM"],
  ["ca65.html",".ERROR","11.35. .ERROR"],
  ["ca65.html",".EXITMACRO","11.36. .EXITMAC, .EXITMACRO"],
  ["ca65.html",".EXPORT","11.37. .EXPORT"],
  ["ca65.html",".EXPORTZP","11.38. .EXPORTZP"],
  ["ca65.html",".FARADDR","11.39. .FARADDR"],
  ["ca65.html",".FATAL","11.40. .FATAL"],
  ["ca65.html",".FEATURE","11.41. .FEATURE"],
  ["ca65.html",".FOPT","11.42. .FILEOPT, .FOPT"],
  ["ca65.html",".FORCEIMPORT","11.43. .FORCEIMPORT"],
  ["ca65.html",".GLOBAL","11.44. .GLOBAL"],
  ["ca65.html",".GLOBALZP","11.45. .GLOBALZP"],
  ["ca65.html",".HIBYTES","11.46. .HIBYTES"],
  ["ca65.html",".IF","11.47. .IF"],
  ["ca65.html",".IFBLANK","11.48. .IFBLANK"],
  ["ca65.html",".IFCONST","11.49. .IFCONST"],
  ["ca65.html",".IFDEF","11.50. .IFDEF"],
  ["ca65.html",".IFNBLANK","11.51. .IFNBLANK"],
  ["ca65.html",".IFNDEF","11.52. .IFNDEF"],
  ["ca65.html",".IFNREF","11.53. .IFNREF"],
  ["ca65.html",".IFP02","11.54. .IFP02"],
  ["ca65.html",".IFPSC02","11.55. .IFPSC02"],
  ["ca65.html",".IFREF","11.56. .IFREF"],
  ["ca65.html",".IMPORT","11.57. .IMPORT"],
  ["ca65.html",".IMPORTZP","11.58. .IMPORTZP"],
  ["ca65.html",".INCBIN","11.59. .INCBIN"],
  ["ca65.html",".INCLUDE","11.60. .INCLUDE"],
  ["ca65.html",".INTERRUPTOR","11.61. .INTERRUPTOR"],
  ["ca65.html",".ISMNEMONIC","11.62. .ISMNEM, .ISMNEMONIC"],
  ["ca65.html",".LINECONT","11.63. .LINECONT"],
  ["ca65.html",".LIST","11.64. .LIST"],
  ["ca65.html",".LISTBYTES","11.65. .LISTBYTES"],
  ["ca65.html",".LOBYTES","11.66. .LOBYTES"],
  ["ca65.html",".LOCAL","11.67. .LOCAL"],
  ["ca65.html",".LOCALCHAR","11.68. .LOCALCHAR"],
  ["ca65.html",".MACPACK","11.69. .MACPACK"],
  ["ca65.html",".MACRO","11.70. .MAC, .MACRO"],
  ["ca65.html",".ORG","11.71. .ORG"],
  ["ca65.html",".OUT","11.72. .OUT"],
  ["ca65.html",".P02","11.73. .P02"],
  ["ca65.html",".PAGELENGTH","11.74. .PAGELEN, .PAGELENGTH"],
  ["ca65.html",".POPCPU","11.75. .POPCPU"],
  ["ca65.html",".POPSEG","11.76. .POPSEG"],
  ["ca65.html",".PROC","11.77. .PROC"],
  ["ca65.html",".PSC02","11.78. .PSC02"],
  ["ca65.html",".PUSHCPU","11.79. .PUSHCPU"],
  ["ca65.html",".PUSHSEG","11.80. .PUSHSEG"],
  ["ca65.html",".RELOC","11.81. .RELOC"],
  ["ca65.html",".REPEAT","11.82. .REPEAT"],
  ["ca65.html",".RES","11.83. .RES"],
  ["ca65.html",".RODATA","11.84. .RODATA"],
  ["ca65.html",".SCOPE","11.85. .SCOPE"],
  ["ca65.html",".SEGMENT","11.86. .SEGMENT"],
  ["ca65.html",".SET","11.87. .SET"],
  ["ca65.html",".SETCPU","11.88. .SETCPU"],
  ["ca65.html",".SMART","11.89. .SMART"],
  ["ca65.html",".STRUCT","11.90. .STRUCT"],
  ["ca65.html",".TAG","11.91. .TAG"],
  ["ca65.html",".UNDEFINE","11.92. .UNDEF, .UNDEFINE"],
  ["ca65.html",".UNION","11.93. .UNION"],
  ["ca65.html",".WARNING","11.94. .WARNING"],
  ["ca65.html",".WORD","11.95. .WORD"],
  ["ca65.html",".ZEROPAGE","11.96. .ZEROPAGE"],
  ["ca65.html","macros","12. Macros"],
  ["ca65.html","sect-12-1","12.1. Introduction"],
  ["ca65.html","sect-12-2","12.2. Macros without parameters"],
  ["ca65.html","sect-12-3","12.3. Parametrized macros"],
  ["ca65.html","sect-12-4","12.4. Detecting parameter types"],
  ["ca65.html","sect-12-5","12.5. Recursive macros"],
  ["ca65.html","sect-12-6","12.6. Local symbols inside macros"],
  ["ca65.html","sect-12-7","12.7. C style macros"],
  ["ca65.html","sect-12-8","12.8. Characters in macros"],
  ["ca65.html","sect-12-9","12.9. Deleting macros"],
  ["ca65.html","macropackages","13. Macro packages"],
  ["ca65.html","sect-13-1","13.1. .MACPACK generic"],
  ["ca65.html","sect-13-2","13.2. .MACPACK longbranch"],
  ["ca65.html","sect-13-3","13.3. .MACPACK cpu"],
  ["ca65.html","sect-13-4","13.4. .MACPACK module"],
  ["ca65.html","predefined-constants","14. Predefined constants"],
  ["ca65.html","structs","15. Structs and unions"],
  ["ca65.html","sect-15-1","15.1. Structs and unions Overview"],
  ["ca65.html","sect-15-2","15.2. Declaration"],
  ["ca65.html","sect-15-3","15.3. The storage allocator keywords"],
  ["ca65.html","sect-15-4","15.4. The .ORG keyword"],
  ["ca65.html","sect-15-5","15.5. The .TAG keyword"],
  ["ca65.html","sect-15-6","15.6. Limitations"],
  ["ca65.html","condes","16. Module constructors/destructors"],
  ["ca65.html","sect-16-1","16.1. Module constructors/destructors Overview"],
  ["ca65.html","sect-16-2","16.2. Calling order"],
  ["ca65.html","sect-16-3","16.3. Pitfalls"],
  ["ca65.html","sect-17","17. Porting sources from other assemblers"],
  ["ca65.html","sect-17-1","17.1. TASS"],
  ["ca65.html","sect-18","18. Copyright"],
  ["cart.html","sect-1","1. Binary format"],
  ["cart.html","sect-1-1","1.1. Generating fixed-size cart images"],
  ["cart.html","sect-2","2. Cart access"],
  ["cart.html","sect-3","3. Reading data from the cart"],
  ["cart.html","sect-4","4. EEPROM (cart save data)"],
  ["cc65.html","sect-1","1. Overview"],
  ["cc65.html","sect-2","2. Usage"],
  ["cc65.html","sect-2-1","2.1. Command line option overview"],
  ["cc65.html","sect-2-2","2.2. Command line options in detail"],
  ["cc65.html","sect-3","3. Input and output"],
  ["cc65.html","sect-4","4. Differences to the ISO standard"],
  ["cc65.html","sect-5","5. Extensions"],
  ["cc65.html","sect-6","6. Predefined macros"],
  ["cc65.html","pragmas","7. #pragmas"],
  ["cc65.html","pragma-allow-eager-inline","7.1. #pragma allow-eager-inline ([push,] on|off)"],
  ["cc65.html","pragma-bss-name","7.2. #pragma bss-name ([push,] <name>)"],
  ["cc65.html","pragma-charmap","7.3. #pragma charmap (<index>, <code>)"],
  ["cc65.html","pragma-check-stack","7.4. #pragma check-stack ([push,] on|off)"],
  ["cc65.html","pragma-code-name","7.5. #pragma code-name ([push,] <name>)"],
  ["cc65.html","pragma-codesize","7.6. #pragma codesize ([push,] <int>)"],
  ["cc65.html","pragma-data-name","7.7. #pragma data-name ([push,] <name>)"],
  ["cc65.html","pragma-inline-stdfuncs","7.8. #pragma inline-stdfuncs ([push,] on|off)"],
  ["cc65.html","pragma-local-strings","7.9. #pragma local-strings ([push,] on|off)"],
  ["cc65.html","pragma-message","7.10. #pragma message (<message>)"],
  ["cc65.html","pragma-optimize","7.11. #pragma optimize ([push,] on|off)"],
  ["cc65.html","pragma-rodata-name","7.12. #pragma rodata-name ([push,] <name>)"],
  ["cc65.html","pragma-regvaraddr","7.13. #pragma regvaraddr ([push,] on|off)"],
  ["cc65.html","pragma-register-vars","7.14. #pragma register-vars ([push,] on|off)"],
  ["cc65.html","pragma-signed-chars","7.15. #pragma signed-chars ([push,] on|off)"],
  ["cc65.html","pragma-static-locals","7.16. #pragma static-locals ([push,] on|off)"],
  ["cc65.html","pragma-warn","7.17. #pragma warn (name, [push,] on|off)"],
  ["cc65.html","pragma-wrapped-call","7.18. #pragma wrapped-call (push, <name>, <identifier>)"],
  ["cc65.html","pragma-writable-strings","7.19. #pragma writable-strings ([push,] on|off)"],
  ["cc65.html","sect-7-20","7.20. #pragma zpsym (<name>)"],
  ["cc65.html","var-storage","8. Variable storage and lifetime"],
  ["cc65.html","zeropage-attr","8.1. The __zeropage specifier"],
  ["cc65.html","register-vars","9. Register variables"],
  ["cc65.html","inline-asm","10. Inline assembler"],
  ["cc65.html","operators","11. The C language: operators"],
  ["cc65.html","operators-standard","11.1. Standard operators"],
  ["cc65.html","operators-bitwise","11.2. Bitwise operators"],
  ["cc65.html","operators-suzy","11.3. Suzy hardware operators"],
  ["cc65.html","asm-from-c","12. Calling assembly functions from C"],
  ["cc65.html","asm-from-c-1","12.1. Calling conventions"],
  ["cc65.html","asm-from-c-2","12.2. Prologue, before the function call"],
  ["cc65.html","asm-from-c-3","12.3. Epilogue, after the function call"],
  ["cc65.html","asm-from-c-3-1","11.3.1. Return requirements"],
  ["cc65.html","asm-from-c-3-2","11.3.2. Clobbered state"],
  ["cc65.html","diagnostics","13. Compiler diagnostics"],
  ["cc65.html","diag-format","13.1. Reading a diagnostic"],
  ["cc65.html","diag-lexical","13.2. Lexical, character and string constants"],
  ["cc65.html","diag-preproc","13.3. Preprocessor directives"],
  ["cc65.html","diag-decl","13.4. Declarations, types and initializers"],
  ["cc65.html","diag-expr","13.5. Expressions and operators"],
  ["cc65.html","diag-stmt","13.6. Statements and control flow"],
  ["cc65.html","diag-sym","13.7. Symbols, functions and scope"],
  ["cc65.html","diag-pragma","13.8. #pragma, segments and wrapped-call"],
  ["cc65.html","diag-asm","13.9. Inline assembler and code generation"],
  ["cc65.html","diag-fatal","13.10. Fatal errors"],
  ["cc65.html","sect-11","14. Copyright"],
  ["cl65.html","sect-1","1. Overview"],
  ["cl65.html","sect-2","2. Basic Usage"],
  ["cl65.html","sect-3","3. More usage"],
  ["cl65.html","sect-4","4. Examples"],
  ["cl65.html","sect-5","5. Copyright"],
  ["coding.html","sect-1","1. Use prototypes"],
  ["coding.html","sect-2","2. Don't declare auto variables in nested function blocks"],
  ["coding.html","sect-3","3. Remember that the compiler does no high level optimizations"],
  ["coding.html","sect-4","4. Longs are slow!"],
  ["coding.html","sect-5","5. Use unsigned types wherever possible"],
  ["coding.html","sect-6","6. Use chars instead of ints if possible"],
  ["coding.html","sect-7","7. Make the size of your array elements one of 1, 2, 4, 8"],
  ["coding.html","sect-8","8. Expressions are evaluated from left to right"],
  ["coding.html","sect-9","9. Use the preincrement and predecrement operators"],
  ["coding.html","sect-10","10. Use constants to access absolute memory locations"],
  ["coding.html","sect-11","11. Use initialized local variables"],
  ["coding.html","sect-12","12. Use the array operator [] even for pointers"],
  ["coding.html","sect-13","13. Use register variables with care"],
  ["coding.html","sect-14","14. Decimal constants greater than 0x7FFF are actually long ints"],
  ["coding.html","sect-15","15. Access to parameters in variadic functions is expensive"],
  ["coding.html","sect-16","16. Prefer static allocation over the heap"],
  ["coding.html","sect-16-1","16.1. Use static arrays, not dynamic allocation"],
  ["coding.html","sect-16-2","16.2. Use global variables for long-lived state"],
  ["coding.html","sect-16-3","16.3. Put hot scalars and pointers in the zero page"],
  ["coding.html","sect-16-4","16.4. Prefer a struct of arrays to an array of structs"],
  ["coding.html","sect-16-5","16.5. Avoid malloc, calloc, realloc and free"],
  ["collisions.html","sect-1","1. The collision buffer"],
  ["collisions.html","sect-2","2. Collision numbers and the depository"],
  ["collisions.html","sect-2-1","2.1. Highest number wins"],
  ["collisions.html","sect-2-2","2.2. Where the depository byte lives"],
  ["collisions.html","sect-3","3. Sprite types: opting in and out"],
  ["collisions.html","sect-4","4. Turning it on"],
  ["collisions.html","sect-4-1","4.1. Reserving the buffer: lynx-coll.cfg"],
  ["collisions.html","sect-4-2","4.2. gfx_init and gfx_setcollisiondetection"],
  ["collisions.html","sect-5","5. Worked example: collision.c"],
  ["collisions.html","sect-6","6. Limitations and gotchas"],
  ["collisions.html","sect-7","7. License"],
  ["comlynx.html","sect-1","1. Overview"],
  ["comlynx.html","sect-2","2. The Mikey UART"],
  ["comlynx.html","sect-2-1","2.1. Connector and signals"],
  ["comlynx.html","sect-2-2","2.2. Baud-rate generation"],
  ["comlynx.html","sect-2-3","2.3. Data format"],
  ["comlynx.html","sect-2-4","2.4. Break"],
  ["comlynx.html","sect-2-5","2.5. Status, errors and the interrupt quirk"],
  ["comlynx.html","sect-3","3. The lynxcc serial library"],
  ["comlynx.html","sect-3-1","3.1. Opening the port"],
  ["comlynx.html","sect-3-2","3.2. Sending and receiving"],
  ["comlynx.html","sect-3-3","3.3. Status and error codes"],
  ["comlynx.html","sect-3-4","3.4. Closing the port"],
  ["comlynx.html","sect-3-5","3.5. Quirks and limitations"],
  ["comlynx.html","sect-4","4. The Redeye protocol (historical)"],
  ["comlynx.html","sect-4-1","4.1. Round-robin communication frames"],
  ["comlynx.html","sect-4-2","4.2. Logon"],
  ["comlynx.html","sect-4-3","4.3. Normal communication"],
  ["comlynx.html","sect-4-4","4.4. Constants and switches"],
  ["comlynx.html","sect-4-5","4.5. Game ID numbers"],
  ["comlynx.html","sect-4-6","4.6. Limitations and resource cost"],
  ["comlynx.html","sect-5","5. Further reading"],
  ["comlynx.html","sect-6","6. License"],
  ["da65.html","sect-1","1. Overview"],
  ["da65.html","sect-2","2. Usage"],
  ["da65.html","sect-2-1","2.1. Command line option overview"],
  ["da65.html","sect-2-2","2.2. Command line options in detail"],
  ["da65.html","sect-3","3. Detailed workings"],
  ["da65.html","sect-3-1","3.1. Supported CPUs"],
  ["da65.html","sect-3-2","3.2. Attribute map"],
  ["da65.html","sect-3-3","3.3. Labels"],
  ["da65.html","sect-3-4","3.4. Info File"],
  ["da65.html","infofile","4. Info File Format"],
  ["da65.html","sect-4-1","4.1. Comments"],
  ["da65.html","global-options","4.2. Specifying global options"],
  ["da65.html","sect-4-3","4.3. Specifying Ranges"],
  ["da65.html","infofile-label","4.4. Specifying Labels"],
  ["da65.html","infofile-segment","4.5. Specifying Segments"],
  ["da65.html","infofile-asminc","4.6. Specifying Assembler Includes"],
  ["da65.html","sect-4-7","4.7. An Info File Example"],
  ["da65.html","sect-5","5. Copyright"],
  ["funcref.html","sect-1","1. Introduction"],
  ["funcref.html","sect-2","2. Functions by header file"],
  ["funcref.html","6502.h","2.1. 6502.h"],
  ["funcref.html","cc65.h","2.2. cc65.h"],
  ["funcref.html","ctype.h","2.3. ctype.h"],
  ["funcref.html","dirent.h","2.4. dirent.h"],
  ["funcref.html","errno.h","2.5. errno.h"],
  ["funcref.html","fcntl.h","2.6. fcntl.h"],
  ["funcref.html","joystick.h","2.7. lynx/joystick.h"],
  ["funcref.html","lynx.h","2.8. lynx/lynx.h"],
  ["funcref.html","lz4.h","2.9. lynx/lz4.h"],
  ["funcref.html","peekpoke.h","2.10. peekpoke.h"],
  ["funcref.html","sdcard-gd.h","2.11. lynx/sdcard-gd.h"],
  ["funcref.html","serial.h","2.12. lynx/serial.h"],
  ["funcref.html","setjmp.h","2.13. setjmp.h"],
  ["funcref.html","stdarg.h","2.14. stdarg.h"],
  ["funcref.html","stdbool.h","2.15. stdbool.h"],
  ["funcref.html","stddef.h","2.16. stddef.h"],
  ["funcref.html","stdio.h","2.17. stdio.h"],
  ["funcref.html","stdlib.h","2.18. stdlib.h"],
  ["funcref.html","string.h","2.19. string.h"],
  ["funcref.html","suzymath.h","2.20. lynx/suzymath.h"],
  ["funcref.html","gfx.h","2.21. lynx/gfx.h"],
  ["funcref.html","time.h","2.22. time.h"],
  ["funcref.html","unistd.h","2.23. unistd.h"],
  ["funcref.html","zlib.h","2.24. lynx/zlib.h"],
  ["funcref.html","sect-3","3. Alphabetical function reference"],
  ["funcref.html","_heapadd","3.1. _heapadd"],
  ["funcref.html","_heapblocksize","3.2. _heapblocksize"],
  ["funcref.html","_heapmaxavail","3.3. _heapmaxavail"],
  ["funcref.html","_heapmemavail","3.4. _heapmemavail"],
  ["funcref.html","_randomize","3.5. _randomize"],
  ["funcref.html","_swap","3.6. _swap"],
  ["funcref.html","_sys","3.7. _sys"],
  ["funcref.html","BRK","3.8. BRK"],
  ["funcref.html","CLI","3.9. CLI"],
  ["funcref.html","PEEK","3.10. PEEK"],
  ["funcref.html","PEEKW","3.11. PEEKW"],
  ["funcref.html","POKE","3.12. POKE"],
  ["funcref.html","POKEW","3.13. POKEW"],
  ["funcref.html","SEI","3.14. SEI"],
  ["funcref.html","abs","3.15. abs"],
  ["funcref.html","atoi","3.16. atoi"],
  ["funcref.html","atol","3.17. atol"],
  ["funcref.html","bsearch","3.18. bsearch"],
  ["funcref.html","bzero","3.19. bzero"],
  ["funcref.html","calloc","3.20. calloc"],
  ["funcref.html","clock","3.21. clock"],
  ["funcref.html","decompress_lz4","3.22. decompress_lz4"],
  ["funcref.html","div","3.23. div"],
  ["funcref.html","eeprom_93c46_read","3.24. eeprom_93c46_read"],
  ["funcref.html","eeprom_93c46_write","3.25. eeprom_93c46_write"],
  ["funcref.html","eeprom_93c66_read","3.26. eeprom_93c66_read"],
  ["funcref.html","eeprom_93c66_write","3.27. eeprom_93c66_write"],
  ["funcref.html","eeprom_93c86_read","3.28. eeprom_93c86_read"],
  ["funcref.html","eeprom_93c86_write","3.29. eeprom_93c86_write"],
  ["funcref.html","free","3.30. free"],
  ["funcref.html","gfx_busy","3.31. gfx_busy"],
  ["funcref.html","gfx_clear","3.32. gfx_clear"],
  ["funcref.html","gfx_clearrows","3.33. gfx_clearrows"],
  ["funcref.html","gfx_flip","3.34. gfx_flip"],
  ["funcref.html","gfx_getcolor","3.35. gfx_getcolor"],
  ["funcref.html","gfx_getcolorcount","3.36. gfx_getcolorcount"],
  ["funcref.html","gfx_getdefpalette","3.37. gfx_getdefpalette"],
  ["funcref.html","gfx_getmaxcolor","3.38. gfx_getmaxcolor"],
  ["funcref.html","gfx_getmaxx","3.39. gfx_getmaxx"],
  ["funcref.html","gfx_getmaxy","3.40. gfx_getmaxy"],
  ["funcref.html","gfx_getpagecount","3.41. gfx_getpagecount"],
  ["funcref.html","gfx_getpalette","3.42. gfx_getpalette"],
  ["funcref.html","gfx_gettextheight","3.43. gfx_gettextheight"],
  ["funcref.html","gfx_gettextwidth","3.44. gfx_gettextwidth"],
  ["funcref.html","gfx_getxres","3.45. gfx_getxres"],
  ["funcref.html","gfx_getyres","3.46. gfx_getyres"],
  ["funcref.html","gfx_gotoxy","3.47. gfx_gotoxy"],
  ["funcref.html","gfx_init","3.48. gfx_init"],
  ["funcref.html","gfx_outtext","3.49. gfx_outtext"],
  ["funcref.html","gfx_outtextxy","3.50. gfx_outtextxy"],
  ["funcref.html","gfx_setbgcolor","3.51. gfx_setbgcolor"],
  ["funcref.html","gfx_setbpp","3.52. gfx_setbpp"],
  ["funcref.html","gfx_setcollisiondetection","3.53. gfx_setcollisiondetection"],
  ["funcref.html","gfx_setcolor","3.54. gfx_setcolor"],
  ["funcref.html","gfx_setdrawpage","3.55. gfx_setdrawpage"],
  ["funcref.html","gfx_setfont","3.56. gfx_setfont"],
  ["funcref.html","gfx_setframerate","3.57. gfx_setframerate"],
  ["funcref.html","gfx_setpalette","3.58. gfx_setpalette"],
  ["funcref.html","gfx_settextdir","3.59. gfx_settextdir"],
  ["funcref.html","gfx_settextscale","3.60. gfx_settextscale"],
  ["funcref.html","gfx_settextstyle","3.61. gfx_settextstyle"],
  ["funcref.html","gfx_setviewpage","3.62. gfx_setviewpage"],
  ["funcref.html","gfx_sprite","3.63. gfx_sprite"],
  ["funcref.html","gfx_updatedisplay","3.64. gfx_updatedisplay"],
  ["funcref.html","isalnum","3.65. isalnum"],
  ["funcref.html","isalpha","3.66. isalpha"],
  ["funcref.html","isascii","3.67. isascii"],
  ["funcref.html","isblank","3.68. isblank"],
  ["funcref.html","isdigit","3.69. isdigit"],
  ["funcref.html","isgraph","3.70. isgraph"],
  ["funcref.html","islower","3.71. islower"],
  ["funcref.html","isprint","3.72. isprint"],
  ["funcref.html","ispunct","3.73. ispunct"],
  ["funcref.html","isspace","3.74. isspace"],
  ["funcref.html","isupper","3.75. isupper"],
  ["funcref.html","isxdigit","3.76. isxdigit"],
  ["funcref.html","itoa","3.77. itoa"],
  ["funcref.html","joy_read","3.78. joy_read"],
  ["funcref.html","labs","3.79. labs"],
  ["funcref.html","longjmp","3.80. longjmp"],
  ["funcref.html","ltoa","3.81. ltoa"],
  ["funcref.html","lynx_exec","3.82. lynx_exec"],
  ["funcref.html","lynx_load","3.83. lynx_load"],
  ["funcref.html","malloc","3.84. malloc"],
  ["funcref.html","memchr","3.85. memchr"],
  ["funcref.html","memcmp","3.86. memcmp"],
  ["funcref.html","memcpy","3.87. memcpy"],
  ["funcref.html","memmove","3.88. memmove"],
  ["funcref.html","memset","3.89. memset"],
  ["funcref.html","mikey_snd_integrate","3.90. mikey_snd_integrate"],
  ["funcref.html","mikey_snd_octave","3.91. mikey_snd_octave"],
  ["funcref.html","mikey_snd_pitch","3.92. mikey_snd_pitch"],
  ["funcref.html","mikey_snd_taps","3.93. mikey_snd_taps"],
  ["funcref.html","mikey_snd_volume","3.94. mikey_snd_volume"],
  ["funcref.html","offsetof","3.95. offsetof"],
  ["funcref.html","openn","3.96. openn"],
  ["funcref.html","qsort","3.97. qsort"],
  ["funcref.html","rand","3.98. rand"],
  ["funcref.html","realloc","3.99. realloc"],
  ["funcref.html","reset_irq","3.100. reset_irq"],
  ["funcref.html","sdcard_gd_clear","3.101. sdcard_gd_clear"],
  ["funcref.html","sdcard_gd_close","3.102. sdcard_gd_close"],
  ["funcref.html","sdcard_gd_init","3.103. sdcard_gd_init"],
  ["funcref.html","sdcard_gd_lowpower","3.104. sdcard_gd_lowpower"],
  ["funcref.html","sdcard_gd_open","3.105. sdcard_gd_open"],
  ["funcref.html","sdcard_gd_opendir","3.106. sdcard_gd_opendir"],
  ["funcref.html","sdcard_gd_program","3.107. sdcard_gd_program"],
  ["funcref.html","sdcard_gd_read","3.108. sdcard_gd_read"],
  ["funcref.html","sdcard_gd_readdir","3.109. sdcard_gd_readdir"],
  ["funcref.html","sdcard_gd_seek","3.110. sdcard_gd_seek"],
  ["funcref.html","sdcard_gd_size","3.111. sdcard_gd_size"],
  ["funcref.html","sdcard_gd_write","3.112. sdcard_gd_write"],
  ["funcref.html","ser_close","3.113. ser_close"],
  ["funcref.html","ser_get","3.114. ser_get"],
  ["funcref.html","ser_open","3.115. ser_open"],
  ["funcref.html","ser_put","3.116. ser_put"],
  ["funcref.html","ser_status","3.117. ser_status"],
  ["funcref.html","set_irq","3.118. set_irq"],
  ["funcref.html","setjmp","3.119. setjmp"],
  ["funcref.html","sleep","3.120. sleep"],
  ["funcref.html","snd_active","3.121. snd_active"],
  ["funcref.html","snd_continue","3.122. snd_continue"],
  ["funcref.html","snd_init","3.123. snd_init"],
  ["funcref.html","snd_pause","3.124. snd_pause"],
  ["funcref.html","snd_play","3.125. snd_play"],
  ["funcref.html","snd_stop","3.126. snd_stop"],
  ["funcref.html","snd_stop_channel","3.127. snd_stop_channel"],
  ["funcref.html","snprintf","3.128. snprintf"],
  ["funcref.html","sprintf","3.129. sprintf"],
  ["funcref.html","srand","3.130. srand"],
  ["funcref.html","sscanf","3.131. sscanf"],
  ["funcref.html","strcasecmp","3.132. strcasecmp"],
  ["funcref.html","strcat","3.133. strcat"],
  ["funcref.html","strchr","3.134. strchr"],
  ["funcref.html","strcmp","3.135. strcmp"],
  ["funcref.html","strcoll","3.136. strcoll"],
  ["funcref.html","strcpy","3.137. strcpy"],
  ["funcref.html","strcspn","3.138. strcspn"],
  ["funcref.html","strdup","3.139. strdup"],
  ["funcref.html","strerror","3.140. strerror"],
  ["funcref.html","stricmp","3.141. stricmp"],
  ["funcref.html","strlen","3.142. strlen"],
  ["funcref.html","strlower","3.143. strlower"],
  ["funcref.html","strlwr","3.144. strlwr"],
  ["funcref.html","strncasecmp","3.145. strncasecmp"],
  ["funcref.html","strncat","3.146. strncat"],
  ["funcref.html","strncmp","3.147. strncmp"],
  ["funcref.html","strncpy","3.148. strncpy"],
  ["funcref.html","strnicmp","3.149. strnicmp"],
  ["funcref.html","strpbrk","3.150. strpbrk"],
  ["funcref.html","strqtok","3.151. strqtok"],
  ["funcref.html","strrchr","3.152. strrchr"],
  ["funcref.html","strspn","3.153. strspn"],
  ["funcref.html","strstr","3.154. strstr"],
  ["funcref.html","strtok","3.155. strtok"],
  ["funcref.html","strupper","3.156. strupper"],
  ["funcref.html","strupr","3.157. strupr"],
  ["funcref.html","strxfrm","3.158. strxfrm"],
  ["funcref.html","suzy_div_result","3.159. suzy_div_result"],
  ["funcref.html","suzy_div_start","3.160. suzy_div_start"],
  ["funcref.html","suzy_math_busy","3.161. suzy_math_busy"],
  ["funcref.html","suzy_mod_result","3.162. suzy_mod_result"],
  ["funcref.html","suzy_mod_start","3.163. suzy_mod_start"],
  ["funcref.html","suzy_mul_result","3.164. suzy_mul_result"],
  ["funcref.html","suzy_mul_start","3.165. suzy_mul_start"],
  ["funcref.html","suzy_muldiv_result","3.166. suzy_muldiv_result"],
  ["funcref.html","suzy_muldiv_start","3.167. suzy_muldiv_start"],
  ["funcref.html","suzy_udiv_result","3.168. suzy_udiv_result"],
  ["funcref.html","suzy_udiv_start","3.169. suzy_udiv_start"],
  ["funcref.html","suzy_umod_result","3.170. suzy_umod_result"],
  ["funcref.html","suzy_umod_start","3.171. suzy_umod_start"],
  ["funcref.html","suzy_umul_result","3.172. suzy_umul_result"],
  ["funcref.html","suzy_umul_start","3.173. suzy_umul_start"],
  ["funcref.html","suzy_umuldiv_result","3.174. suzy_umuldiv_result"],
  ["funcref.html","suzy_umuldiv_start","3.175. suzy_umuldiv_start"],
  ["funcref.html","tolower","3.176. tolower"],
  ["funcref.html","toupper","3.177. toupper"],
  ["funcref.html","ultoa","3.178. ultoa"],
  ["funcref.html","utoa","3.179. utoa"],
  ["funcref.html","vsnprintf","3.180. vsnprintf"],
  ["funcref.html","vsprintf","3.181. vsprintf"],
  ["funcref.html","vsscanf","3.182. vsscanf"],
  ["funcref.html","sect-4","4. How the Suzy math functions work"],
  ["funcref.html","suzymath-model","4.1. The hardware unit and the start / poll / harvest model"],
  ["funcref.html","suzymath-divide","4.2. Why only the divide is worth overlapping"],
  ["funcref.html","suzymath-safety","4.3. Why they are safe to use"],
  ["funcref.html","sect-5","5. What is not in the library"],
  ["funcref.html","notcovered-unavailable","5.1. Functions that are not available"],
  ["funcref.html","notcovered-fileio","5.2. File I/O is read-only"],
  ["funcref.html","notcovered-limited","5.3. Functions that are limited"],
  ["funcref.html","notcovered-inlined","5.4. Extra limits when inlining with -Os"],
  ["funcref.html","notcovered-drivers","5.5. Removed driver APIs"],
  ["graphics.html","sect-1","1. The display mode"],
  ["graphics.html","sect-2","2. API overview"],
  ["graphics.html","sect-3","3. Frame buffers and double buffering"],
  ["graphics.html","sect-3-1","3.1. The two pages in memory"],
  ["graphics.html","sect-3-2","3.2. Draw page and view page"],
  ["graphics.html","sect-3-3","3.3. The deferred swap at VBL"],
  ["graphics.html","sect-4","4. Refresh rate and frame timing"],
  ["graphics.html","sect-5","5. Display depth"],
  ["graphics.html","sect-6","6. Colour and the palette"],
  ["graphics.html","sect-7","7. Sprites, collision and going deeper"],
  ["graphics.html","sect-8","8. License"],
  ["history.html","sect-1","1. Contributors at a glance"],
  ["history.html","sect-2","2. The cc65 toolchain"],
  ["history.html","sect-3","3. Atari Lynx support"],
  ["history.html","sect-4","4. Licensing"],
  ["intro.html","sect-1","1. Overview"],
  ["intro.html","sect-1-1","1.1. Before we start"],
  ["intro.html","sect-1-2","1.2. The sample modules"],
  ["intro.html","sect-1-3","1.3. Translation phases"],
  ["intro.html","sect-2","2. The compiler"],
  ["intro.html","sect-3","3. The assembler"],
  ["intro.html","sect-4","4. The linker"],
  ["intro.html","using-cl65","5. The easy way (using the cl65 utility)"],
  ["intro.html","hello-graphics","6. A Hello World you can see"],
  ["intro.html","sect-6","7. Running The Executable"],
  ["intro.html","sect-6-1","7.1. Emulators"],
  ["joypad.html","sect-1","1. Reading the joypad"],
  ["joypad.html","sect-2","2. Buttons and switches"],
  ["joypad.html","sect-3","3. Edge detection"],
  ["joypad.html","sect-4","4. Debouncing and auto-repeat"],
  ["ld65.html","sect-1","1. Overview"],
  ["ld65.html","sect-2","2. Usage"],
  ["ld65.html","sect-2-1","2.1. Command-line option overview"],
  ["ld65.html","sect-2-2","2.2. Command-line options in detail"],
  ["ld65.html","sect-3","3. Search paths"],
  ["ld65.html","sect-3-1","3.1. Library search path"],
  ["ld65.html","sect-3-2","3.2. Object file search path"],
  ["ld65.html","sect-3-3","3.3. Config file search path"],
  ["ld65.html","sect-4","4. Detailed workings"],
  ["ld65.html","config-files","5. Configuration files"],
  ["ld65.html","sect-5-1","5.1. Memory areas"],
  ["ld65.html","sect-5-2","5.2. Segments"],
  ["ld65.html","sect-5-3","5.3. Output files"],
  ["ld65.html","sect-5-4","5.4. OVERWRITE segments"],
  ["ld65.html","sect-5-5","5.5. LOAD and RUN addresses (ROMable code)"],
  ["ld65.html","sect-5-6","5.6. Other MEMORY area attributes"],
  ["ld65.html","sect-5-7","5.7. Other SEGMENT attributes"],
  ["ld65.html","sect-5-8","5.8. The FILES section"],
  ["ld65.html","FORMAT","5.9. The FORMAT section"],
  ["ld65.html","FEATURES","5.10. The FEATURES section"],
  ["ld65.html","sect-5-10-1","5.10.1. The CONDES feature"],
  ["ld65.html","sect-5-10-2","5.10.2. The STARTADDRESS feature"],
  ["ld65.html","SYMBOLS","5.11. The SYMBOLS section"],
  ["ld65.html","sect-6","6. Special segments"],
  ["ld65.html","sect-6-1","6.1. ONCE"],
  ["ld65.html","sect-6-2","6.2. LOWCODE"],
  ["ld65.html","sect-6-3","6.3. STARTUP"],
  ["ld65.html","sect-6-4","6.4. ZPSAVE"],
  ["ld65.html","sect-7","7. Copyright"],
  ["licenses.html","sect-1","1. Summary"],
  ["licenses.html","sect-2","2. cc65 license (applies to the package)"],
  ["licenses.html","sect-3","3. Original compiler copyright (John R. Dunning)"],
  ["licenses.html","sect-4","4. Bundled third-party components"],
  ["licenses.html","sect-4-1","4.1. zlib / inflate decompression"],
  ["licenses.html","sect-4-2","4.2. lz4 decompression"],
  ["licenses.html","sect-4-3","4.3. sprpck — Lynx Sprite Packer (Apache-2.0)"],
  ["licenses.html","sect-4-4","4.4. lynxdir — Lynx ROM Builder (no declared licence)"],
  ["licenses.html","sect-5","5. SDK toolchain — Mozilla Public License 2.0"],
  ["licenses.html","sect-6","6. Example games and templates — MIT License"],
  ["licenses.html","sect-7","7. Documentation — Creative Commons Attribution 4.0"],
  ["lnx.html","sect-1","1. Overview"],
  ["lnx.html","sect-2","2. The .lnx header"],
  ["lnx.html","sect-3","3. Usage"],
  ["lnx.html","sect-3-1","3.1. Command line overview"],
  ["lnx.html","sect-3-2","3.2. Commands in detail"],
  ["lnx.html","sect-3-3","3.3. Field options"],
  ["lnx.html","sect-4","4. Per-game JSON configuration"],
  ["lnx.html","sect-5","5. Examples"],
  ["lnx.html","sect-6","6. BLL objects → cartridge ROM"],
  ["lnx.html","sect-6-1","6.1. The BLL/BS93 object"],
  ["lnx.html","sect-6-2","6.2. The cartridge ROM it builds"],
  ["lnx.html","sect-6-3","6.3. Cart size and the LYNX header"],
  ["lnx.html","sect-7","7. Copyright"],
  ["memory.html","sect-1","1. Memory layout"],
  ["memory.html","sect-2","2. Memory-mapped I/O and the display overlay"],
  ["memory.html","sect-3","3. The C runtime stack and stack-overflow checking"],
  ["memory.html","sect-3-1","3.1. Setting the stack size"],
  ["memory.html","sect-3-2","3.2. Enabling stack-overflow checking"],
  ["memory.html","sect-4","4. Dynamic memory allocation (the heap)"],
  ["memory.html","sect-4-1","4.1. How allocation works"],
  ["memory.html","sect-4-2","4.2. How it deviates from POSIX"],
  ["memory.html","sect-5","5. Startup reclamation and relocation"],
  ["memory.html","sect-5-1","5.1. The split: resident stub vs one-shot body"],
  ["memory.html","sect-5-2","5.2. Relocation and reclaim"],
  ["memory.html","sect-5-3","5.3. The boot sequence"],
  ["migrating.html","sect-1","1. Overview"],
  ["migrating.html","sect-2","2. Differences from upstream cc65"],
  ["migrating.html","sect-2-1","2.1. Source- and build-level changes"],
  ["migrating.html","sect-2-2","2.2. API and runtime changes"],
  ["migrating.html","sect-3","3. Migration aids"],
  ["migrating.html","sect-3-1","3.1. The include-rewrite"],
  ["migrating.html","sect-3-2","3.2. Resolving the optional libraries"],
  ["migrating.html","sect-3-3","3.3. Reference builds"],
  ["migrating.html","sect-4","4. Checklist of concrete edits"],
  ["migrating.html","sect-5","5. Compatibility deliberately preserved"],
  ["migrating.html","sect-tgi-deprecation","6. TGI Deprecation"],
  ["migrating.html","sect-tgi-deprecation-fns","6.1. Functions"],
  ["migrating.html","sect-tgi-deprecation-macros","6.2. Query macros"],
  ["migrating.html","sect-tgi-deprecation-consts","6.3. Constants"],
  ["migrating.html","sect-eeprom-rename","7. EEPROM API rename"],
  ["optlim.html","sect-1","1. Lynx-specific code-generation optimizations"],
  ["optlim.html","sect-2","2. Limitations"],
  ["samples.html","sec-getting-started","Getting started"],
  ["samples.html","sec-games","Games"],
  ["samples.html","sec-suzy-hardware","Suzy hardware"],
  ["samples.html","sec-mikey-display","Mikey display"],
  ["samples.html","sec-mikey-audio","Mikey audio"],
  ["samples.html","sec-memory","Memory"],
  ["samples.html","sec-network","Network"],
  ["samples.html","sec-storage","Storage"],
  ["sdcard-gd.html","sect-1","1. Overview"],
  ["sdcard-gd.html","sect-2","2. Wire protocol"],
  ["sdcard-gd.html","sect-2-1","2.1. Registers and handshake"],
  ["sdcard-gd.html","sect-2-2","2.2. Command set"],
  ["sdcard-gd.html","sect-3","3. The C API"],
  ["sdcard-gd.html","sect-4","4. Result codes and directory entries"],
  ["sdcard-gd.html","sect-5","5. SD card layout"],
  ["sdcard-gd.html","sect-5-1","5.1. Recognised ROM types"],
  ["sdcard-gd.html","sect-5-2","5.2. The menu/ directory"],
  ["sdcard-gd.html","sect-5-3","5.3. Long names, previews and block sizing"],
  ["sdcard-gd.html","sect-6","6. Worked example"],
  ["sdcard-gd.html","sect-7","7. Compatibility"],
  ["smc.html","sect-1","1. Overview"],
  ["smc.html","sect-2","2. Usage"],
  ["smc.html","sect-2-1","2.1. Argument placeholders"],
  ["smc.html","sect-2-2","2.2. Accessing opcodes"],
  ["smc.html","sect-2-3","2.3. Accessing arguments"],
  ["smc.html","sect-2-4","2.4. Operational macros"],
  ["smc.html","sect-2-5","2.5. Scope macros"],
  ["smc.html","sect-3","3. A complex example"],
  ["sound.html","overview","1. Overview"],
  ["sound.html","playback","2. How playback works"],
  ["sound.html","composing","3. Composing with abccc"],
  ["sound.html","examples","4. Worked examples"],
  ["sound.html","audition","5. Auditioning with abcrom"],
  ["sound.html","sfx","6. Sound effects (sfx)"],
  ["sound.html","reference","7. Reference"],
  ["sp65.html","sect-1","1. Overview"],
  ["sp65.html","sect-2","2. Usage"],
  ["sp65.html","sect-2-1","2.1. Command line option overview"],
  ["sp65.html","sect-2-2","2.2. Command line options in detail"],
  ["sp65.html","processing-pipeline","3. Processing pipeline"],
  ["sp65.html","attr-lists","4. Attribute lists"],
  ["sp65.html","input-formats","5. Input formats"],
  ["sp65.html","sect-5-1","5.1. PCX"],
  ["sp65.html","conversions","6. Conversions"],
  ["sp65.html","sect-6-1","6.1. Lynx sprite"],
  ["sp65.html","sprite-sheets","6.2. Sprite sheets"],
  ["sp65.html","output-formats","7. Output formats"],
  ["sp65.html","sect-7-1","7.1. Binary"],
  ["sp65.html","sect-7-2","7.2. Assembler code"],
  ["sp65.html","sect-7-3","7.3. C code"],
  ["sp65.html","sect-8","8. Copyright"],
  ["sprites.html","sect-1","1. The hardware sprite model"],
  ["sprites.html","sect-2","2. The sprite control block"],
  ["sprites.html","sect-2-1","2.1. Linking and painter's order"],
  ["sprites.html","sect-2-2","2.2. The eight struct variants and the reload bits"],
  ["sprites.html","sect-3","3. Sprite types"],
  ["sprites.html","sect-4","4. The pen palette"],
  ["sprites.html","sect-5","5. Pixel and data format"],
  ["sprites.html","sect-6","6. Sizing, stretch, tilt and flip"],
  ["sprites.html","sect-6-1","6.1. Flipping about the reference point"],
  ["sprites.html","sect-6-2","6.2. Panning the display window: hoff and voff"],
  ["sprites.html","sect-7","7. Driving the engine"],
  ["sprites.html","sect-8","8. License"],
  ["sprpck.html","sect-1","1. Overview"],
  ["sprpck.html","sect-2","2. Usage"],
  ["sprpck.html","sect-2-1","2.1. Command line option overview"],
  ["sprpck.html","sect-2-2","2.2. Command line options in detail"],
  ["sprpck.html","input-formats","3. Input formats"],
  ["sprpck.html","sect-3-1","3.1. BMP"],
  ["sprpck.html","sect-3-2","3.2. SPS (ASCII)"],
  ["sprpck.html","output","4. Output"],
  ["sprpck.html","sect-4-1","4.1. Sprite data (.spr)"],
  ["sprpck.html","sect-4-2","4.2. Palette (.pal)"],
  ["sprpck.html","example","5. Worked example"],
  ["sprpck.html","vs-sp65","6. sprpck versus sp65"],
  ["sprpck.html","sect-7","7. Copyright"],
  ["using-make.html","sect-1","1. Overview"],
  ["using-make.html","sect-2","2. What is GNU Make?"],
  ["using-make.html","sect-3","3. A sample Makefile"],
  ["using-make.html","sect-3-1","3.1. Invoking the sample Makefile"],
  ["using-make.html","sect-3-2","3.2. Understanding the sample Makefile"],
  ["using-make.html","sect-3-3","3.3. Invoking the sample Makefile on Windows"],
  ["using-make.html","sect-4","4. Target-specific Variable Values"],
];
// === END SEARCH_INDEX ===

// <site-nav>: injects the top bar and marks the current page active.
(function () {
  function currentPage() {
    var p = location.pathname.split('/').pop();
    return p || 'index.html';
  }
  class SiteNav extends HTMLElement {
    connectedCallback() {
      this.innerHTML = TOPBAR_HTML;
      var page = currentPage();
      var links = this.querySelectorAll('.nav .dropdown-menu a[href]');
      for (var i = 0; i < links.length; i++) {
        var a = links[i];
        if (a.getAttribute('href') === page) {
          a.classList.add('active');
          var d = a.closest('.dropdown');
          if (d) d.classList.add('active');
        }
      }
      wireChrome();
    }
  }
  if (!customElements.get('site-nav')) customElements.define('site-nav', SiteNav);
})();

// <site-foot>: injects the shared footer line.  An optional "note" attribute
// appends an extra sentence (used by the graphics-fonts page).
(function () {
  var BASE = '<strong>lynxcc</strong> documentation \u2014 a complete Atari Lynx Game Development SDK.';
  class SiteFoot extends HTMLElement {
    connectedCallback() {
      var note = this.getAttribute('note');
      this.className = 'site-foot';
      this.innerHTML = note ? BASE + ' ' + note : BASE;
    }
  }
  if (!customElements.get('site-foot')) customElements.define('site-foot', SiteFoot);
})();

// Theme toggle + dropdown wiring.  Called after the top bar is injected.
function wireChrome() {
  function current() {
    return document.documentElement.getAttribute('data-theme') ||
      (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches
        ? 'dark' : 'light');
  }
  function apply(t) {
    document.documentElement.setAttribute('data-theme', t);
    try { localStorage.setItem('cc65-theme', t); } catch (e) {}
  }
  var btn = document.getElementById('themeToggle');
  if (btn && !btn.dataset.wired) {
    btn.dataset.wired = '1';
    btn.addEventListener('click', function () {
      apply(current() === 'dark' ? 'light' : 'dark');
    });
  }

  // Every dropdown gets click/touch/keyboard toggle (CSS still handles hover).
  var drops = document.querySelectorAll('.topbar .nav .dropdown');
  function closeAll(except) {
    drops.forEach(function (d) {
      if (d === except) return;
      d.classList.remove('open');
      var t = d.querySelector('.dropbtn');
      if (t) t.setAttribute('aria-expanded', 'false');
    });
  }
  drops.forEach(function (drop) {
    var trigger = drop.querySelector('.dropbtn');
    if (!trigger || trigger.dataset.wired) return;
    trigger.dataset.wired = '1';
    trigger.addEventListener('click', function (e) {
      e.preventDefault();
      var open = drop.classList.toggle('open');
      trigger.setAttribute('aria-expanded', open ? 'true' : 'false');
      if (open) closeAll(drop);
    });
  });
  if (!document.body.dataset.dropWired) {
    document.body.dataset.dropWired = '1';
    document.addEventListener('click', function (e) {
      if (!e.target.closest('.topbar .nav .dropdown')) closeAll(null);
    });
    document.addEventListener('keydown', function (e) {
      if (e.key === 'Escape') closeAll(null);
    });
  }

  initSearch();
}

// Quick-jump search palette.  A Cmd/Ctrl-K or "/" shortcut (and the topbar
// magnifier) opens an overlay that filters page entries (derived at runtime
// from the nav) and section headings (from the generated SEARCH_INDEX), and
// navigates to the chosen page or page#anchor.  See design/DOC_SEARCH_DESIGN.md.
function initSearch() {
  if (document.body.dataset.searchWired) return;
  document.body.dataset.searchWired = '1';

  // Page entries, built from the just-injected nav (label, description, href).
  var pages = [];
  document.querySelectorAll('.topbar .nav .dropdown-menu a[href]').forEach(function (a) {
    var spans = a.querySelectorAll('span');
    pages.push({
      type: 'page',
      href: a.getAttribute('href'),
      title: spans[0] ? spans[0].textContent.trim() : a.textContent.trim(),
      desc: spans[1] ? spans[1].textContent.trim() : ''
    });
  });
  // Map filename -> nav label, for section breadcrumbs.
  var pageName = {};
  pages.forEach(function (p) { pageName[p.href] = p.title; });

  // Section entries from the generated index: [page, id, label].
  var sections = (typeof SEARCH_INDEX !== 'undefined' ? SEARCH_INDEX : []).map(function (r) {
    return {
      type: 'section',
      href: r[0] + '#' + r[1],
      title: r[2],
      crumb: pageName[r[0]] || r[0].replace(/\.html$/, '')
    };
  });
  var ALL = pages.concat(sections);

  // --- overlay DOM (built once, lazily) ---
  var overlay, input, list, status, results = [], active = -1;
  function build() {
    overlay = document.createElement('div');
    overlay.className = 'doc-search';
    overlay.setAttribute('hidden', '');
    overlay.innerHTML =
      '<div class="ds-backdrop"></div>' +
      '<div class="ds-panel" role="dialog" aria-modal="true" aria-label="Search documentation">' +
        '<div class="ds-inputrow"><span class="ds-ic">⌕</span>' +
        '<input type="text" class="ds-input" placeholder="Search pages and sections…" ' +
        'autocomplete="off" spellcheck="false" aria-label="Search" aria-controls="dsList">' +
        '<kbd class="ds-esc">Esc</kbd></div>' +
        '<ul class="ds-list" id="dsList" role="listbox"></ul>' +
        '<div class="ds-status" aria-live="polite"></div>' +
      '</div>';
    document.body.appendChild(overlay);
    input = overlay.querySelector('.ds-input');
    list = overlay.querySelector('.ds-list');
    status = overlay.querySelector('.ds-status');
    input.addEventListener('input', function () { render(input.value); });
    input.addEventListener('keydown', onKey);
    overlay.querySelector('.ds-backdrop').addEventListener('click', close);
  }

  function score(item, q) {
    var t = item.title.toLowerCase();
    var i = t.indexOf(q);
    if (i === -1) {
      // allow matching a page's description / section breadcrumb too
      var extra = ((item.desc || '') + ' ' + (item.crumb || '')).toLowerCase();
      if (extra.indexOf(q) === -1) return -1;
      return 1;
    }
    var s = 100;
    if (i === 0) s += 50;            // prefix match ranks higher
    if (item.type === 'page') s += 30; // pages above sections
    s -= i;                          // earlier match ranks higher
    return s;
  }

  function render(q) {
    q = q.trim().toLowerCase();
    if (!q) {
      results = pages.slice(0, 30);
    } else {
      results = ALL.map(function (it) { return { it: it, s: score(it, q) }; })
        .filter(function (r) { return r.s >= 0; })
        .sort(function (a, b) { return b.s - a.s; })
        .slice(0, 40)
        .map(function (r) { return r.it; });
    }
    active = results.length ? 0 : -1;
    list.innerHTML = results.map(function (it, i) {
      var crumb = it.type === 'section'
        ? '<span class="ds-crumb">' + esc(it.crumb) + '</span>'
        : (it.desc ? '<span class="ds-crumb">' + esc(it.desc) + '</span>' : '');
      return '<li class="ds-item' + (i === active ? ' active' : '') +
        '" role="option" data-i="' + i + '">' +
        '<span class="ds-kind ds-kind-' + it.type + '">' +
        (it.type === 'page' ? 'Page' : '§') + '</span>' +
        '<span class="ds-title">' + esc(it.title) + '</span>' + crumb + '</li>';
    }).join('');
    status.textContent = results.length
      ? results.length + ' result' + (results.length === 1 ? '' : 's')
      : (q ? 'No matches' : '');
    Array.prototype.forEach.call(list.children, function (li) {
      li.addEventListener('mousemove', function () { setActive(+li.dataset.i); });
      li.addEventListener('click', function () { go(+li.dataset.i); });
    });
  }

  function esc(s) {
    return s.replace(/[&<>"]/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
    });
  }

  function setActive(i) {
    if (i < 0 || i >= results.length) return;
    var prev = list.querySelector('.ds-item.active');
    if (prev) prev.classList.remove('active');
    active = i;
    var cur = list.children[i];
    if (cur) { cur.classList.add('active'); cur.scrollIntoView({ block: 'nearest' }); }
  }

  function go(i) {
    var it = results[i];
    if (!it) return;
    close();
    window.location.href = it.href;
  }

  function onKey(e) {
    if (e.key === 'ArrowDown') { e.preventDefault(); setActive(Math.min(active + 1, results.length - 1)); }
    else if (e.key === 'ArrowUp') { e.preventDefault(); setActive(Math.max(active - 1, 0)); }
    else if (e.key === 'Enter') { e.preventDefault(); if (active >= 0) go(active); }
    else if (e.key === 'Escape') { e.preventDefault(); close(); }
  }

  function open() {
    if (!overlay) build();
    overlay.removeAttribute('hidden');
    document.body.classList.add('ds-open');
    input.value = '';
    render('');
    input.focus();
  }
  function close() {
    if (!overlay) return;
    overlay.setAttribute('hidden', '');
    document.body.classList.remove('ds-open');
  }

  var trigger = document.getElementById('docSearchBtn');
  if (trigger) trigger.addEventListener('click', open);

  document.addEventListener('keydown', function (e) {
    var openNow = overlay && !overlay.hasAttribute('hidden');
    if ((e.key === 'k' || e.key === 'K') && (e.metaKey || e.ctrlKey)) {
      e.preventDefault(); openNow ? close() : open(); return;
    }
    if (e.key === 'Escape' && openNow) { e.preventDefault(); close(); return; }
    if (e.key === '/' && !openNow) {
      var el = document.activeElement, tag = el && el.tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || (el && el.isContentEditable)) return;
      e.preventDefault(); open();
    }
  });
}

// Function reference: live filter for the left-hand function index, and
// highlight of the entry currently scrolled into view.
(function () {
  var input = document.getElementById('fnFilter');
  var index = document.getElementById('fnIndex');
  if (!input || !index) return;
  var items = Array.prototype.slice.call(index.querySelectorAll('li'));
  var empty = document.querySelector('.fn-empty');
  input.addEventListener('input', function () {
    var q = input.value.trim().toLowerCase();
    var shown = 0;
    items.forEach(function (li) {
      var hit = !q || li.getAttribute('data-name').indexOf(q) !== -1;
      li.classList.toggle('fn-hidden', !hit);
      if (hit) shown++;
    });
    if (empty) empty.style.display = shown ? 'none' : 'block';
  });

  var links = {};
  items.forEach(function (li) {
    var a = li.querySelector('a');
    if (a) links[a.getAttribute('href').slice(1)] = a;
  });
  var current = null;
  var headings = Array.prototype.slice.call(
    document.querySelectorAll('.fn-main h3[id]'));
  if (!('IntersectionObserver' in window) || !headings.length) return;
  var io = new IntersectionObserver(function (entries) {
    entries.forEach(function (en) {
      if (!en.isIntersecting) return;
      var a = links[en.target.id];
      if (!a || a === current) return;
      if (current) current.classList.remove('current');
      a.classList.add('current');
      current = a;
      a.scrollIntoView({ block: 'nearest' });
    });
  }, { rootMargin: '-60px 0px -75% 0px' });
  headings.forEach(function (h) { io.observe(h); });
})();
