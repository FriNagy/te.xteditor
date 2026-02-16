/******************************************************************************
*
*
* TE - Text Editor
*
* version.h
*   TE version information
*
* Based on Notepad2 by Florian Balmer
* Extended and maintained for TE project
*
******************************************************************************/


#define VERSION_FILEVERSION_NUM      4,2,25,0
#define VERSION_FILEVERSION_SHORT    L"4.2.25"


// BUILD_TW = UTF16 (TW.exe)
// BUILD_TE = UTF8 (TE.exe) - default

#ifdef BUILD_TW
  #define winutf16 1
#else
  // Default: BUILD_TE / UTF8
  #define winutf8 1
#endif

#ifdef winutf8
 #define nbsb 0xA0
 #define proname  "TE"
 #define Lproname L"TE"
 #define mainicon "..\\res\\tun.ico"
 #define mitunic 1
#endif

#ifdef winutf16
#define nbsb 0xA0
#define proname  "TW"
#define Lproname L"TW"
#define mitunic 1
#define mainicon "..\\res\\twn.ico"
#endif




#define VERSION_FILEVERSION_LONG     Lproname L" 4.2.25 - Scintilla 5.58"
#define VERSION_FILEDESCRIPTION      Lproname L" Text Editor"


#define VERSION_LEGALCOPYRIGHT_SHORT L"Copyright 2024"
#define VERSION_LEGALCOPYRIGHT_LONG  L"TE Project"
#define VERSION_WEBPAGEDISPLAY       L""

#define VERSION_INTERNALNAME  Lproname L".exe"

#define VERSION_ORIGINALFILENAME Lproname L".exe"

#define VERSION_AUTHORNAME           L""
