/******************************************************************************
*
* TU - Text Editor
*
* Styles.c - Scintilla Style Management
*
* Based on Notepad2 by Florian Balmer
*
******************************************************************************/
#define _WIN32_WINNT 0x501
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include "scintilla.h"
#include "scilexer.h"

// Forward declaration for CreateLexer (from Lexilla)
// Using void* instead of ILexer5* for C compatibility
#ifdef __cplusplus
extern "C"
#endif
void* __stdcall CreateLexer(const char *name);

#include "TE.h"
#include "edit.h"
#include "styles.h"
#include "dialogs.h"
#include "helpers.h"
#include "resource.h"


#define MULTI_STYLE(a,b,c,d) ((a)|(b<<8)|(c<<16)|(d<<24))

// Custom SCE_DIFF extensions (not in standard Scintilla)
#define SCE_DIFF_MY1 12
#define SCE_DIFF_MY2 13

// Custom CPS lexer (must match LexCPS.cxx)
#define SCLEX_CPS 200
#define SCE_CPS_DEFAULT 0
#define SCE_CPS_COMMENT 1            // # comment (normal mode, line start only)
#define SCE_CPS_LABEL 2              // :labelname
#define SCE_CPS_VARIABLE 3           // %var / $var assignment and reference
#define SCE_CPS_VAREXPAND_DELIM 4    // (% and ) and varname - red on yellow
#define SCE_CPS_VAREXPAND_CONTENT 5  // processor content in (%var proc) - black on yellow
#define SCE_CPS_STRING 6             // "..." / '...' and assignment values
#define SCE_CPS_OUTPUT 7             // > output text - light green
#define SCE_CPS_COMMAND 8            // Commands: goto, include, shell, !> !< etc.
#define SCE_CPS_OPERATOR 9           // =, ==, >, <, ~=
#define SCE_CPS_COLORTAG 10          // [red], [#RGB], [bold] etc. (in > output)
#define SCE_CPS_FILEOP 11            // !> !>> file/HTTP write block - gray
#define SCE_CPS_INPUT 12             // (reserved)
#define SCE_CPS_FILEREAD 13          // !< extract content
#define SCE_CPS_KEYWORD 14           // if, ifnot, then, else, undef, next, in
#define SCE_CPS_JUMP_REL 15          // +N relative jumps
#define SCE_CPS_HEADER 16            // # heading (markdown mode), ~~~ fence
#define SCE_CPS_DOCCOMMENT 17        // (reserved)

// Custom lexer for F2T (Flex2Text)
#define SCLEX_F2T 201

// F2T style definitions - based on last 2 chars of line
#define SCE_F2T_DEFAULT   0   // Default/unknown
#define SCE_F2T_NOKEND    1   // Tab + NBSpace
#define SCE_F2T_DKBLUE    2   // Tab + Space
#define SCE_F2T_LTBLUE    3   // Tab + Tab
#define SCE_F2T_OKEND     4   // NBSpace + Space
#define SCE_F2T_VARI1     5   // NBSpace + Tab
#define SCE_F2T_VARI2     6   // NBSpace + NBSpace
#define SCE_F2T_HOKEND    7   // Space + NBSpace
#define SCE_F2T_LTGRAY    8   // Space + Tab
// BIG variants (120% size) - activated by NBS as 3rd char from end
#define SCE_F2T_NOKEND_BIG  9   // NBS + Tab + NBSpace
#define SCE_F2T_DKBLUE_BIG 10   // NBS + Tab + Space
#define SCE_F2T_LTBLUE_BIG 11   // NBS + Tab + Tab
#define SCE_F2T_OKEND_BIG  12   // NBS + NBSpace + Space
#define SCE_F2T_VARI1_BIG  13   // NBS + NBSpace + Tab
#define SCE_F2T_VARI2_BIG  14   // NBS + NBSpace + NBSpace
#define SCE_F2T_HOKEND_BIG 15   // NBS + Space + NBSpace
#define SCE_F2T_LTGRAY_BIG 16   // NBS + Space + Tab

// Custom lexer for F2P (Flex2PDF template) - must match LexF2P.cxx
#define SCLEX_F2P 202
#define SCE_F2P_DEFAULT    0   // body text (printed verbatim)
#define SCE_F2P_COMMENT    1   // [=/ comment line
#define SCE_F2P_HEADER     2   // [=) head/setup line (first line)
#define SCE_F2P_SETUP      3   // [= positioning/setup
#define SCE_F2P_PDF        4   // [=pdf, metadata/bookmarks
#define SCE_F2P_BLOCKDEF   5   // [==,X, template definition
#define SCE_F2P_OBJECT     6   // [x graphic object
#define SCE_F2P_COLUMN     7   // [: / [. columns/tabs
#define SCE_F2P_CONDITION  8   // [? multi-column / space layout
#define SCE_F2P_FORMAT     9   // format shortcuts [F [f [B [S [0
#define SCE_F2P_BLOCKCALL 10   // block/template call [P [7 [Z [I
#define SCE_F2P_LINEFEED  11   // [l [l- [pp [Pp [PP feed/page break
#define SCE_F2P_VPECODE   12   // VPE inline code [ ... ] with ]
#define SCE_F2P_VARIABLE  13   // [=%, def + placeholders %a %g %%% [%x
#define SCE_F2P_RTF       14   // embedded RTF string {\rtf1 ... }
#define SCE_F2P_FLOWBLOCK 15   // \== ... /== keep-together marker
#define SCE_F2P_OPERATOR  16   // reserved (unused)

// Helper function to get lexer name from SCLEX_* constant
static const char* GetLexerNameFromID(int lexerId) {
  switch (lexerId) {
    case SCLEX_NULL: return "null";
    case SCLEX_SQL: return "sql";
    case SCLEX_BATCH: return "batch";
    case SCLEX_F2T: return "f2t"; // Custom F2T lexer
    case SCLEX_F2P: return "f2p"; // Custom F2P (Flex2PDF) lexer
    case SCLEX_XML: return "hypertext";  // XML uses hypertext lexer
    case SCLEX_JSON: return "json";
    case SCLEX_PASCAL: return "pascal";
    case SCLEX_HTML: return "hypertext";
    case SCLEX_CPS: return "cps";        // Custom CPS lexer
    case SCLEX_MARKDOWN: return "markdown";
    default: return "null";
  }
}


KEYWORDLIST KeyWords_NULL = {
"", "", "", "", "", "", "", "", "" };


EDITLEXER lexDefault = { SCLEX_NULL, 63000, L"Default Text", L"txt; text; wtx; asc; log; doc; diz; nfo", L"", &KeyWords_NULL, {
                /*  0 */ { STYLE_DEFAULT, 63100, L"Default Style", L"font:Consolas; size:11", L"" },
                /*  1 */ { STYLE_LINENUMBER, 63101, L"Margins and Line Numbers", L"size:-2; fore:#FF0000", L"" },
                /*  2 */ { STYLE_BRACELIGHT, 63102, L"Matching Braces", L"size:+1; bold; fore:#FF0000", L"" },
                /*  3 */ { STYLE_BRACEBAD, 63103, L"Matching Braces Error", L"size:+1; bold; fore:#000080", L"" },
                /*  4 */ { STYLE_CONTROLCHAR, 63104, L"Control Characters (Font)", L"size:-1", L"" },
                /*  5 */ { STYLE_INDENTGUIDE, 63105, L"Indentation Guide (Color)", L"fore:#A0A0A0", L"" },
                /*  6 */ { SCI_SETSELFORE+SCI_SETSELBACK, 63106, L"Selected Text (Colors)", L"back:#0A246A; eolfilled; alpha:95", L"" },
                /*  7 */ { SCI_SETWHITESPACEFORE+SCI_SETWHITESPACEBACK+SCI_SETWHITESPACESIZE, 63107, L"Whitespace (Colors, Size 0-5)", L"fore:#FF4000", L"" },
                /*  8 */ { SCI_SETCARETLINEBACK, 63108, L"Current Line Background (Color)", L"back:#FFFF00; alpha:50", L"" },
                /*  9 */ { SCI_SETCARETFORE+SCI_SETCARETWIDTH, 63109, L"Caret (Color, Size 1-3)", L"", L"" },
                /* 10 */ { SCI_SETEDGECOLOUR, 63110, L"Long Line Marker (Colors)", L"fore:#FFC000", L"" },
                /* 11 */ { SCI_SETEXTRAASCENT+SCI_SETEXTRADESCENT, 63111, L"Extra Line Spacing (Size)", L"size:2", L"" },

                /* 12 */ { STYLE_DEFAULT, 63112, L"2nd Default Style", L"font:Courier New; size:10", L"" },
                /* 13 */ { STYLE_LINENUMBER, 63113, L"2nd Margins and Line Numbers", L"font:Tahoma; size:-2; fore:#FF0000", L"" },
                /* 14 */ { STYLE_BRACELIGHT, 63114, L"2nd Matching Braces", L"bold; fore:#FF0000", L"" },
                /* 15 */ { STYLE_BRACEBAD, 63115, L"2nd Matching Braces Error", L"bold; fore:#000080", L"" },
                /* 16 */ { STYLE_CONTROLCHAR, 63116, L"2nd Control Characters (Font)", L"size:-1", L"" },
                /* 17 */ { STYLE_INDENTGUIDE, 63117, L"2nd Indentation Guide (Color)", L"fore:#A0A0A0", L"" },
                /* 18 */ { SCI_SETSELFORE+SCI_SETSELBACK, 63118, L"2nd Selected Text (Colors)", L"eolfilled", L"" },
                /* 19 */ { SCI_SETWHITESPACEFORE+SCI_SETWHITESPACEBACK+SCI_SETWHITESPACESIZE, 63119, L"2nd Whitespace (Colors, Size 0-5)", L"fore:#FF4000", L"" },
                /* 20 */ { SCI_SETCARETLINEBACK, 63120, L"2nd Current Line Background (Color)", L"back:#FFFF00; alpha:50", L"" },
                /* 21 */ { SCI_SETCARETFORE+SCI_SETCARETWIDTH, 63121, L"2nd Caret (Color, Size 1-3)", L"", L"" },
                /* 22 */ { SCI_SETEDGECOLOUR, 63122, L"2nd Long Line Marker (Colors)", L"fore:#FFC000", L"" },
                /* 23 */ { SCI_SETEXTRAASCENT+SCI_SETEXTRADESCENT, 63123, L"2nd Extra Line Spacing (Size)", L"", L"" },
                         { -1, 00000, L"", L"", L"" } } };



KEYWORDLIST KeyWords_XML = {
"", "", "", "", "", "", "", "", "" };


EDITLEXER lexXML = { SCLEX_XML, 63002, L"XML Document", L"xml; xsl; rss; svg; xul; xsd; xslt; axl; rdf; xaml; vcproj", L"", &KeyWords_XML, {
                     { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                     { MULTI_STYLE(SCE_H_TAG,SCE_H_TAGUNKNOWN,SCE_H_TAGEND,0), 63187, L"XML Tag", L"fore:#881280", L"" },
                     { MULTI_STYLE(SCE_H_ATTRIBUTE,SCE_H_ATTRIBUTEUNKNOWN,0,0), 63188, L"XML Attribute", L"fore:#994500", L"" },
                     { SCE_H_VALUE, 63189, L"XML Value", L"fore:#1A1AA6", L"" },
                     { MULTI_STYLE(SCE_H_DOUBLESTRING,SCE_H_SINGLESTRING,0,0), 63190, L"XML String", L"fore:#1A1AA6", L"" },
                     { SCE_H_OTHER, 63191, L"XML Other Inside Tag", L"fore:#1A1AA6", L"" },
                     { MULTI_STYLE(SCE_H_COMMENT,SCE_H_XCCOMMENT,0,0), 63192, L"XML Comment", L"fore:#646464", L"" },
                     { SCE_H_ENTITY, 63193, L"XML Entity", L"fore:#B000B0", L"" },
                     { SCE_H_DEFAULT, 63257, L"XML Element Text", L"", L"" },
                     { MULTI_STYLE(SCE_H_XMLSTART,SCE_H_XMLEND,0,0), 63145, L"XML Identifier", L"bold; fore:#881280", L"" },
                     { SCE_H_SGML_DEFAULT, 63237, L"SGML", L"fore:#881280", L"" },
                     { SCE_H_CDATA, 63147, L"CDATA", L"fore:#646464", L"" },
                     { -1, 00000, L"", L"", L"" } } };


KEYWORDLIST KeyWords_JSON = {
"false null true", "", "", "", "", "", "", "", "" };


EDITLEXER lexJSON = { SCLEX_JSON, 63020, L"JSON Document", L"json", L"", &KeyWords_JSON, {
                     { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                     { SCE_JSON_NUMBER, 63130, L"Number", L"fore:#FF0000", L"" },
                     { SCE_JSON_STRING, 63131, L"String", L"fore:#008000", L"" },
                     { SCE_JSON_PROPERTYNAME, 63194, L"Property Name", L"fore:#A52A2A", L"" },
                     { SCE_JSON_ESCAPESEQUENCE, 63195, L"Escape Sequence", L"fore:#0080FF", L"" },
                     { SCE_JSON_LINECOMMENT, 63127, L"Comment", L"fore:#646464", L"" },
                     { SCE_JSON_BLOCKCOMMENT, 63196, L"Block Comment", L"fore:#646464", L"" },
                     { SCE_JSON_OPERATOR, 63132, L"Operator", L"fore:#B000B0", L"" },
                     { SCE_JSON_KEYWORD, 63128, L"Keyword", L"bold; fore:#0A246A", L"" },
                     { SCE_JSON_ERROR, 63197, L"Error", L"fore:#FF0000; back:#FFFF00", L"" },
                     { -1, 00000, L"", L"", L"" } } };

KEYWORDLIST KeyWords_F2T = {
"", "", "", "", "", "", "", "", "" };


EDITLEXER lexF2T = { SCLEX_F2T, 63007, L"Flex2Text", L"f2u; f2t; f2w", L"", &KeyWords_F2T, {
                     { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                     { SCE_F2T_NOKEND, 63127, L"nokend (Tab+NBS)", L"fore:#FF0000; back:#FFE0E0; eolfilled", L"" },
                     { SCE_F2T_DKBLUE, 63236, L"dkblue (Tab+Spc)", L"fore:#FFFFFF; back:#334C95; eolfilled", L"" },
                     { SCE_F2T_LTBLUE, 63238, L"ltblue (Tab+Tab)", L"fore:#102864; back:#D3E8F5; eolfilled", L"" },
                     { SCE_F2T_OKEND, 63239, L"okend (NBS+Spc)", L"fore:#000000; back:#90EE90; eolfilled", L"" },
                     { SCE_F2T_VARI1, 63240, L"vari1 (NBS+Tab)", L"fore:#000000; back:#FFFF00; eolfilled", L"" },
                     { SCE_F2T_VARI2, 63241, L"vari2 (NBS+NBS)", L"fore:#FFFFFF; back:#FF8000; eolfilled", L"" },
                     { SCE_F2T_HOKEND, 63242, L"hokend (Spc+NBS)", L"fore:#FFFFFF; back:#009000; eolfilled", L"" },
                     { SCE_F2T_LTGRAY, 63232, L"ltgray (Spc+Tab)", L"fore:#000000; back:#D2D2D2; eolfilled", L"" },
                     // BIG variants (120% size, Segoe UI) - NBS as 3rd char from end
                     { SCE_F2T_NOKEND_BIG, 63250, L"nokend BIG", L"font:Segoe UI; size:+2; fore:#FF0000; back:#FFE0E0; eolfilled", L"" },
                     { SCE_F2T_DKBLUE_BIG, 63251, L"dkblue BIG", L"font:Segoe UI; size:+2; fore:#FFFFFF; back:#334C95; eolfilled", L"" },
                     { SCE_F2T_LTBLUE_BIG, 63252, L"ltblue BIG", L"font:Segoe UI; size:+2; fore:#102864; back:#D3E8F5; eolfilled", L"" },
                     { SCE_F2T_OKEND_BIG, 63253, L"okend BIG", L"font:Segoe UI; size:+2; fore:#000000; back:#90EE90; eolfilled", L"" },
                     { SCE_F2T_VARI1_BIG, 63254, L"vari1 BIG (Standard)", L"font:Segoe UI; size:+2", L"" },
                     { SCE_F2T_VARI2_BIG, 63255, L"vari2 BIG", L"font:Segoe UI; size:+2; fore:#FFFFFF; back:#FF8000; eolfilled", L"" },
                     { SCE_F2T_HOKEND_BIG, 63256, L"hokend BIG", L"font:Segoe UI; size:+2; fore:#FFFFFF; back:#009000; eolfilled", L"" },
                     { SCE_F2T_LTGRAY_BIG, 63258, L"ltgray BIG", L"font:Segoe UI; size:+2; fore:#000000; back:#D2D2D2; eolfilled", L"" },
                     { -1, 00000, L"", L"", L"" } } };

            /*   
			
			//	 { SCE_DIFF_MY1, 63243, L"myLine 1", L"fore:#000000; back:#D3E8F5; eolfilled", L"" },  // lblau
			//	 { SCE_DIFF_MY2, 63244, L"myLine 2", L"fore:#000000; back:#F4F4F4; eolfilled", L"" },  // grau
			//		 { SCE_C_WORD, 63245, L"Keyword", L"fore:#FFFFFF; back:#FF4040; eolfilled", L"" },
			//		 { SCE_C_COMMENT, 63246, L"Comment", L"fore:#000000; back:#99D7FF; eolfilled", L"" },

			
			
			
			//{ SCE_MAKE_DEFAULT, L"Default", L"", L"" },
                     { SCE_MAKE_COMMENT, 63127, L"Comment", L"fore:#008000", L"" },
                     { MULTI_STYLE(SCE_MAKE_IDENTIFIER,SCE_MAKE_IDEOL,0,0), 63129, L"Identifier", L"fore:#003CE6", L"" },
                     { SCE_MAKE_OPERATOR, 63132, L"Operator", L"", L"" },
                     { SCE_MAKE_TARGET, 63204, L"Target", L"fore:#003CE6; back:#FFC000", L"" },
                     { SCE_MAKE_PREPROCESSOR, 63133, L"Preprocessor", L"fore:#FF8000", L"" },
                     { -1, 00000, L"", L"", L"" } } };
          */


/* KEYWORDLIST KeyWords_VBS = {
	"alias and as attribute begin boolean byref byte byval call case class compare const continue "
	"currency date declare dim do double each else elseif empty end enum eqv erase error event exit "
	"explicit false for friend function get global gosub goto if imp implement in integer is let lib "
	"load long loop lset me mid mod module new next not nothing null object on option optional or "
	"preserve private property public raiseevent redim rem resume return rset select set single "
	"static stop string sub then to true type unload until variant wend while with withevents xor",
	"", "", "", "", "", "", "", ""
};


EDITLEXER lexVBS = { SCLEX_VBSCRIPT, 63008, L"VBScript", L"vbs; dsm", L"", &KeyWords_VBS, {
	{ STYLE_DEFAULT, 63126, L"Default", L"", L"" },
	//{ SCE_B_DEFAULT, L"Default", L"", L"" },
	{ SCE_B_COMMENT, 63127, L"Comment", L"fore:#808080", L"" },
	{ SCE_B_KEYWORD, 63128, L"Keyword", L"bold; fore:#B000B0", L"" },
	{ SCE_B_IDENTIFIER, 63129, L"Identifier", L"", L"" },
	{ MULTI_STYLE(SCE_B_STRING, SCE_B_STRINGEOL, 0, 0), 63131, L"String", L"fore:#008000", L"" },
	{ SCE_B_NUMBER, 63130, L"Number", L"fore:#FF0000", L"" },
	{ SCE_B_OPERATOR, 63132, L"Operator", L"", L"" },
	//{ SCE_B_PREPROCESSOR, 63133, L"Preprocessor", L"fore:#FF9C00", L"" },
	//{ SCE_B_CONSTANT, L"Constant", L"", L"" },
	//{ SCE_B_DATE, L"Date", L"", L"" },
	//{ SCE_B_KEYWORD2, L"Keyword 2", L"", L"" },
	//{ SCE_B_KEYWORD3, L"Keyword 3", L"", L"" },
	//{ SCE_B_KEYWORD4, L"Keyword 4", L"", L"" },
	//{ SCE_B_ASM, L"Inline Asm", L"fore:#FF8000", L"" },
	{ -1, 00000, L"", L"", L"" }
}
};
*/


#ifdef BUILD_TE
KEYWORDLIST KeyWords_CPS = {
// Commands (line start): goto, gosub, include, shell, run, runw, pause, pause$, setlog, exit, return, call, extract, pmdump, sleep
// IF keywords: if, ifnot, then, else, undef, next, in
"if ifnot goto gosub include shell run runw pause pause$ setlog exit return call extract pmdump sleep then else undef next in",
"", "", "", "", "", "", "", "" };


EDITLEXER lexCPS =    { SCLEX_CPS, 63015, L"Curl Program Script", L"cps; cpu; cpy; cpx", L"", &KeyWords_CPS, {
                      { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                      { SCE_CPS_COMMENT, 63127, L"Comment", L"fore:#008000; italic", L"" },
                      { SCE_CPS_LABEL, 63235, L"Label", L"fore:#00008B; back:#E3ECFF; bold", L"" },
                      { SCE_CPS_VARIABLE, 63129, L"Variable Assignment", L"fore:#C80000; bold", L"" },
                      { SCE_CPS_VAREXPAND_DELIM, 63249, L"VarExpand Delim", L"fore:#FF0000; back:#FFFF00", L"" },
                      { SCE_CPS_VAREXPAND_CONTENT, 63252, L"VarExpand Content", L"fore:#000000; back:#FFFF00", L"" },
                      { SCE_CPS_STRING, 63131, L"String", L"fore:#A31515", L"" },
                      { SCE_CPS_OUTPUT, 63250, L"Output Text", L"fore:#000000; back:#E0F7FA; eolfilled", L"" },
                      { SCE_CPS_COMMAND, 63128, L"Command", L"fore:#0000FF; bold", L"" },
                      { SCE_CPS_OPERATOR, 63132, L"Operator", L"fore:#000000", L"" },
                      { SCE_CPS_COLORTAG, 63251, L"Color Tag", L"fore:#808080; back:#E0F7FA", L"" },
                      { SCE_CPS_FILEOP, 63253, L"File Write Block", L"fore:#000000; back:#F0F0F0; eolfilled", L"" },
                      { SCE_CPS_INPUT, 63254, L"Input", L"fore:#795E26", L"" },
                      { SCE_CPS_FILEREAD, 63255, L"File Read", L"fore:#000000; back:#FDDCB2; eolfilled", L"" },
                      { SCE_CPS_KEYWORD, 63256, L"IF Keyword", L"fore:#0000FF; bold", L"" },
                      { SCE_CPS_JUMP_REL, 63257, L"Relative Jump", L"fore:#0000C0; bold", L"" },
                      { SCE_CPS_HEADER, 63258, L"Header", L"fore:#800080; bold", L"" },
                      { SCE_CPS_DOCCOMMENT, 63259, L"Doc Comment", L"fore:#606060; back:#F5F5F5; eolfilled", L"" },
                      { -1, 00000, L"", L"", L"" } } };
#endif


// F2P (Flex2PDF template) - rule-based lexer, empty keyword list
KEYWORDLIST KeyWords_F2P = {
"", "", "", "", "", "", "", "", "" };

EDITLEXER lexF2P = { SCLEX_F2P, 63017, L"Flex2PDF Template", L"f2p", L"", &KeyWords_F2P, {
                     { STYLE_DEFAULT,     63126, L"Default",            L"",                                 L"" },
                     { SCE_F2P_COMMENT,   63127, L"Comment",            L"fore:#008000; italic",             L"" },
                     { SCE_F2P_HEADER,    63258, L"Header [=)",         L"fore:#800080; bold",               L"" },
                     { SCE_F2P_SETUP,     63128, L"Setup [=",           L"fore:#0000C0",                     L"" },
                     { SCE_F2P_PDF,       63015, L"PDF Meta [=pdf",     L"fore:#008080; bold",               L"" },
                     { SCE_F2P_BLOCKDEF,  63235, L"Template Def [==",   L"fore:#8B4513; back:#F4ECE3; bold",  L"" },
                     { SCE_F2P_OBJECT,    63129, L"Object [x",          L"fore:#00008B; bold",               L"" },
                     { SCE_F2P_COLUMN,    63131, L"Column [: [.",       L"fore:#C86400",                     L"" },
                     { SCE_F2P_CONDITION, 63256, L"Condition [?",       L"fore:#AA00AA; bold",               L"" },
                     { SCE_F2P_FORMAT,    63132, L"Format [F [0",       L"fore:#C80000; bold",               L"" },
                     { SCE_F2P_BLOCKCALL, 63257, L"Block Call [P [7",   L"fore:#6000C0; bold",               L"" },
                     { SCE_F2P_LINEFEED,  63250, L"Linefeed/Break",     L"fore:#909090",                     L"" },
                     { SCE_F2P_VPECODE,   63251, L"VPE Inline [ ]",     L"fore:#707070; back:#F0F4F8",       L"" },
                     { SCE_F2P_VARIABLE,  63249, L"Variable / %ph",     L"fore:#FF0000; back:#FFFF00",       L"" },
                     { SCE_F2P_RTF,       63259, L"RTF Block",          L"fore:#606060; back:#F5F5F5",       L"" },
                     { SCE_F2P_FLOWBLOCK, 63236, L"Flow \\== /==",      L"fore:#800080; bold",               L"" },
                     { -1, 00000, L"", L"", L"" } } };


KEYWORDLIST KeyWords_BAT = {
"break call cd chcp chdir choice cls color com con copy country date defined del dir "
"disabledelayedexpansion disableextensions do doskey echo else enabledelayedexpansion "
"enableextensions endlocal equ erase errorlevel exist exit for geq goto gtr if in leq "
"loadfix loadhigh lpt lss md mkdir more move neq not nul off on path pause popd print "
"prompt pushd rd rem ren rename rmdir set setlocal shift time title tree type ver verify",
"", "", "", "", "", "", "", "" };


EDITLEXER lexBAT = { SCLEX_BATCH, 63016, L"Batch Files", L"bat; cmd", L"", &KeyWords_BAT, {
                     { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                     //{ SCE_BAT_DEFAULT, L"Default", L"", L"" },
                     { SCE_BAT_COMMENT, 63127, L"Comment", L"fore:#008000", L"" },
                     { SCE_BAT_WORD, 63128, L"Keyword", L"bold; fore:#0A246A", L"" },
                     { SCE_BAT_IDENTIFIER, 63129, L"Identifier", L"fore:#003CE6; back:#FFF1A8", L"" },
                     { SCE_BAT_OPERATOR, 63132, L"Operator", L"", L"" },
                     { MULTI_STYLE(SCE_BAT_COMMAND,SCE_BAT_HIDE,0,0), 63236, L"Command", L"bold", L"" },
                     { SCE_BAT_LABEL, 63235, L"Label", L"fore:#C80000; back:#F4F4F4; eolfilled", L"" },
                     { -1, 00000, L"", L"", L"" } } };


KEYWORDLIST KeyWords_SQL = {
"abort accessible action add after all alter analyze and as asc asensitive attach autoincrement "
"before begin between bigint binary bit blob both by call cascade case cast change char character "
"check collate column commit condition conflict constraint continue convert create cross current_date "
"current_time current_timestamp current_user cursor database date day_hour "
"day_minute day_second dec decimal declare default deferrable deferred delayed delete desc describe "
"detach deterministic distinct distinctrow div double drop dual each else elseif enclosed end enum "
"escape escaped except exclusive exists exit explain fail false fetch float float4 float8 for force "
"foreign from full fulltext glob grant group having "
"hour_second if ignore immediate in index infile initially inner inout insensitive insert instead int "
"int1 int2 int3 int4 int8 integer intersect interval into is isnull iterate join key keys kill "
"leading leave left like limit linear lines load localtime localtimestamp lock long longblob longtext "
"loop low_priority  match mediumblob mediumint mediumtext middleint "
"mod modifies natural no no_write_to_binlog not notnull null numeric "
"of offset on optimize option optionally or order out outer outfile plan pragma precision primary "
"procedure purge query raise range read read_only read_write reads real references regexp reindex "
"release rename repeat replace require restrict return revoke right rlike rollback row rowid schema "
"schemas second_microsecond select sensitive separator set show smallint spatial specific sql "
"sql_big_result sql_calc_found_rows sql_small_result sqlexception sqlstate sqlwarning ssl starting "
"straight_join table temp temporary terminated text then time timestamp tinyblob tinyint tinytext to "
"trailing transaction trigger true undo union unique unlock unsigned update usage use using utc_date "
"utc_time utc_timestamp vacuum values varbinary varchar varcharacter varying view virtual when where "
"while with write xor year_month zerofill",
"", "", "", "", "", "", "", "" };


EDITLEXER lexSQL = { SCLEX_SQL, 63018, L"SQL Query", L"sql", L"", &KeyWords_SQL, {
                     { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                     //{ SCE_SQL_DEFAULT, L"Default", L"", L"" },
                     { SCE_SQL_COMMENT, 63127, L"Comment", L"fore:#505050", L"" },
                     { SCE_SQL_WORD, 63128, L"Keyword", L"bold; fore:#800080", L"" },
                     { MULTI_STYLE(SCE_SQL_STRING,SCE_SQL_CHARACTER,0,0), 63131, L"String", L"fore:#008000; back:#FFF1A8", L"" },
                     { SCE_SQL_IDENTIFIER, 63129, L"Identifier", L"fore:#800080", L"" },
                     { SCE_SQL_QUOTEDIDENTIFIER, 63243, L"Quoted Identifier", L"fore:#800080; back:#FFCCFF", L"" },
                     { SCE_SQL_NUMBER, 63130, L"Number", L"fore:#FF0000", L"" },
                     { SCE_SQL_OPERATOR, 63132, L"Operator", L"bold; fore:#800080", L"" },
                     { -1, 00000, L"", L"", L"" } } };


#ifdef BUILD_TE
KEYWORDLIST KeyWords_Markdown = {
"", "", "", "", "", "", "", "", "" };


EDITLEXER lexMarkdown = { SCLEX_MARKDOWN, 63021, L"Markdown", L"md; markdown; mdown; mkdn; mkd", L"", &KeyWords_Markdown, {
                     { STYLE_DEFAULT, 63126, L"Default", L"", L"" },
                     { SCE_MARKDOWN_LINE_BEGIN, 63270, L"Line Begin", L"", L"" },
                     { MULTI_STYLE(SCE_MARKDOWN_STRONG1,SCE_MARKDOWN_STRONG2,0,0), 63271, L"Strong", L"bold", L"" },
                     { MULTI_STYLE(SCE_MARKDOWN_EM1,SCE_MARKDOWN_EM2,0,0), 63272, L"Emphasis", L"italic", L"" },
                     { SCE_MARKDOWN_HEADER1, 63273, L"Header 1", L"bold; fore:#0000A0; size:+4", L"" },
                     { SCE_MARKDOWN_HEADER2, 63274, L"Header 2", L"bold; fore:#0000A0; size:+3", L"" },
                     { SCE_MARKDOWN_HEADER3, 63275, L"Header 3", L"bold; fore:#0000A0; size:+2", L"" },
                     { SCE_MARKDOWN_HEADER4, 63276, L"Header 4", L"bold; fore:#0000A0; size:+1", L"" },
                     { SCE_MARKDOWN_HEADER5, 63277, L"Header 5", L"bold; fore:#0000A0", L"" },
                     { SCE_MARKDOWN_HEADER6, 63278, L"Header 6", L"bold; fore:#0000A0", L"" },
                     { SCE_MARKDOWN_PRECHAR, 63279, L"Preformatted Char", L"fore:#606060", L"" },
                     { SCE_MARKDOWN_ULIST_ITEM, 63280, L"Unordered List", L"fore:#A00000", L"" },
                     { SCE_MARKDOWN_OLIST_ITEM, 63281, L"Ordered List", L"fore:#A00000", L"" },
                     { SCE_MARKDOWN_BLOCKQUOTE, 63282, L"Block Quote", L"fore:#006000; italic", L"" },
                     { SCE_MARKDOWN_STRIKEOUT, 63283, L"Strikeout", L"fore:#808080", L"" },
                     { SCE_MARKDOWN_HRULE, 63284, L"Horizontal Rule", L"fore:#FF8000; bold", L"" },
                     { SCE_MARKDOWN_LINK, 63285, L"Link", L"fore:#0000FF; underline", L"" },
                     { MULTI_STYLE(SCE_MARKDOWN_CODE,SCE_MARKDOWN_CODE2,0,0), 63286, L"Inline Code", L"fore:#C00000; back:#F0F0F0", L"" },
                     { SCE_MARKDOWN_CODEBK, 63287, L"Code Block", L"fore:#C00000; back:#F0F0F0; eolfilled", L"" },
                     { -1, 00000, L"", L"", L"" } } };
#endif





// This array holds all the lexers...
PEDITLEXER pLexArray[NUMLEXERS] =
{
  &lexDefault,
  &lexXML,
  &lexJSON,
  &lexF2T,
  &lexF2P,
  &lexSQL,
  &lexBAT,
#ifdef BUILD_TE
  &lexCPS,
  &lexMarkdown
#endif
};


// Currently used lexer
PEDITLEXER pLexCurrent = &lexDefault;
COLORREF crCustom[16];
BOOL bUse2ndDefaultStyle;
BOOL fStylesModified = FALSE;
BOOL fWarnedNoIniFile = FALSE;
BOOL fIsConsolasAvailable = FALSE;
int iBaseFontSize = 10;
int iDefaultLexer;
BOOL bAutoSelect;
int cxStyleSelectDlg;
int cyStyleSelectDlg;
extern int  iDefaultCodePage;
extern int  iDefaultCharSet;
extern BOOL bHiliteCurrentLine;


//=============================================================================
//
//  Style_Load()
//
void Style_Load()
{
  int i,iLexer;
  WCHAR tch[32];
  WCHAR *pIniSection = LocalAlloc(LPTR,sizeof(WCHAR)*32*1024);
  int   cchIniSection = (int)LocalSize(pIniSection)/sizeof(WCHAR);

  // Custom colors
  crCustom [0] = RGB(0x00,0x00,0x00);
  crCustom [1] = RGB(0x0A,0x24,0x6A);
  crCustom [2] = RGB(0x3A,0x6E,0xA5);
  crCustom [3] = RGB(0x00,0x3C,0xE6);
  crCustom [4] = RGB(0x00,0x66,0x33);
  crCustom [5] = RGB(0x60,0x80,0x20);
  crCustom [6] = RGB(0x64,0x80,0x00);
  crCustom [7] = RGB(0xA4,0x60,0x00);
  crCustom [8] = RGB(0xFF,0xFF,0xFF);
  crCustom [9] = RGB(0xFF,0xFF,0xE2);
  crCustom[10] = RGB(0xFF,0xF1,0xA8);
  crCustom[11] = RGB(0xFF,0xC0,0x00);
  crCustom[12] = RGB(0xFF,0x40,0x00);
  crCustom[13] = RGB(0xC8,0x00,0x00);
  crCustom[14] = RGB(0xB0,0x00,0xB0);
  crCustom[15] = RGB(0xB2,0x8B,0x40);

  LoadIniSection(L"Custom Colors",pIniSection,cchIniSection);
  for (i = 0; i < 16; i++) {
    int itok;
    int irgb;
    WCHAR wch[32];
    wsprintf(tch,L"%02i",i+1);
    if (IniSectionGetString(pIniSection,tch,L"",wch,COUNTOF(wch))) {
      if (wch[0] == L'#') {
        itok = swscanf(CharNext(wch),L"%x",&irgb);
        if (itok == 1)
          crCustom[i] = RGB((irgb&0xFF0000) >> 16,(irgb&0xFF00) >> 8,irgb&0xFF);
      }
    }
  }

  LoadIniSection(L"Styles",pIniSection,cchIniSection);
  // 2nd default
  bUse2ndDefaultStyle = (IniSectionGetInt(pIniSection,L"Use2ndDefaultStyle",0)) ? 1 : 0;

  // default scheme
  iDefaultLexer = IniSectionGetInt(pIniSection,L"DefaultScheme",0);
  iDefaultLexer = min(max(iDefaultLexer,0),NUMLEXERS-1);

  // auto select
  bAutoSelect = (IniSectionGetInt(pIniSection,L"AutoSelect",1)) ? 1 : 0;

  // scheme select dlg dimensions
  cxStyleSelectDlg = IniSectionGetInt(pIniSection,L"SelectDlgSizeX",304);
  cxStyleSelectDlg = max(cxStyleSelectDlg,0);

  cyStyleSelectDlg = IniSectionGetInt(pIniSection,L"SelectDlgSizeY",0);
  cyStyleSelectDlg = max(cyStyleSelectDlg,324);

  for (iLexer = 0; iLexer < NUMLEXERS; iLexer++) {
    LoadIniSection(pLexArray[iLexer]->pszName,pIniSection,cchIniSection);
    if (!IniSectionGetString(pIniSection,L"FileNameExtensions",pLexArray[iLexer]->pszDefExt,
          pLexArray[iLexer]->szExtensions,COUNTOF(pLexArray[iLexer]->szExtensions)))
      lstrcpyn(pLexArray[iLexer]->szExtensions,pLexArray[iLexer]->pszDefExt,
        COUNTOF(pLexArray[iLexer]->szExtensions));
    i = 0;
    while (pLexArray[iLexer]->Styles[i].iStyle != -1) {
      IniSectionGetString(pIniSection,pLexArray[iLexer]->Styles[i].pszName,
        pLexArray[iLexer]->Styles[i].pszDefault,
        pLexArray[iLexer]->Styles[i].szValue,
        COUNTOF(pLexArray[iLexer]->Styles[i].szValue));
      i++;
    }
  }
  LocalFree(pIniSection);
}


//=============================================================================
//
//  Style_Save()
//
void Style_Save()
{
  int i,iLexer;
  WCHAR tch[32];
  WCHAR *pIniSection = LocalAlloc(LPTR,sizeof(WCHAR)*32*1024);
  int   cchIniSection = (int)LocalSize(pIniSection)/sizeof(WCHAR);

  // Custom colors
  for (i = 0; i < 16; i++) {
    WCHAR wch[32];
    wsprintf(tch,L"%02i",i+1);
    wsprintf(wch,L"#%02X%02X%02X",
      (int)GetRValue(crCustom[i]),(int)GetGValue(crCustom[i]),(int)GetBValue(crCustom[i]));
    IniSectionSetString(pIniSection,tch,wch);
  }
  SaveIniSection(L"Custom Colors",pIniSection);
  ZeroMemory(pIniSection,cchIniSection);

  // auto select
  IniSectionSetInt(pIniSection,L"Use2ndDefaultStyle",bUse2ndDefaultStyle);

  // default scheme
  IniSectionSetInt(pIniSection,L"DefaultScheme",iDefaultLexer);

  // auto select
  IniSectionSetInt(pIniSection,L"AutoSelect",bAutoSelect);

  // scheme select dlg dimensions
  IniSectionSetInt(pIniSection,L"SelectDlgSizeX",cxStyleSelectDlg);
  IniSectionSetInt(pIniSection,L"SelectDlgSizeY",cyStyleSelectDlg);

  SaveIniSection(L"Styles",pIniSection);

  if (!fStylesModified) {
    LocalFree(pIniSection);
    return;
  }

  ZeroMemory(pIniSection,cchIniSection);
  for (iLexer = 0; iLexer < NUMLEXERS; iLexer++) {
    IniSectionSetString(pIniSection,L"FileNameExtensions",pLexArray[iLexer]->szExtensions);
    i = 0;
    while (pLexArray[iLexer]->Styles[i].iStyle != -1) {
      IniSectionSetString(pIniSection,pLexArray[iLexer]->Styles[i].pszName,pLexArray[iLexer]->Styles[i].szValue);
      i++;
    }
    SaveIniSection(pLexArray[iLexer]->pszName,pIniSection);
    ZeroMemory(pIniSection,cchIniSection);
  }
  LocalFree(pIniSection);
}


//=============================================================================
//
//  Style_Import()
//
BOOL Style_Import(HWND hwnd)
{
  WCHAR szFile[MAX_PATH * 2] = L"";
  WCHAR szFilter[256];
  OPENFILENAME ofn;

  ZeroMemory(&ofn,sizeof(OPENFILENAME));
  GetString(IDS_FILTER_INI,szFilter,COUNTOF(szFilter));
  PrepareFilterStr(szFilter);

  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = szFilter;
  ofn.lpstrFile = szFile;
  ofn.lpstrDefExt = L"ini";
  ofn.nMaxFile = COUNTOF(szFile);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
            | OFN_PATHMUSTEXIST | OFN_SHAREAWARE /*| OFN_NODEREFERENCELINKS*/;

  if (GetOpenFileName(&ofn)) {

    int i,iLexer;
    WCHAR *pIniSection = LocalAlloc(LPTR,sizeof(WCHAR)*32*1024);
    int   cchIniSection = (int)LocalSize(pIniSection)/sizeof(WCHAR);

    for (iLexer = 0; iLexer < NUMLEXERS; iLexer++) {
      if (GetPrivateProfileSection(pLexArray[iLexer]->pszName,pIniSection,cchIniSection,szFile)) {
        if (!IniSectionGetString(pIniSection,L"FileNameExtensions",pLexArray[iLexer]->pszDefExt,
              pLexArray[iLexer]->szExtensions,COUNTOF(pLexArray[iLexer]->szExtensions)))
          lstrcpyn(pLexArray[iLexer]->szExtensions,pLexArray[iLexer]->pszDefExt,
            COUNTOF(pLexArray[iLexer]->szExtensions));
        i = 0;
        while (pLexArray[iLexer]->Styles[i].iStyle != -1) {
          IniSectionGetString(pIniSection,pLexArray[iLexer]->Styles[i].pszName,
            pLexArray[iLexer]->Styles[i].pszDefault,
            pLexArray[iLexer]->Styles[i].szValue,
            COUNTOF(pLexArray[iLexer]->Styles[i].szValue));
          i++;
        }
      }
    }
    LocalFree(pIniSection);
    return(TRUE);
  }
  return(FALSE);
}

//=============================================================================
//
//  Style_Export()
//
BOOL Style_Export(HWND hwnd)
{
  WCHAR szFile[MAX_PATH * 2] = L"";
  WCHAR szFilter[256];
  OPENFILENAME ofn;
  DWORD dwError = ERROR_SUCCESS;

  ZeroMemory(&ofn,sizeof(OPENFILENAME));
  GetString(IDS_FILTER_INI,szFilter,COUNTOF(szFilter));
  PrepareFilterStr(szFilter);

  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = szFilter;
  ofn.lpstrFile = szFile;
  ofn.lpstrDefExt = L"ini";
  ofn.nMaxFile = COUNTOF(szFile);
  ofn.Flags = /*OFN_FILEMUSTEXIST |*/ OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
            | OFN_PATHMUSTEXIST | OFN_SHAREAWARE /*| OFN_NODEREFERENCELINKS*/ | OFN_OVERWRITEPROMPT;

  if (GetSaveFileName(&ofn)) {

    int i,iLexer;
    WCHAR *pIniSection = LocalAlloc(LPTR,sizeof(WCHAR)*32*1024);
    int   cchIniSection = (int)LocalSize(pIniSection)/sizeof(WCHAR);

    for (iLexer = 0; iLexer < NUMLEXERS; iLexer++) {
      IniSectionSetString(pIniSection,L"FileNameExtensions",pLexArray[iLexer]->szExtensions);
      i = 0;
      while (pLexArray[iLexer]->Styles[i].iStyle != -1) {
        IniSectionSetString(pIniSection,pLexArray[iLexer]->Styles[i].pszName,pLexArray[iLexer]->Styles[i].szValue);
        i++;
      }
      if (!WritePrivateProfileSection(pLexArray[iLexer]->pszName,pIniSection,szFile))
        dwError = GetLastError();
      ZeroMemory(pIniSection,cchIniSection);
    }
    LocalFree(pIniSection);

    if (dwError != ERROR_SUCCESS) {
      MsgBox(MBINFO,IDS_EXPORT_FAIL,szFile);
    }
    return(TRUE);
  }
  return(FALSE);
}


//=============================================================================
//
//  Style_SetLexer()
//
void Style_SetLexer(HWND hwnd,PEDITLEXER pLexNew)
{
  int i;
  //WCHAR *p;
  int rgb;
  int iValue;
  int iIdx;
  WCHAR wchCaretStyle[64] = L"";

  // Select default if NULL is specified
  if (!pLexNew)
    pLexNew = pLexArray[iDefaultLexer];

  // Lexer - use new Scintilla 5.x API with CreateLexer
  {
    const char *lexerName = GetLexerNameFromID(pLexNew->iLexer);
    void *pLexer = CreateLexer(lexerName);
    SendMessage(hwnd, SCI_SETILEXER, 0, (LPARAM)pLexer);
  }

  if (pLexNew->iLexer == SCLEX_XML)
    SendMessage(hwnd,SCI_SETPROPERTY,(WPARAM)"lexer.xml.allow.scripts",(LPARAM)"1");
  if (pLexNew->iLexer == SCLEX_PASCAL)
    SendMessage(hwnd,SCI_SETPROPERTY,(WPARAM)"lexer.pascal.smart.highlighting",(LPARAM)"1");
  else if (pLexNew->iLexer == SCLEX_SQL) {
    SendMessage(hwnd,SCI_SETPROPERTY,(WPARAM)"sql.backslash.escapes",(LPARAM)"1");
    SendMessage(hwnd,SCI_SETPROPERTY,(WPARAM)"lexer.sql.backticks.identifier",(LPARAM)"1");
    SendMessage(hwnd,SCI_SETPROPERTY,(WPARAM)"lexer.sql.numbersign.comment",(LPARAM)"1");
  }

  // Add KeyWord Lists
  for (i = 0; i < 9; i++)
    SendMessage(hwnd,SCI_SETKEYWORDS,i,(LPARAM)pLexNew->pKeyWords->pszKeyWords[i]);

  // Use 2nd default style
  iIdx = (bUse2ndDefaultStyle) ? 12 : 0;

  // Font quality setup, check availability of Consolas
  Style_SetFontQuality(hwnd,lexDefault.Styles[0+iIdx].szValue);
  fIsConsolasAvailable = IsFontAvailable(L"Consolas");

  // Clear
  SendMessage(hwnd,SCI_CLEARDOCUMENTSTYLE,0,0);

  // Default Values are always set
  SendMessage(hwnd,SCI_STYLERESETDEFAULT,0,0);
  SendMessage(hwnd,SCI_STYLESETCHARACTERSET,STYLE_DEFAULT,(LPARAM)DEFAULT_CHARSET);
  iBaseFontSize = 10;
  Style_SetStyles(hwnd,lexDefault.Styles[0+iIdx].iStyle,lexDefault.Styles[0+iIdx].szValue);  // default
  Style_StrGetSize(lexDefault.Styles[0+iIdx].szValue,&iBaseFontSize);                        // base size

  // Auto-select codepage according to charset
  //Style_SetACPfromCharSet(hwnd);

  if (!Style_StrGetColor(TRUE,lexDefault.Styles[0+iIdx].szValue,&iValue))
    SendMessage(hwnd,SCI_STYLESETFORE,STYLE_DEFAULT,(LPARAM)GetSysColor(COLOR_WINDOWTEXT));   // default text color
  if (!Style_StrGetColor(FALSE,lexDefault.Styles[0+iIdx].szValue,&iValue))
    SendMessage(hwnd,SCI_STYLESETBACK,STYLE_DEFAULT,(LPARAM)GetSysColor(COLOR_WINDOW));       // default window color

  if (pLexNew->iLexer != SCLEX_NULL)
    Style_SetStyles(hwnd,pLexNew->Styles[0].iStyle,pLexNew->Styles[0].szValue); // lexer default
  SendMessage(hwnd,SCI_STYLECLEARALL,0,0);

  Style_SetStyles(hwnd,lexDefault.Styles[1+iIdx].iStyle,lexDefault.Styles[1+iIdx].szValue); // linenumber
  Style_SetStyles(hwnd,lexDefault.Styles[2+iIdx].iStyle,lexDefault.Styles[2+iIdx].szValue); // brace light
  Style_SetStyles(hwnd,lexDefault.Styles[3+iIdx].iStyle,lexDefault.Styles[3+iIdx].szValue); // brace bad
  Style_SetStyles(hwnd,lexDefault.Styles[4+iIdx].iStyle,lexDefault.Styles[4+iIdx].szValue); // control char
  Style_SetStyles(hwnd,lexDefault.Styles[5+iIdx].iStyle,lexDefault.Styles[5+iIdx].szValue); // indent guide

  // More default values...
  if (Style_StrGetColor(TRUE,lexDefault.Styles[6+iIdx].szValue,&rgb)) { // selection fore
    SendMessage(hwnd,SCI_SETSELFORE,TRUE,rgb);
    SendMessage(hwnd,SCI_SETADDITIONALSELFORE,rgb,0);
  }
  else {
    SendMessage(hwnd,SCI_SETSELFORE,0,0);
    SendMessage(hwnd,SCI_SETADDITIONALSELFORE,0,0);
  }

  if (Style_StrGetColor(FALSE,lexDefault.Styles[6+iIdx].szValue,&iValue)) { // selection back
    SendMessage(hwnd,SCI_SETSELBACK,TRUE,iValue);
    SendMessage(hwnd,SCI_SETADDITIONALSELBACK,iValue,0);
  }
  else {
    SendMessage(hwnd,SCI_SETSELBACK,TRUE,RGB(0xC0,0xC0,0xC0)); // use a default value...
    SendMessage(hwnd,SCI_SETADDITIONALSELBACK,RGB(0xC0,0xC0,0xC0),0);
  }

  if (Style_StrGetAlpha(lexDefault.Styles[6+iIdx].szValue,&iValue)) { // selection alpha
    SendMessage(hwnd,SCI_SETSELALPHA,iValue,0);
    SendMessage(hwnd,SCI_SETADDITIONALSELALPHA,iValue,0);
  }
  else {
    SendMessage(hwnd,SCI_SETSELALPHA,SC_ALPHA_NOALPHA,0);
    SendMessage(hwnd,SCI_SETADDITIONALSELALPHA,SC_ALPHA_NOALPHA,0);
  }

  if (StrStrI(lexDefault.Styles[6+iIdx].szValue,L"eolfilled")) // selection eolfilled
    SendMessage(hwnd,SCI_SETSELEOLFILLED,1,0);
  else
    SendMessage(hwnd,SCI_SETSELEOLFILLED,0,0);

  if (Style_StrGetColor(TRUE,lexDefault.Styles[7+iIdx].szValue,&rgb)) // whitespace fore
    SendMessage(hwnd,SCI_SETWHITESPACEFORE,TRUE,rgb);
  else
    SendMessage(hwnd,SCI_SETWHITESPACEFORE,0,0);

  if (Style_StrGetColor(FALSE,lexDefault.Styles[7+iIdx].szValue,&rgb)) // whitespace back
    SendMessage(hwnd,SCI_SETWHITESPACEBACK,TRUE,rgb);
  else
    SendMessage(hwnd,SCI_SETWHITESPACEBACK,0,0);    // use a default value...

  // whitespace dot size
  iValue = 1;
  if (Style_StrGetSize(lexDefault.Styles[7+iIdx].szValue,&iValue)) {

    WCHAR tch[32];
    WCHAR wchStyle[COUNTOF(lexDefault.Styles[0].szValue)];
    lstrcpyn(wchStyle,lexDefault.Styles[7+iIdx].szValue,COUNTOF(lexDefault.Styles[0].szValue));

    iValue = max(min(iValue,5),0);
    wsprintf(lexDefault.Styles[7+iIdx].szValue,L"size:%i",iValue);

    if (Style_StrGetColor(TRUE,wchStyle,&rgb)) {
      wsprintf(tch,L"; fore:#%02X%02X%02X",
        (int)GetRValue(rgb),
        (int)GetGValue(rgb),
        (int)GetBValue(rgb));
      lstrcat(lexDefault.Styles[7+iIdx].szValue,tch);
    }

    if (Style_StrGetColor(FALSE,wchStyle,&rgb)) {
      wsprintf(tch,L"; back:#%02X%02X%02X",
        (int)GetRValue(rgb),
        (int)GetGValue(rgb),
        (int)GetBValue(rgb));
      lstrcat(lexDefault.Styles[7+iIdx].szValue,tch);
    }
  }
  SendMessage(hwnd,SCI_SETWHITESPACESIZE,iValue,0);

  if (bHiliteCurrentLine) {

    if (Style_StrGetColor(FALSE,lexDefault.Styles[8+iIdx].szValue,&rgb)) // caret line back
    {
      SendMessage(hwnd,SCI_SETCARETLINEVISIBLE,TRUE,0);
      SendMessage(hwnd,SCI_SETCARETLINEBACK,rgb,0);

      if (Style_StrGetAlpha(lexDefault.Styles[8+iIdx].szValue,&iValue))
        SendMessage(hwnd,SCI_SETCARETLINEBACKALPHA,iValue,0);
      else
        SendMessage(hwnd,SCI_SETCARETLINEBACKALPHA,SC_ALPHA_NOALPHA,0);
    }
    else
      SendMessage(hwnd,SCI_SETCARETLINEVISIBLE,FALSE,0);
  }
  else
    SendMessage(hwnd,SCI_SETCARETLINEVISIBLE,FALSE,0);

  // caret style and width
  if (StrStr(lexDefault.Styles[9+iIdx].szValue,L"block")) {
    SendMessage(hwnd,SCI_SETCARETSTYLE,CARETSTYLE_BLOCK,0);
    lstrcpy(wchCaretStyle,L"block");
  }
  else {
    WCHAR wch[32];
    iValue = 1;
    if (Style_StrGetSize(lexDefault.Styles[9+iIdx].szValue,&iValue)) {
      iValue = max(min(iValue,3),1);
      wsprintf(wch,L"size:%i",iValue);
      lstrcat(wchCaretStyle,wch);
    }
    SendMessage(hwnd,SCI_SETCARETSTYLE,CARETSTYLE_LINE,0);
    SendMessage(hwnd,SCI_SETCARETWIDTH,iValue,0);
  }
  if (StrStr(lexDefault.Styles[9+iIdx].szValue,L"noblink")) {
    SendMessage(hwnd,SCI_SETCARETPERIOD,(WPARAM)0,0);
    if (lstrlen(wchCaretStyle))
      lstrcat(wchCaretStyle,L"; ");
    lstrcat(wchCaretStyle,L"noblink");
  }
  else
    SendMessage(hwnd,SCI_SETCARETPERIOD,(WPARAM)GetCaretBlinkTime(),0);

  // caret fore
  if (!Style_StrGetColor(TRUE,lexDefault.Styles[9+iIdx].szValue,&rgb))
    rgb = GetSysColor(COLOR_WINDOWTEXT);
  else {
    WCHAR wch[32];
    wsprintf(wch,L"fore:#%02X%02X%02X",
      (int)GetRValue(rgb),
      (int)GetGValue(rgb),
      (int)GetBValue(rgb));
    if (lstrlen(wchCaretStyle))
      lstrcat(wchCaretStyle,L"; ");
    lstrcat(wchCaretStyle,wch);
  }
  if (!VerifyContrast(rgb,(COLORREF)SendMessage(hwnd,SCI_STYLEGETBACK,0,0)))
    rgb = (int)SendMessage(hwnd,SCI_STYLEGETFORE,0,0);
  SendMessage(hwnd,SCI_SETCARETFORE,rgb,0);
  SendMessage(hwnd,SCI_SETADDITIONALCARETFORE,rgb,0);
  lstrcpy(lexDefault.Styles[9+iIdx].szValue,wchCaretStyle);

  if (SendMessage(hwnd,SCI_GETEDGEMODE,0,0) == EDGE_LINE) {
    if (Style_StrGetColor(TRUE,lexDefault.Styles[10+iIdx].szValue,&rgb)) // edge fore
      SendMessage(hwnd,SCI_SETEDGECOLOUR,rgb,0);
    else
      SendMessage(hwnd,SCI_SETEDGECOLOUR,GetSysColor(COLOR_3DLIGHT),0);
  }
  else {
    if (Style_StrGetColor(FALSE,lexDefault.Styles[10+iIdx].szValue,&rgb)) // edge back
      SendMessage(hwnd,SCI_SETEDGECOLOUR,rgb,0);
    else
      SendMessage(hwnd,SCI_SETEDGECOLOUR,GetSysColor(COLOR_3DLIGHT),0);
  }

  // Extra Line Spacing
  if (Style_StrGetSize(lexDefault.Styles[11+iIdx].szValue,&iValue)) {
    int iAscent = 0;
    int iDescent = 0;
    iValue = min(max(iValue,0),64);
    wsprintf(lexDefault.Styles[11+iIdx].szValue,L"size:%i",iValue);
    if (iValue % 2) {
      iAscent++;
      iValue--;
    }
    iAscent += iValue / 2;
    iDescent += iValue / 2;
    SendMessage(hwnd,SCI_SETEXTRAASCENT,(WPARAM)iAscent,0);
    SendMessage(hwnd,SCI_SETEXTRADESCENT,(WPARAM)iDescent,0);
  }
  else {
    SendMessage(hwnd,SCI_SETEXTRAASCENT,0,0);
    SendMessage(hwnd,SCI_SETEXTRADESCENT,0,0);
    //wsprintf(lexDefault.Styles[11+iIdx].szValue,L"size:0");
  }

  if (SendMessage(hwnd,SCI_GETINDENTATIONGUIDES,0,0) != SC_IV_NONE)
    Style_SetIndentGuides(hwnd,TRUE);

  if (pLexNew->iLexer != SCLEX_NULL)
  {
    int j;
    i = 1;
    while (pLexNew->Styles[i].iStyle != -1) {

      for (j = 0; j < 4 && (pLexNew->Styles[i].iStyle8[j] != 0 || j == 0); ++j)
        Style_SetStyles(hwnd,pLexNew->Styles[i].iStyle8[j],pLexNew->Styles[i].szValue);

      if (pLexNew->iLexer == SCLEX_HTML && pLexNew->Styles[i].iStyle8[0] == SCE_HPHP_DEFAULT) {
        int iRelated[] = { SCE_HPHP_COMMENT, SCE_HPHP_COMMENTLINE, SCE_HPHP_WORD, SCE_HPHP_HSTRING, SCE_HPHP_SIMPLESTRING, SCE_HPHP_NUMBER,
                           SCE_HPHP_OPERATOR, SCE_HPHP_VARIABLE, SCE_HPHP_HSTRING_VARIABLE, SCE_HPHP_COMPLEX_VARIABLE };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if (pLexNew->iLexer == SCLEX_HTML && pLexNew->Styles[i].iStyle8[0] == SCE_HJ_DEFAULT) {
        int iRelated[] = { SCE_HJ_COMMENT, SCE_HJ_COMMENTLINE, SCE_HJ_COMMENTDOC, SCE_HJ_KEYWORD, SCE_HJ_WORD, SCE_HJ_DOUBLESTRING,
                           SCE_HJ_SINGLESTRING, SCE_HJ_STRINGEOL, SCE_HJ_REGEX, SCE_HJ_NUMBER, SCE_HJ_SYMBOLS };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if (pLexNew->iLexer == SCLEX_HTML && pLexNew->Styles[i].iStyle8[0] == SCE_HJA_DEFAULT) {
        int iRelated[] = { SCE_HJA_COMMENT, SCE_HJA_COMMENTLINE, SCE_HJA_COMMENTDOC, SCE_HJA_KEYWORD, SCE_HJA_WORD, SCE_HJA_DOUBLESTRING,
                           SCE_HJA_SINGLESTRING, SCE_HJA_STRINGEOL, SCE_HJA_REGEX, SCE_HJA_NUMBER, SCE_HJA_SYMBOLS };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if (pLexNew->iLexer == SCLEX_HTML && pLexNew->Styles[i].iStyle8[0] == SCE_HB_DEFAULT) {
        int iRelated[] = { SCE_HB_COMMENTLINE, SCE_HB_WORD, SCE_HB_IDENTIFIER, SCE_HB_STRING, SCE_HB_STRINGEOL, SCE_HB_NUMBER };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if (pLexNew->iLexer == SCLEX_HTML && pLexNew->Styles[i].iStyle8[0] == SCE_HBA_DEFAULT) {
        int iRelated[] = { SCE_HBA_COMMENTLINE, SCE_HBA_WORD, SCE_HBA_IDENTIFIER, SCE_HBA_STRING, SCE_HBA_STRINGEOL, SCE_HBA_NUMBER };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if ((pLexNew->iLexer == SCLEX_HTML || pLexNew->iLexer == SCLEX_XML) && pLexNew->Styles[i].iStyle8[0] == SCE_H_SGML_DEFAULT) {
        int iRelated[] = { SCE_H_SGML_COMMAND, SCE_H_SGML_1ST_PARAM, SCE_H_SGML_DOUBLESTRING, SCE_H_SGML_SIMPLESTRING, SCE_H_SGML_ERROR,
                           SCE_H_SGML_SPECIAL, SCE_H_SGML_ENTITY, SCE_H_SGML_COMMENT, SCE_H_SGML_1ST_PARAM_COMMENT, SCE_H_SGML_BLOCK_DEFAULT };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if ((pLexNew->iLexer == SCLEX_HTML || pLexNew->iLexer == SCLEX_XML) && pLexNew->Styles[i].iStyle8[0] == SCE_H_CDATA) {
        int iRelated[] = { SCE_HP_START, SCE_HP_DEFAULT, SCE_HP_COMMENTLINE, SCE_HP_NUMBER, SCE_HP_STRING,
                           SCE_HP_CHARACTER, SCE_HP_WORD, SCE_HP_TRIPLE, SCE_HP_TRIPLEDOUBLE, SCE_HP_CLASSNAME,
                           SCE_HP_DEFNAME, SCE_HP_OPERATOR, SCE_HP_IDENTIFIER, SCE_HPA_START, SCE_HPA_DEFAULT,
                           SCE_HPA_COMMENTLINE, SCE_HPA_NUMBER, SCE_HPA_STRING, SCE_HPA_CHARACTER, SCE_HPA_WORD,
                           SCE_HPA_TRIPLE, SCE_HPA_TRIPLEDOUBLE, SCE_HPA_CLASSNAME, SCE_HPA_DEFNAME, SCE_HPA_OPERATOR,
                           SCE_HPA_IDENTIFIER };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if (pLexNew->iLexer == SCLEX_XML && pLexNew->Styles[i].iStyle8[0] == SCE_H_CDATA) {
        int iRelated[] = { SCE_H_SCRIPT, SCE_H_ASP, SCE_H_ASPAT, SCE_H_QUESTION,
                           SCE_HPHP_DEFAULT, SCE_HPHP_COMMENT, SCE_HPHP_COMMENTLINE, SCE_HPHP_WORD, SCE_HPHP_HSTRING,
                           SCE_HPHP_SIMPLESTRING, SCE_HPHP_NUMBER, SCE_HPHP_OPERATOR, SCE_HPHP_VARIABLE,
                           SCE_HPHP_HSTRING_VARIABLE, SCE_HPHP_COMPLEX_VARIABLE, SCE_HJ_START, SCE_HJ_DEFAULT,
                           SCE_HJ_COMMENT, SCE_HJ_COMMENTLINE, SCE_HJ_COMMENTDOC, SCE_HJ_KEYWORD, SCE_HJ_WORD,
                           SCE_HJ_DOUBLESTRING, SCE_HJ_SINGLESTRING, SCE_HJ_STRINGEOL, SCE_HJ_REGEX, SCE_HJ_NUMBER,
                           SCE_HJ_SYMBOLS, SCE_HJA_START, SCE_HJA_DEFAULT, SCE_HJA_COMMENT, SCE_HJA_COMMENTLINE,
                           SCE_HJA_COMMENTDOC, SCE_HJA_KEYWORD, SCE_HJA_WORD, SCE_HJA_DOUBLESTRING, SCE_HJA_SINGLESTRING,
                           SCE_HJA_STRINGEOL, SCE_HJA_REGEX, SCE_HJA_NUMBER, SCE_HJA_SYMBOLS, SCE_HB_START, SCE_HB_DEFAULT,
                           SCE_HB_COMMENTLINE, SCE_HB_WORD, SCE_HB_IDENTIFIER, SCE_HB_STRING, SCE_HB_STRINGEOL,
                           SCE_HB_NUMBER, SCE_HBA_START, SCE_HBA_DEFAULT, SCE_HBA_COMMENTLINE, SCE_HBA_WORD,
                           SCE_HBA_IDENTIFIER, SCE_HBA_STRING, SCE_HBA_STRINGEOL, SCE_HBA_NUMBER, SCE_HP_START,
                           SCE_HP_DEFAULT, SCE_HP_COMMENTLINE, SCE_HP_NUMBER, SCE_HP_STRING, SCE_HP_CHARACTER, SCE_HP_WORD,
                           SCE_HP_TRIPLE, SCE_HP_TRIPLEDOUBLE, SCE_HP_CLASSNAME, SCE_HP_DEFNAME, SCE_HP_OPERATOR,
                           SCE_HP_IDENTIFIER, SCE_HPA_START, SCE_HPA_DEFAULT, SCE_HPA_COMMENTLINE, SCE_HPA_NUMBER,
                           SCE_HPA_STRING, SCE_HPA_CHARACTER, SCE_HPA_WORD, SCE_HPA_TRIPLE, SCE_HPA_TRIPLEDOUBLE,
                           SCE_HPA_CLASSNAME, SCE_HPA_DEFNAME, SCE_HPA_OPERATOR, SCE_HPA_IDENTIFIER };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }

      if (pLexNew -> iLexer == SCLEX_SQL && pLexNew->Styles[i].iStyle8[0] == SCE_SQL_COMMENT) {
        int iRelated[] = { SCE_SQL_COMMENTLINE, SCE_SQL_COMMENTDOC, SCE_SQL_COMMENTLINEDOC, SCE_SQL_COMMENTDOCKEYWORD, SCE_SQL_COMMENTDOCKEYWORDERROR };
        for (j = 0; j < COUNTOF(iRelated); j++)
          Style_SetStyles(hwnd,iRelated[j],pLexNew->Styles[i].szValue);
      }
      i++;
    }
  }

  SendMessage(hwnd,SCI_COLOURISE,0,(LPARAM)-1);

  // Save current lexer
  pLexCurrent = pLexNew;
}


//=============================================================================
//
//  Style_SetLongLineColors()
//
void Style_SetLongLineColors(HWND hwnd)
{
  int rgb;

  // Use 2nd default style
  int iIdx = (bUse2ndDefaultStyle) ? 12 : 0;

  if (SendMessage(hwnd,SCI_GETEDGEMODE,0,0) == EDGE_LINE) {
    if (Style_StrGetColor(TRUE,lexDefault.Styles[10+iIdx].szValue,&rgb)) // edge fore
      SendMessage(hwnd,SCI_SETEDGECOLOUR,rgb,0);
    else
      SendMessage(hwnd,SCI_SETEDGECOLOUR,GetSysColor(COLOR_3DLIGHT),0);
  }
  else {
    if (Style_StrGetColor(FALSE,lexDefault.Styles[10+iIdx].szValue,&rgb)) // edge back
      SendMessage(hwnd,SCI_SETEDGECOLOUR,rgb,0);
    else
      SendMessage(hwnd,SCI_SETEDGECOLOUR,GetSysColor(COLOR_3DLIGHT),0);
  }
}


//=============================================================================
//
//  Style_SetCurrentLineBackground()
//
void Style_SetCurrentLineBackground(HWND hwnd)
{
  int rgb, iValue;

  // Use 2nd default style
  int iIdx = (bUse2ndDefaultStyle) ? 12 : 0;

  if (bHiliteCurrentLine) {

    if (Style_StrGetColor(FALSE,lexDefault.Styles[8+iIdx].szValue,&rgb)) // caret line back
    {
      SendMessage(hwnd,SCI_SETCARETLINEVISIBLE,TRUE,0);
      SendMessage(hwnd,SCI_SETCARETLINEBACK,rgb,0);

      if (Style_StrGetAlpha(lexDefault.Styles[8+iIdx].szValue,&iValue))
        SendMessage(hwnd,SCI_SETCARETLINEBACKALPHA,iValue,0);
       else
        SendMessage(hwnd,SCI_SETCARETLINEBACKALPHA,SC_ALPHA_NOALPHA,0);
    }
    else
      SendMessage(hwnd,SCI_SETCARETLINEVISIBLE,FALSE,0);
  }
  else
    SendMessage(hwnd,SCI_SETCARETLINEVISIBLE,FALSE,0);
}


//=============================================================================
//
//  Style_SniffShebang()
//
PEDITLEXER __fastcall Style_SniffShebang(char *pchText)
{
  if (StrCmpNA(pchText,"#!",2) == 0) {
    char *pch = pchText + 2;
    while (*pch == ' ' || *pch == '\t')
      pch++;
    while (*pch && *pch != ' ' && *pch != '\t' && *pch != '\r' && *pch != '\n')
      pch++;
    if ((pch - pchText) >= 3 && StrCmpNA(pch-3,"env",3) == 0) {
      while (*pch == ' ')
        pch++;
      while (*pch && *pch != ' ' && *pch != '\t' && *pch != '\r' && *pch != '\n')
        pch++;
    }
  }

  return(NULL);
}


//=============================================================================
//
//  Style_MatchLexer()
//
PEDITLEXER __fastcall Style_MatchLexer(LPCWSTR lpszMatch,BOOL bCheckNames)
{
  int i;
  WCHAR  tch[256+16];
  WCHAR  *p1,*p2;

  if (!bCheckNames) {

    for (i = 0; i < NUMLEXERS; i++) {

      ZeroMemory(tch,sizeof(WCHAR)*COUNTOF(tch));
      lstrcpy(tch,pLexArray[i]->szExtensions);
      p1 = tch;
      while (*p1) {
        if ((p2 = StrChr(p1,L';')) != NULL)
          *p2 = L'\0';
        else
          p2 = StrEnd(p1);
        StrTrim(p1,L" .");
        if (lstrcmpi(p1,lpszMatch) == 0)
          return(pLexArray[i]);
        p1 = p2+1;
      }
    }
  }

  else {

    int cch = lstrlen(lpszMatch);
    if (cch >= 3) {

      for (i = 0; i < NUMLEXERS; i++) {
        if (StrCmpNI(pLexArray[i]->pszName,lpszMatch,cch) == 0)
          return(pLexArray[i]);
      }
    }
  }
  return(NULL);
}


//=============================================================================
//
//  Style_SetLexerFromFile()
//

extern int fNoHTMLGuess;
extern int fNoCGIGuess;
extern FILEVARS fvCurFile;

void Style_SetLexerFromFile(HWND hwnd,LPCWSTR lpszFile)
{
  LPWSTR lpszExt;
  BOOL  bFound = FALSE;
  PEDITLEXER pLexNew = pLexArray[iDefaultLexer];
 // PEDITLEXER pLexSniffed;
 // WCHAR wchMode[32];
  PEDITLEXER pLexMode;

  lpszExt = PathFindExtension(lpszFile);

  if ( bAutoSelect && (lpszFile && lstrlen(lpszFile) > 0 && *lpszExt)) {

	  if (*lpszExt == L'.')  lpszExt++;

	  if (lpszExt[0] == 'f' || lpszExt[0] == 'F')  {
		  if (lpszExt[1] == '2' &&
		      (lpszExt[2] == 'p' || lpszExt[2] == 'P') && lpszExt[3] == L'\0')  {
		  pLexNew = &lexF2P;   // .f2p -> Flex2PDF template lexer
		  Style_SetLexer(hwnd, pLexNew);
		  return ;
		  };
		  if (lpszExt[1] == '2')  {
		  pLexNew = &lexF2T;
		  Style_SetLexer(hwnd, pLexNew);
		  return ;
		  };
	  };


	  if ((pLexMode = Style_MatchLexer(lpszExt, FALSE)) != NULL) {
			  pLexNew = pLexMode;
			  bFound = TRUE;
		  }
	  else if ((pLexMode = Style_MatchLexer(lpszExt, TRUE)) != NULL) {
			  pLexNew = pLexMode;
			  bFound = TRUE;
		  }
//	  };

  };

  /*  if ((fvCurFile.mask & FV_MODE) && fvCurFile.tchMode[0]) {

    UINT cp = (UINT)SendMessage(hwnd,SCI_GETCODEPAGE,0,0);
    MultiByteToWideChar(cp,0,fvCurFile.tchMode,-1,wchMode,COUNTOF(wchMode));

    if (!fNoCGIGuess && (lstrcmpi(wchMode,L"cgi") == 0 || lstrcmpi(wchMode,L"fcgi") == 0)) {
      char tchText[256];
      SendMessage(hwnd,SCI_GETTEXT,(WPARAM)COUNTOF(tchText)-1,(LPARAM)tchText);
      StrTrimA(tchText," \t\n\r");
      if (pLexSniffed = Style_SniffShebang(tchText)) {
        pLexNew = pLexSniffed;
        bFound = TRUE;
      }
    }

    if (!bFound) {
      if (pLexMode = Style_MatchLexer(wchMode,FALSE)) {
        pLexNew = pLexMode;
        bFound = TRUE;
      }
      else if (pLexMode = Style_MatchLexer(wchMode,TRUE)) {
        pLexNew = pLexMode;
        bFound = TRUE;
      }
    }
  }

  lpszExt = PathFindExtension(lpszFile);
  if (!bFound && bAutoSelect &&       (lpszFile && lstrlen(lpszFile) > 0 && *lpszExt)) {

    if (*lpszExt == L'.')
      lpszExt++;

    if (!fNoCGIGuess && (lstrcmpi(lpszExt,L"cgi") == 0 || lstrcmpi(lpszExt,L"fcgi") == 0)) {
      char tchText[256];
      SendMessage(hwnd,SCI_GETTEXT,(WPARAM)COUNTOF(tchText)-1,(LPARAM)tchText);
      StrTrimA(tchText," \t\n\r");
      if (pLexSniffed = Style_SniffShebang(tchText)) {
        pLexNew = pLexSniffed;
        bFound = TRUE;
      }
    }

    // check associated extensions
    if (!bFound) {
      if (pLexSniffed = Style_MatchLexer(lpszExt,FALSE)) {
        pLexNew = pLexSniffed;
        bFound = TRUE;
      }
    }
  }

  if (!bFound && bAutoSelect &&
       lstrcmpi(PathFindFileName(lpszFile),L"f2t") == 0) {
    pLexNew = &lexF2T;
    bFound = TRUE;
  }

  if (!bFound && bAutoSelect && (!fNoHTMLGuess || !fNoCGIGuess)) {
    char tchText[512];
    SendMessage(hwnd,SCI_GETTEXT,(WPARAM)COUNTOF(tchText)-1,(LPARAM)tchText);
    StrTrimA(tchText," \t\n\r");
 //   if (!fNoHTMLGuess && tchText[0] == '<') {
	//     if (StrStrIA(tchText,"<html"))
	//     pLexNew = &lexHTML;
	//  else
        pLexNew = &lexXML;
      bFound = TRUE;
	  // }
	  //else 
	 if (!fNoCGIGuess && (pLexSniffed = Style_SniffShebang(tchText))) {
      pLexNew = pLexSniffed;
      bFound = TRUE;
    }
  }
  */

  // Apply the new lexer
  Style_SetLexer(hwnd,pLexNew);
}


//=============================================================================
//
//  Style_SetLexerFromName()
//
void Style_SetLexerFromName(HWND hwnd,LPCWSTR lpszFile,LPCWSTR lpszName)
{
  PEDITLEXER pLexNew;
  if ((pLexNew = Style_MatchLexer(lpszName,FALSE)) != NULL)
    Style_SetLexer(hwnd,pLexNew);
  else if ((pLexNew = Style_MatchLexer(lpszName,TRUE)) != NULL)
    Style_SetLexer(hwnd,pLexNew);
  else
    Style_SetLexerFromFile(hwnd,lpszFile);
}


//=============================================================================
//
//  Style_SetDefaultLexer()
//
void Style_SetDefaultLexer(HWND hwnd)
{
  Style_SetLexer(hwnd,pLexArray[0]);
}


//=============================================================================
//
//  Style_SetHTMLLexer()
//
void Style_SetHTMLLexer(HWND hwnd)
{
  Style_SetLexer(hwnd,Style_MatchLexer(L"Web Source Code",TRUE));
}


//=============================================================================
//
//  Style_SetXMLLexer()
//
void Style_SetXMLLexer(HWND hwnd)
{
  Style_SetLexer(hwnd,Style_MatchLexer(L"XML Document",TRUE));
}


//=============================================================================
//
//  Style_SetLexerFromID()
//
void Style_SetLexerFromID(HWND hwnd,int id)
{
  if (id >= 0 && id < NUMLEXERS) {
    Style_SetLexer(hwnd,pLexArray[id]);
  }
}


//=============================================================================
//
//  Style_ToggleUse2ndDefault()
//
void Style_ToggleUse2ndDefault(HWND hwnd)
{
  bUse2ndDefaultStyle = (bUse2ndDefaultStyle) ? 0 : 1;
  Style_SetLexer(hwnd,pLexCurrent);
}


//=============================================================================
//
//  Style_SetDefaultFont()
//
void Style_SetDefaultFont(HWND hwnd)
{
  int iIdx = (bUse2ndDefaultStyle) ? 12 : 0;
  if (Style_SelectFont(hwnd,
        lexDefault.Styles[0+iIdx].szValue,
        COUNTOF(lexDefault.Styles[0].szValue),
        TRUE)) {
    fStylesModified = TRUE;
    Style_SetLexer(hwnd,pLexCurrent);
  }
}


//=============================================================================
//
//  Style_GetUse2ndDefault()
//
BOOL Style_GetUse2ndDefault(HWND hwnd)
{
  (void)hwnd;
  return (bUse2ndDefaultStyle);
}


//=============================================================================
//
//  Style_SetIndentGuides()
//
extern int flagSimpleIndentGuides;

void Style_SetIndentGuides(HWND hwnd,BOOL bShow)
{
  int iIndentView = SC_IV_NONE;
  if (bShow) {
    if (!flagSimpleIndentGuides) {
      if (SendMessage(hwnd,SCI_GETLEXER,0,0) == SCLEX_PYTHON)
        iIndentView = SC_IV_LOOKFORWARD;
      else
        iIndentView = SC_IV_LOOKBOTH;
    }
    else
      iIndentView = SC_IV_REAL;
  }
  SendMessage(hwnd,SCI_SETINDENTATIONGUIDES,iIndentView,0);
}


//=============================================================================
//
//  Style_GetFileOpenDlgFilter()
//
extern WCHAR tchFileDlgFilters[5*1024];

BOOL Style_GetOpenDlgFilterStr(LPWSTR lpszFilter,int cchFilter)
{
  if (lstrlen(tchFileDlgFilters) == 0)
    GetString(IDS_FILTER_ALL,lpszFilter,cchFilter);
  else {
    lstrcpyn(lpszFilter,tchFileDlgFilters,cchFilter-2);
    lstrcat(lpszFilter,L"||");
  }
  PrepareFilterStr(lpszFilter);
  return TRUE;
}


//=============================================================================
//
//  Style_StrGetFont()
//
BOOL Style_StrGetFont(LPCWSTR lpszStyle,LPWSTR lpszFont,int cchFont)
{
  WCHAR tch[256];
  WCHAR *p;

  if ((p = StrStrI(lpszStyle,L"font:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"font:"));
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    lstrcpyn(lpszFont,tch,cchFont);
    return TRUE;
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetFontQuality()
//
BOOL Style_StrGetFontQuality(LPCWSTR lpszStyle,LPWSTR lpszQuality,int cchQuality)
{
  WCHAR tch[256];
  WCHAR *p;

  if ((p = StrStrI(lpszStyle,L"smoothing:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"smoothing:"));
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    if (lstrcmpi(tch,L"none") == 0 ||
        lstrcmpi(tch,L"standard") == 0 ||
        lstrcmpi(tch,L"cleartype") == 0 ||
        lstrcmpi(tch,L"default") == 0) {
      lstrcpyn(lpszQuality,tch,cchQuality);
      return TRUE;
    }
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetCharSet()
//
BOOL Style_StrGetCharSet(LPCWSTR lpszStyle,int *i)
{
  WCHAR tch[256];
  WCHAR *p;
  int  iValue;
  int  itok;

  if ((p = StrStrI(lpszStyle,L"charset:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"charset:"));
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    itok = swscanf(tch,L"%i",&iValue);
    if (itok == 1)
    {
      *i = iValue;
      return TRUE;
    }
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetSize()
//
BOOL Style_StrGetSize(LPCWSTR lpszStyle,int *i)
{
  WCHAR tch[256];
  WCHAR *p;
  int  iValue;
  int  iSign = 0;
  int  itok;

  if ((p = StrStrI(lpszStyle,L"size:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"size:"));
    if (tch[0] == L'+')
    {
      iSign = 1;
      tch[0] = L' ';
    }
    else if (tch[0] == L'-')
    {
      iSign = -1;
      tch[0] = L' ';
    }
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    itok = swscanf(tch,L"%i",&iValue);
    if (itok == 1)
    {
      if (iSign == 0)
        *i = iValue;
      else
        *i = max(0,iBaseFontSize + iValue * iSign); // size must be +
      return TRUE;
    }
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetSizeStr()
//
BOOL Style_StrGetSizeStr(LPCWSTR lpszStyle,LPWSTR lpszSize,int cchSize)
{
  WCHAR tch[256];
  WCHAR *p;

  if ((p = StrStrI(lpszStyle,L"size:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"size:"));
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    lstrcpyn(lpszSize,tch,cchSize);
    return TRUE;
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetColor()
//
BOOL Style_StrGetColor(BOOL bFore,LPCWSTR lpszStyle,int *rgb)
{
  WCHAR tch[256];
  WCHAR *p;
  int  iValue;
  int  itok;
  WCHAR *pItem = (bFore) ? L"fore:" : L"back:";

  if ((p = StrStrI(lpszStyle,pItem)) != NULL)
  {
    lstrcpy(tch,p + lstrlen(pItem));
    if (tch[0] == L'#')
      tch[0] = L' ';
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    itok = swscanf(tch,L"%x",&iValue);
    if (itok == 1)
    {
      *rgb = RGB((iValue&0xFF0000) >> 16,(iValue&0xFF00) >> 8,iValue&0xFF);
      return TRUE;
    }
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetCase()
//
BOOL Style_StrGetCase(LPCWSTR lpszStyle,int *i)
{
  WCHAR tch[256];
  WCHAR *p;

  if ((p = StrStrI(lpszStyle,L"case:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"case:"));
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    if (tch[0] == L'u' || tch[0] == L'U') {
      *i = SC_CASE_UPPER;
      return TRUE;
    }
    else if (tch[0] == L'l' || tch[0] == L'L') {
      *i = SC_CASE_LOWER;
      return TRUE;
    }
  }
  return FALSE;
}


//=============================================================================
//
//  Style_StrGetAlpha()
//
BOOL Style_StrGetAlpha(LPCWSTR lpszStyle,int *i)
{
  WCHAR tch[256];
  WCHAR *p;
  int  iValue;
  int  itok;

  if ((p = StrStrI(lpszStyle,L"alpha:")) != NULL)
  {
    lstrcpy(tch,p + CSTRLEN(L"alpha:"));
    if ((p = StrChr(tch,L';')) != NULL)
      *p = L'\0';
    TrimString(tch);
    itok = swscanf(tch,L"%i",&iValue);
    if (itok == 1)
    {
      *i = min(max(SC_ALPHA_TRANSPARENT,iValue),SC_ALPHA_OPAQUE);
      return TRUE;
    }
  }
  return FALSE;
}


//=============================================================================
//
//  Style_SelectFont()
//
BOOL Style_SelectFont(HWND hwnd,LPWSTR lpszStyle,int cchStyle,BOOL bDefaultStyle)
{
  CHOOSEFONT cf;
  LOGFONT lf;
  WCHAR szNewStyle[512];
  int  iValue;
  WCHAR tch[32];
  HDC hdc;

  ZeroMemory(&cf,sizeof(CHOOSEFONT));
  ZeroMemory(&lf,sizeof(LOGFONT));

  // Map lpszStyle to LOGFONT
  if (Style_StrGetFont(lpszStyle,tch,COUNTOF(tch)))
    lstrcpyn(lf.lfFaceName,tch,COUNTOF(lf.lfFaceName));
  if (Style_StrGetCharSet(lpszStyle,&iValue))
    lf.lfCharSet = (BYTE)iValue;
  if (Style_StrGetSize(lpszStyle,&iValue)) {
    hdc = GetDC(hwnd);
    lf.lfHeight = -MulDiv(iValue,GetDeviceCaps(hdc,LOGPIXELSY),72);
    ReleaseDC(hwnd,hdc);
  }
  lf.lfWeight = (StrStrI(lpszStyle,L"bold")) ? FW_BOLD : FW_NORMAL;
  lf.lfItalic = (StrStrI(lpszStyle,L"italic")) ? 1 : 0;

  // Init cf
  cf.lStructSize = sizeof(CHOOSEFONT);
  cf.hwndOwner = hwnd;
  cf.lpLogFont = &lf;
  cf.Flags = CF_INITTOLOGFONTSTRUCT /*| CF_NOSCRIPTSEL*/ | CF_SCREENFONTS;

  if (HIBYTE(GetKeyState(VK_SHIFT)))
    cf.Flags |= CF_FIXEDPITCHONLY;

  if (!ChooseFont(&cf) || !lstrlen(lf.lfFaceName))
    return FALSE;

  // Map back to lpszStyle
  lstrcpy(szNewStyle,L"font:");
  lstrcat(szNewStyle,lf.lfFaceName);
  if (Style_StrGetFontQuality(lpszStyle,tch,COUNTOF(tch)))
  {
    lstrcat(szNewStyle,L"; smoothing:");
    lstrcat(szNewStyle,tch);
  }
  if (bDefaultStyle &&
      lf.lfCharSet != DEFAULT_CHARSET &&
      lf.lfCharSet != ANSI_CHARSET &&
      lf.lfCharSet != iDefaultCharSet) {
    lstrcat(szNewStyle,L"; charset:");
    wsprintf(tch,L"%i",lf.lfCharSet);
    lstrcat(szNewStyle,tch);
  }
  lstrcat(szNewStyle,L"; size:");
  wsprintf(tch,L"%i",cf.iPointSize/10);
  lstrcat(szNewStyle,tch);
  if (cf.nFontType & BOLD_FONTTYPE)
    lstrcat(szNewStyle,L"; bold");
  if (cf.nFontType & ITALIC_FONTTYPE)
    lstrcat(szNewStyle,L"; italic");

  if (StrStrI(lpszStyle,L"underline"))
    lstrcat(szNewStyle,L"; underline");

  // save colors
  if (Style_StrGetColor(TRUE,lpszStyle,&iValue))
  {
    wsprintf(tch,L"; fore:#%02X%02X%02X",
      (int)GetRValue(iValue),
      (int)GetGValue(iValue),
      (int)GetBValue(iValue));
    lstrcat(szNewStyle,tch);
  }
  if (Style_StrGetColor(FALSE,lpszStyle,&iValue))
  {
    wsprintf(tch,L"; back:#%02X%02X%02X",
      (int)GetRValue(iValue),
      (int)GetGValue(iValue),
      (int)GetBValue(iValue));
    lstrcat(szNewStyle,tch);
  }

  if (StrStrI(lpszStyle,L"eolfilled"))
    lstrcat(szNewStyle,L"; eolfilled");

  if (Style_StrGetCase(lpszStyle,&iValue)) {
    lstrcat(szNewStyle,L"; case:");
    lstrcat(szNewStyle,(iValue == SC_CASE_UPPER) ? L"u" : L"");
  }

  if (Style_StrGetAlpha(lpszStyle,&iValue)) {
    lstrcat(szNewStyle,L"; alpha:");
    wsprintf(tch,L"%i",iValue);
    lstrcat(szNewStyle,tch);
  }

  lstrcpyn(lpszStyle,szNewStyle,cchStyle);
  return TRUE;
}


//=============================================================================
//
//  Style_SelectColor()
//
BOOL Style_SelectColor(HWND hwnd,BOOL bFore,LPWSTR lpszStyle,int cchStyle)
{
  CHOOSECOLOR cc;
  WCHAR szNewStyle[512];
  int  iRGBResult;
  int  iValue;
  WCHAR tch[32];

  ZeroMemory(&cc,sizeof(CHOOSECOLOR));

  iRGBResult = (bFore) ? GetSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_WINDOW);
  Style_StrGetColor(bFore,lpszStyle,&iRGBResult);

  cc.lStructSize = sizeof(CHOOSECOLOR);
  cc.hwndOwner = hwnd;
  cc.rgbResult = iRGBResult;
  cc.lpCustColors = crCustom;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR;

  if (!ChooseColor(&cc))
    return FALSE;

  iRGBResult = cc.rgbResult;

  // Rebuild style string
  lstrcpy(szNewStyle,L"");
  if (Style_StrGetFont(lpszStyle,tch,COUNTOF(tch)))
  {
    lstrcat(szNewStyle,L"font:");
    lstrcat(szNewStyle,tch);
  }
  if (Style_StrGetFontQuality(lpszStyle,tch,COUNTOF(tch)))
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    lstrcat(szNewStyle,L"smoothing:");
    lstrcat(szNewStyle,tch);
  }
  if (Style_StrGetCharSet(lpszStyle,&iValue))
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    wsprintf(tch,L"charset:%i",iValue);
    lstrcat(szNewStyle,tch);
  }
  if (Style_StrGetSizeStr(lpszStyle,tch,COUNTOF(tch)))
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    lstrcat(szNewStyle,L"size:");
    lstrcat(szNewStyle,tch);
  }

  if (StrStrI(lpszStyle,L"bold"))
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    lstrcat(szNewStyle,L"bold");
  }
  if (StrStrI(lpszStyle,L"italic"))
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    lstrcat(szNewStyle,L"italic");
  }
  if (StrStrI(lpszStyle,L"underline"))
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    lstrcat(szNewStyle,L"underline");
  }

  if (bFore)
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    wsprintf(tch,L"fore:#%02X%02X%02X",
      (int)GetRValue(iRGBResult),
      (int)GetGValue(iRGBResult),
      (int)GetBValue(iRGBResult));
    lstrcat(szNewStyle,tch);
    if (Style_StrGetColor(FALSE,lpszStyle,&iValue))
    {
      wsprintf(tch,L"; back:#%02X%02X%02X",
        (int)GetRValue(iValue),
        (int)GetGValue(iValue),
        (int)GetBValue(iValue));
      lstrcat(szNewStyle,tch);
    }
  }
  else
  {
    if (lstrlen(szNewStyle))
      lstrcat(szNewStyle,L"; ");
    if (Style_StrGetColor(TRUE,lpszStyle,&iValue))
    {
      wsprintf(tch,L"fore:#%02X%02X%02X; ",
        (int)GetRValue(iValue),
        (int)GetGValue(iValue),
        (int)GetBValue(iValue));
      lstrcat(szNewStyle,tch);
    }
    wsprintf(tch,L"back:#%02X%02X%02X",
      (int)GetRValue(iRGBResult),
      (int)GetGValue(iRGBResult),
      (int)GetBValue(iRGBResult));
    lstrcat(szNewStyle,tch);
  }

  if (StrStrI(lpszStyle,L"eolfilled"))
    lstrcat(szNewStyle,L"; eolfilled");

  if (Style_StrGetCase(lpszStyle,&iValue)) {
    lstrcat(szNewStyle,L"; case:");
    lstrcat(szNewStyle,(iValue == SC_CASE_UPPER) ? L"u" : L"");
  }

  if (Style_StrGetAlpha(lpszStyle,&iValue)) {
    lstrcat(szNewStyle,L"; alpha:");
    wsprintf(tch,L"%i",iValue);
    lstrcat(szNewStyle,tch);
  }

  if (StrStrI(lpszStyle,L"block"))
    lstrcat(szNewStyle,L"; block");

  if (StrStrI(lpszStyle,L"noblink"))
    lstrcat(szNewStyle,L"; noblink");

  lstrcpyn(lpszStyle,szNewStyle,cchStyle);
  return TRUE;
}


//=============================================================================
//
//  Style_SetStyles()
//
void Style_SetStyles(HWND hwnd,int iStyle,LPCWSTR lpszStyle)
{

  WCHAR tch[256];
  WCHAR *p;
  int  iValue;

  // Font
  if (Style_StrGetFont(lpszStyle,tch,COUNTOF(tch))) {
    char mch[256] = "Lucida Console";
    if (fIsConsolasAvailable || lstrcmpi(tch,L"Consolas"))
      WideCharToMultiByte(CP_ACP,0,tch,-1,mch,COUNTOF(mch),NULL,NULL);
    SendMessage(hwnd,SCI_STYLESETFONT,iStyle,(LPARAM)mch);
  }

  // Size
  if (Style_StrGetSize(lpszStyle,&iValue))
    SendMessage(hwnd,SCI_STYLESETSIZE,iStyle,(LPARAM)iValue);

  // Fore
  if (Style_StrGetColor(TRUE,lpszStyle,&iValue))
    SendMessage(hwnd,SCI_STYLESETFORE,iStyle,(LPARAM)iValue);

  // Back
  if (Style_StrGetColor(FALSE,lpszStyle,&iValue))
    SendMessage(hwnd,SCI_STYLESETBACK,iStyle,(LPARAM)iValue);

  // Bold
  if ((p = StrStrI(lpszStyle,L"bold")) != NULL)
    SendMessage(hwnd,SCI_STYLESETBOLD,iStyle,(LPARAM)TRUE);
  else
    SendMessage(hwnd,SCI_STYLESETBOLD,iStyle,(LPARAM)FALSE);

  // Italic
  if ((p = StrStrI(lpszStyle,L"italic")) != NULL)
    SendMessage(hwnd,SCI_STYLESETITALIC,iStyle,(LPARAM)TRUE);
  else
    SendMessage(hwnd,SCI_STYLESETITALIC,iStyle,(LPARAM)FALSE);

  // Underline
  if ((p = StrStrI(lpszStyle,L"underline")) != NULL)
    SendMessage(hwnd,SCI_STYLESETUNDERLINE,iStyle,(LPARAM)TRUE);
  else
    SendMessage(hwnd,SCI_STYLESETUNDERLINE,iStyle,(LPARAM)FALSE);

  // EOL Filled
  if ((p = StrStrI(lpszStyle,L"eolfilled")) != NULL)
    SendMessage(hwnd,SCI_STYLESETEOLFILLED,iStyle,(LPARAM)TRUE);
  else
    SendMessage(hwnd,SCI_STYLESETEOLFILLED,iStyle,(LPARAM)FALSE);

  // Case
  if (Style_StrGetCase(lpszStyle,&iValue))
    SendMessage(hwnd,SCI_STYLESETCASE,iStyle,(LPARAM)iValue);

  // Character Set
  if (Style_StrGetCharSet(lpszStyle,&iValue))
    SendMessage(hwnd,SCI_STYLESETCHARACTERSET,iStyle,(LPARAM)iValue);

}


//=============================================================================
//
//  Style_SetFontQuality()
//
void Style_SetFontQuality(HWND hwnd,LPCWSTR lpszStyle) {

  WPARAM wQuality = SC_EFF_QUALITY_DEFAULT;
  WCHAR tch[32];

  if (Style_StrGetFontQuality(lpszStyle,tch,COUNTOF(tch))) {
    if (lstrcmpi(tch,L"none") == 0)
      wQuality = SC_EFF_QUALITY_NON_ANTIALIASED;
    else if (lstrcmpi(tch,L"standard") == 0)
      wQuality = SC_EFF_QUALITY_ANTIALIASED;
    else if (lstrcmpi(tch,L"cleartype") == 0)
      wQuality = SC_EFF_QUALITY_LCD_OPTIMIZED;
    else
      wQuality = SC_EFF_QUALITY_DEFAULT;
  }
  else {
    if (Style_StrGetFont(lpszStyle,tch,COUNTOF(tch))) {
      if (lstrcmpi(tch,L"Calibri") == 0 ||
          lstrcmpi(tch,L"Cambria") == 0 ||
          lstrcmpi(tch,L"Candara") == 0 ||
          lstrcmpi(tch,L"Consolas") == 0 ||
          lstrcmpi(tch,L"Constantia") == 0 ||
          lstrcmpi(tch,L"Corbel") == 0 ||
          lstrcmpi(tch,L"Segoe UI") == 0)
        wQuality = SC_EFF_QUALITY_LCD_OPTIMIZED;
    }
    else
      wQuality = SC_EFF_QUALITY_DEFAULT;
  }
  SendMessage(hwnd,SCI_SETFONTQUALITY,wQuality,0);
}


//=============================================================================
//
//  Style_GetCurrentLexerName()
//
void Style_GetCurrentLexerName(LPWSTR lpszName,int cchName)
{
  if (!GetString(pLexCurrent->rid,lpszName,cchName))
    lstrcpyn(lpszName,pLexCurrent->pszName,cchName);
}


//=============================================================================
//
//  Style_GetLexerIconId()
//
int Style_GetLexerIconId(PEDITLEXER plex)
{
  WCHAR *p;
  WCHAR *pszExtensions;
  WCHAR *pszFile;

  SHFILEINFO shfi;

  if (lstrlen(plex->szExtensions))
    pszExtensions = plex->szExtensions;
  else
    pszExtensions = plex->pszDefExt;

  pszFile = GlobalAlloc(GPTR,sizeof(WCHAR)*(lstrlen(pszExtensions) + CSTRLEN(L"*.txt") + 16));
  lstrcpy(pszFile,L"*.");
  lstrcat(pszFile,pszExtensions);
  if ((p = StrChr(pszFile,L';')) != NULL)
    *p = L'\0';

  // check for ; at beginning
  if (lstrlen(pszFile) < 3)
    lstrcat(pszFile,L"txt");

  SHGetFileInfo(pszFile,FILE_ATTRIBUTE_NORMAL,&shfi,sizeof(SHFILEINFO),
    SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

  GlobalFree(pszFile);

  return (shfi.iIcon);
}


//=============================================================================
//
//  Style_AddLexerToTreeView()
//
void Style_AddLexerToTreeView(HWND hwnd,PEDITLEXER plex)
{
  int i = 0;
  WCHAR tch[128];

  HTREEITEM hTreeNode;

  TVINSERTSTRUCT tvis;
  ZeroMemory(&tvis,sizeof(TVINSERTSTRUCT));

  tvis.hInsertAfter = TVI_LAST;

  tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
  if (GetString(plex->rid,tch,COUNTOF(tch)))
    tvis.item.pszText = tch;
  else
    tvis.item.pszText = plex->pszName;
  tvis.item.iImage = Style_GetLexerIconId(plex);
  tvis.item.iSelectedImage = tvis.item.iImage;
  tvis.item.lParam = (LPARAM)plex;

  hTreeNode = (HTREEITEM)TreeView_InsertItem(hwnd,&tvis);

  tvis.hParent = hTreeNode;

  tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
  //tvis.item.iImage = -1;
  //tvis.item.iSelectedImage = -1;

  while (plex->Styles[i].iStyle != -1) {

    if (GetString(plex->Styles[i].rid,tch,COUNTOF(tch)))
      tvis.item.pszText = tch;
    else
      tvis.item.pszText = plex->Styles[i].pszName;
    tvis.item.lParam = (LPARAM)(&plex->Styles[i]);
    TreeView_InsertItem(hwnd,&tvis);
    i++;
  }
}


//=============================================================================
//
//  Style_AddLexerToListView()
//
void Style_AddLexerToListView(HWND hwnd,PEDITLEXER plex)
{
  WCHAR tch[128];
  LVITEM lvi;
  ZeroMemory(&lvi,sizeof(LVITEM));

  lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT;
  lvi.iItem = ListView_GetItemCount(hwnd);
  if (GetString(plex->rid,tch,COUNTOF(tch)))
    lvi.pszText = tch;
  else
    lvi.pszText = plex->pszName;
  lvi.iImage = Style_GetLexerIconId(plex);
  lvi.lParam = (LPARAM)plex;

  ListView_InsertItem(hwnd,&lvi);
}


//=============================================================================
//
//  Style_SelectLexerDlgProc()
//
INT_PTR CALLBACK Style_SelectLexerDlgProc(HWND hwnd,UINT umsg,WPARAM wParam,LPARAM lParam)
{

  static int cxClient;
  static int cyClient;
  static int mmiPtMaxY;
  static int mmiPtMinX;

  static HWND hwndLV;
  static int  iInternalDefault;

  switch(umsg)
  {

    case WM_INITDIALOG:
      {
        int i;
        int lvItems;
        LVITEM lvi;
        SHFILEINFO shfi;
        LVCOLUMN lvc = { LVCF_FMT|LVCF_TEXT, LVCFMT_LEFT, 0, L"", -1, 0, 0, 0 };

        RECT rc;
        WCHAR tch[MAX_PATH];
        int cGrip;

        GetClientRect(hwnd,&rc);
        cxClient = rc.right - rc.left;
        cyClient = rc.bottom - rc.top;

        AdjustWindowRectEx(&rc,GetWindowLong(hwnd,GWL_STYLE)|WS_THICKFRAME,FALSE,0);
        mmiPtMinX = rc.right-rc.left;
        mmiPtMaxY = rc.bottom-rc.top;

        if (cxStyleSelectDlg < (rc.right-rc.left))
          cxStyleSelectDlg = rc.right-rc.left;
        if (cyStyleSelectDlg < (rc.bottom-rc.top))
          cyStyleSelectDlg = rc.bottom-rc.top;
        SetWindowPos(hwnd,NULL,rc.left,rc.top,cxStyleSelectDlg,cyStyleSelectDlg,SWP_NOZORDER);

        SetWindowLongPtr(hwnd,GWL_STYLE,GetWindowLongPtr(hwnd,GWL_STYLE)|WS_THICKFRAME);
        SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED);

        GetMenuString(GetSystemMenu(GetParent(hwnd),FALSE),SC_SIZE,tch,COUNTOF(tch),MF_BYCOMMAND);
        InsertMenu(GetSystemMenu(hwnd,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_STRING|MF_ENABLED,SC_SIZE,tch);
        InsertMenu(GetSystemMenu(hwnd,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_SEPARATOR,0,NULL);

        SetWindowLongPtr(GetDlgItem(hwnd,IDC_RESIZEGRIP3),GWL_STYLE,
          GetWindowLongPtr(GetDlgItem(hwnd,IDC_RESIZEGRIP3),GWL_STYLE)|SBS_SIZEGRIP|WS_CLIPSIBLINGS);

        cGrip = GetSystemMetrics(SM_CXHTHUMB);
        SetWindowPos(GetDlgItem(hwnd,IDC_RESIZEGRIP3),NULL,cxClient-cGrip,
                     cyClient-cGrip,cGrip,cGrip,SWP_NOZORDER);

        hwndLV = GetDlgItem(hwnd,IDC_STYLELIST);

        ListView_SetImageList(hwndLV,
          (HIMAGELIST)SHGetFileInfo(L"C:\\",0,&shfi,sizeof(SHFILEINFO),SHGFI_SMALLICON | SHGFI_SYSICONINDEX),
          LVSIL_SMALL);

        ListView_SetImageList(hwndLV,
          (HIMAGELIST)SHGetFileInfo(L"C:\\",0,&shfi,sizeof(SHFILEINFO),SHGFI_LARGEICON | SHGFI_SYSICONINDEX),
          LVSIL_NORMAL);

        //SetExplorerTheme(hwndLV);
        ListView_SetExtendedListViewStyle(hwndLV,/*LVS_EX_FULLROWSELECT|*/LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
        ListView_InsertColumn(hwndLV,0,&lvc);

        // Add lexers
        for (i = 0; i < NUMLEXERS; i++)
          Style_AddLexerToListView(hwndLV,pLexArray[i]);

        ListView_SetColumnWidth(hwndLV,0,LVSCW_AUTOSIZE_USEHEADER);

        // Select current lexer
        lvItems = ListView_GetItemCount(hwndLV);
        lvi.mask = LVIF_PARAM;
        for (i = 0; i < lvItems; i++) {
          lvi.iItem = i;
          ListView_GetItem(hwndLV,&lvi);;
          if (lstrcmp(((PEDITLEXER)lvi.lParam)->pszName,pLexCurrent->pszName) == 0) {
            ListView_SetItemState(hwndLV,i,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
            ListView_EnsureVisible(hwndLV,i,FALSE);
            if (iDefaultLexer == i) {
              CheckDlgButton(hwnd,IDC_DEFAULTSCHEME,BST_CHECKED);
            }
          }
        }

        iInternalDefault = iDefaultLexer;

        if (bAutoSelect)
          CheckDlgButton(hwnd,IDC_AUTOSELECT,BST_CHECKED);

        CenterDlgInParent(hwnd);
      }
      return TRUE;


    case WM_DESTROY:
      {
        RECT rc;

        GetWindowRect(hwnd,&rc);
        cxStyleSelectDlg = rc.right-rc.left;
        cyStyleSelectDlg = rc.bottom-rc.top;
      }
      return FALSE;


    case WM_SIZE:
      {
        RECT rc;

        int dxClient = LOWORD(lParam) - cxClient;
        int dyClient = HIWORD(lParam) - cyClient;
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);

        GetWindowRect(GetDlgItem(hwnd,IDC_RESIZEGRIP3),&rc);
        MapWindowPoints(NULL,hwnd,(LPPOINT)&rc,2);
        SetWindowPos(GetDlgItem(hwnd,IDC_RESIZEGRIP3),NULL,rc.left+dxClient,rc.top+dyClient,0,0,SWP_NOZORDER|SWP_NOSIZE);
        InvalidateRect(GetDlgItem(hwnd,IDC_RESIZEGRIP3),NULL,TRUE);

        GetWindowRect(GetDlgItem(hwnd,IDOK),&rc);
        MapWindowPoints(NULL,hwnd,(LPPOINT)&rc,2);
        SetWindowPos(GetDlgItem(hwnd,IDOK),NULL,rc.left+dxClient,rc.top+dyClient,0,0,SWP_NOZORDER|SWP_NOSIZE);
        InvalidateRect(GetDlgItem(hwnd,IDOK),NULL,TRUE);

        GetWindowRect(GetDlgItem(hwnd,IDCANCEL),&rc);
        MapWindowPoints(NULL,hwnd,(LPPOINT)&rc,2);
        SetWindowPos(GetDlgItem(hwnd,IDCANCEL),NULL,rc.left+dxClient,rc.top+dyClient,0,0,SWP_NOZORDER|SWP_NOSIZE);
        InvalidateRect(GetDlgItem(hwnd,IDCANCEL),NULL,TRUE);

        GetWindowRect(GetDlgItem(hwnd,IDC_STYLELIST),&rc);
        MapWindowPoints(NULL,hwnd,(LPPOINT)&rc,2);
        SetWindowPos(GetDlgItem(hwnd,IDC_STYLELIST),NULL,0,0,rc.right-rc.left+dxClient,rc.bottom-rc.top+dyClient,SWP_NOZORDER|SWP_NOMOVE);
        ListView_SetColumnWidth(GetDlgItem(hwnd,IDC_STYLELIST),0,LVSCW_AUTOSIZE_USEHEADER);
        InvalidateRect(GetDlgItem(hwnd,IDC_STYLELIST),NULL,TRUE);

        GetWindowRect(GetDlgItem(hwnd,IDC_AUTOSELECT),&rc);
        MapWindowPoints(NULL,hwnd,(LPPOINT)&rc,2);
        SetWindowPos(GetDlgItem(hwnd,IDC_AUTOSELECT),NULL,rc.left,rc.top+dyClient,0,0,SWP_NOZORDER|SWP_NOSIZE);
        InvalidateRect(GetDlgItem(hwnd,IDC_AUTOSELECT),NULL,TRUE);

        GetWindowRect(GetDlgItem(hwnd,IDC_DEFAULTSCHEME),&rc);
        MapWindowPoints(NULL,hwnd,(LPPOINT)&rc,2);
        SetWindowPos(GetDlgItem(hwnd,IDC_DEFAULTSCHEME),NULL,rc.left,rc.top+dyClient,0,0,SWP_NOZORDER|SWP_NOSIZE);
        InvalidateRect(GetDlgItem(hwnd,IDC_DEFAULTSCHEME),NULL,TRUE);
      }
      return TRUE;


    case WM_GETMINMAXINFO:
      {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = mmiPtMinX;
        lpmmi->ptMinTrackSize.y = mmiPtMaxY;
        //lpmmi->ptMaxTrackSize.y = mmiPtMaxY;
      }
      return TRUE;


    case WM_NOTIFY: {
      if (((LPNMHDR)(lParam))->idFrom == IDC_STYLELIST) {

      switch (((LPNMHDR)(lParam))->code) {

        case NM_DBLCLK:
          SendMessage(hwnd,WM_COMMAND,MAKELONG(IDOK,1),0);
          break;

        case LVN_ITEMCHANGED:
        case LVN_DELETEITEM: {
          int i = ListView_GetNextItem(hwndLV,-1,LVNI_ALL | LVNI_SELECTED);
          if (iInternalDefault == i)
            CheckDlgButton(hwnd,IDC_DEFAULTSCHEME,BST_CHECKED);
          else
            CheckDlgButton(hwnd,IDC_DEFAULTSCHEME,BST_UNCHECKED);
          EnableWindow(GetDlgItem(hwnd,IDC_DEFAULTSCHEME),i != -1);
          EnableWindow(GetDlgItem(hwnd,IDOK),i != -1);
          }
          break;
        }
      }
    }

      return TRUE;


    case WM_COMMAND:

      switch(LOWORD(wParam))
      {

        case IDC_DEFAULTSCHEME:
          if (IsDlgButtonChecked(hwnd,IDC_DEFAULTSCHEME) == BST_CHECKED)
            iInternalDefault = ListView_GetNextItem(hwndLV,-1,LVNI_ALL | LVNI_SELECTED);
          else
            iInternalDefault = 0;
          break;


        case IDOK:
          {
            LVITEM lvi;

            lvi.mask = LVIF_PARAM;
            lvi.iItem = ListView_GetNextItem(hwndLV,-1,LVNI_ALL | LVNI_SELECTED);
            if (ListView_GetItem(hwndLV,&lvi)) {
              pLexCurrent = (PEDITLEXER)lvi.lParam;
              iDefaultLexer = iInternalDefault;
              bAutoSelect = (IsDlgButtonChecked(hwnd,IDC_AUTOSELECT) == BST_CHECKED) ? 1 : 0;
              EndDialog(hwnd,IDOK);
            }
          }
          break;


        case IDCANCEL:
          EndDialog(hwnd,IDCANCEL);
          break;

      }

      return TRUE;

  }

  return FALSE;

}


//=============================================================================
//
//  Style_SelectLexerDlg()
//
void Style_SelectLexerDlg(HWND hwnd)
{
  if (IDOK == ThemedDialogBoxParam(g_hInstance,
                MAKEINTRESOURCE(IDD_STYLESELECT),
                GetParent(hwnd),Style_SelectLexerDlgProc,0))

    Style_SetLexer(hwnd,pLexCurrent);
}


// End of Styles.c
