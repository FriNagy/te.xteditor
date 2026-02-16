// Scintilla source code edit control
/** @file LexCPS.cxx
 ** Lexer for CPS script files.
 **
 ** Based on CPS Skriptsprache syntax specification:
 **   - Comments: # at line start (only in non-markdown mode)
 **   - Labels: :labelname at line start (A-Za-z0-9_#äöüÄÖÜß, not starting with digit)
 **   - Variables: %varname / $varname (assignment), (%var) / (%var proc) (reference)
 **   - Output: > text (with [color] tags and escape sequences)
 **   - File/HTTP blocks: !> / !>> until empty line
 **   - Extract: !< / !<json / !<xml (single line)
 **   - Commands: goto, gosub, return, pause, shell, run, extract, etc.
 **   - IF keywords: if, ifnot, then, else, undef, next, in
 **   - Markdown mode: first non-empty line starting with # enables heading mode
 **   - Code fences: ~~~ / ``` (markdown)
 **/
// Copyright 2024-2026

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cctype>

#include <string>
#include <string_view>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

using namespace Lexilla;

// Custom SCLEX for CPS - use a high number to avoid conflicts
#define SCLEX_CPS 200

// Style definitions for CPS
#define SCE_CPS_DEFAULT 0
#define SCE_CPS_COMMENT 1            // # comment (normal mode, line start only)
#define SCE_CPS_LABEL 2              // :labelname
#define SCE_CPS_VARIABLE 3           // %var / $var assignment and reference
#define SCE_CPS_VAREXPAND_DELIM 4    // (% and ) and varname - red on yellow
#define SCE_CPS_VAREXPAND_CONTENT 5  // processor content in (%var proc) - black on yellow
#define SCE_CPS_STRING 6             // "..." / '...' and assignment values
#define SCE_CPS_OUTPUT 7             // > output text - light green
#define SCE_CPS_COMMAND 8            // Commands at line start: goto, include, shell, !> !< etc.
#define SCE_CPS_OPERATOR 9           // =, ==, >, <, ~=
#define SCE_CPS_COLORTAG 10          // [red], [#RGB], [bold], [cls] etc. (in > output)
#define SCE_CPS_FILEOP 11            // !> !>> file/HTTP write block content - gray
#define SCE_CPS_INPUT 12             // (reserved for future use)
#define SCE_CPS_FILEREAD 13          // !< extract content - dark blue
#define SCE_CPS_KEYWORD 14           // IF keywords: if, ifnot, then, else, undef, next, in
#define SCE_CPS_JUMP_REL 15          // Relative jumps: +N (1-32)
#define SCE_CPS_HEADER 16            // # heading (markdown mode), ~~~ / ``` fence
#define SCE_CPS_DOCCOMMENT 17        // (reserved for future use)

namespace {

// Commands that can appear at line start (word-based)
const char* lineCommands[] = {
    "goto", "gosub", "include", "shell", "run", "runw", "pause", "pause$",
    "setlog", "exit", "return", "call", "extract", "pmdump", "sleep", nullptr
};

// IF keywords (line start or inline in if-statements)
const char* ifKeywords[] = {
    "if", "ifnot", "then", "else", "undef", "next", "in", nullptr
};

// Variable name chars: A-Za-z0-9 _ $ & # plus high bytes (äöüÄÖÜß etc.)
inline bool IsVarNameChar(int ch) {
    return isalnum(ch) || ch == '_' || ch == '$' || ch == '&' || ch == '#' ||
           (static_cast<unsigned char>(ch) >= 0x80);
}

// Label name chars: A-Za-z0-9 _ # plus high bytes (äöüÄÖÜß etc.)
inline bool IsLabelChar(int ch) {
    return isalnum(ch) || ch == '_' || ch == '#' ||
           (static_cast<unsigned char>(ch) >= 0x80);
}

// Can start a label name (not a digit)
inline bool IsLabelStartChar(int ch) {
    return isalpha(ch) || ch == '_' || ch == '#' ||
           (static_cast<unsigned char>(ch) >= 0x80);
}

// Can start a variable name after % or $ (not a digit)
inline bool IsVarStartChar(int ch) {
    return isalpha(ch) || ch == '_' || ch == '$' ||
           (static_cast<unsigned char>(ch) >= 0x80);
}

// Word char for commands/keywords (ASCII only)
inline bool IsWordChar(int ch) {
    return isalnum(ch) || ch == '_';
}

// Check if // at position i is a real comment (not :// in URLs, not inside strings)
inline bool IsInlineComment(Accessor &styler, Sci_PositionU i) {
    return (i == 0 || styler[i - 1] != ':');
}

bool MatchWordList(const char* word, const char* const* list) {
    for (int i = 0; list[i]; i++) {
        if (_stricmp(word, list[i]) == 0) return true;
    }
    return false;
}

bool IsLineCommand(const char* word) {
    return MatchWordList(word, lineCommands);
}

bool IsIfKeyword(const char* word) {
    return MatchWordList(word, ifKeywords);
}

// Check if line is empty or whitespace only
bool IsEmptyLine(Accessor &styler, Sci_Position line) {
    Sci_Position startPos = styler.LineStart(line);
    Sci_Position endPos = styler.LineStart(line + 1);
    for (Sci_Position i = startPos; i < endPos; i++) {
        char ch = styler[i];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return false;
        }
    }
    return true;
}

// Check if line starts with !> or !>>
bool IsFileOpStartLine(Accessor &styler, Sci_Position line) {
    Sci_Position startPos = styler.LineStart(line);
    Sci_Position endPos = styler.LineStart(line + 1);
    Sci_Position i = startPos;
    while (i < endPos && (styler[i] == ' ' || styler[i] == '\t')) {
        i++;
    }
    if (i < endPos && styler[i] == '!') {
        i++;
        if (i < endPos && styler[i] == '>') {
            return true;
        }
    }
    return false;
}

// Detect markdown mode: first non-empty line starts with #
bool DetectMarkdownMode(Accessor &styler, Sci_PositionU docEnd) {
    Sci_Position numLines = styler.GetLine(docEnd > 0 ? docEnd - 1 : 0);
    for (Sci_Position line = 0; line <= numLines && line < 10; line++) {
        Sci_Position startPos = styler.LineStart(line);
        Sci_Position endPos = styler.LineStart(line + 1);
        for (Sci_Position i = startPos; i < endPos; i++) {
            char ch = styler[i];
            if (ch == ' ' || ch == '\t') continue;
            if (ch == '\r' || ch == '\n') break; // empty line, try next
            return (ch == '#');
        }
    }
    return false;
}

// Style a variable expansion: (%var), (%var proc), (%var proc:param), (%"lit" proc)
void StyleVarExpand(Accessor &styler, Sci_PositionU &i, Sci_PositionU lineEndPos) {
    // Style (% as delimiter
    styler.ColourTo(i + 1, SCE_CPS_VAREXPAND_DELIM);
    i += 2;

    // Check if next char is a quote (string literal inside variable)
    if (i < lineEndPos && (styler[i] == '"' || styler[i] == '\'')) {
        char quote = styler[i];
        // Style opening quote as delimiter
        styler.ColourTo(i, SCE_CPS_VAREXPAND_DELIM);
        i++;
        // Style string content as CONTENT
        while (i < lineEndPos && styler[i] != quote) {
            styler.ColourTo(i, SCE_CPS_VAREXPAND_CONTENT);
            i++;
        }
        // Style closing quote as delimiter
        if (i < lineEndPos && styler[i] == quote) {
            styler.ColourTo(i, SCE_CPS_VAREXPAND_DELIM);
            i++;
        }
    } else {
        // Regular variable name - find end (until space or ))
        Sci_PositionU varEnd = i;
        while (varEnd < lineEndPos && styler[varEnd] != ')' && styler[varEnd] != ' ') {
            varEnd++;
        }
        if (varEnd > i) {
            styler.ColourTo(varEnd - 1, SCE_CPS_VAREXPAND_DELIM);
            i = varEnd;
        }
    }

    // Content after variable/string (processor, default value, etc.)
    while (i < lineEndPos && styler[i] != ')') {
        styler.ColourTo(i, SCE_CPS_VAREXPAND_CONTENT);
        i++;
    }

    // Closing )
    if (i < lineEndPos && styler[i] == ')') {
        styler.ColourTo(i, SCE_CPS_VAREXPAND_DELIM);
        i++;
    }
}

// Style variables (%var and (%...)) inline in FILEOP/FILEREAD blocks
// In FILEREAD context, bare %var uses VAREXPAND_DELIM (red on yellow) for better contrast
void StyleBlockContent(Accessor &styler, Sci_PositionU &i, Sci_PositionU lineEndPos, int baseStyle) {
    int varStyle = (baseStyle == SCE_CPS_FILEREAD || baseStyle == SCE_CPS_FILEOP) ? SCE_CPS_VAREXPAND_DELIM : SCE_CPS_VARIABLE;
    while (i < lineEndPos) {
        if (i + 1 < lineEndPos && styler[i] == '/' && styler[i + 1] == '/' && IsInlineComment(styler, i)) {
            styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
            i = lineEndPos;
            break;
        } else if (i + 1 < lineEndPos && styler[i] == '(' && styler[i + 1] == '%') {
            StyleVarExpand(styler, i, lineEndPos);
        } else if (styler[i] == '%' && i + 1 < lineEndPos && IsVarStartChar(styler[i + 1])) {
            Sci_PositionU varEnd = i + 1;
            while (varEnd < lineEndPos && IsVarNameChar(styler[varEnd])) varEnd++;
            styler.ColourTo(varEnd - 1, varStyle);
            i = varEnd;
        } else {
            styler.ColourTo(i, baseStyle);
            i++;
        }
    }
}

void ColouriseCPSDoc(
    Sci_PositionU startPos,
    Sci_Position length,
    int initStyle,
    WordList *keywordlists[],
    Accessor &styler) {

    Sci_PositionU endPos = startPos + length;

    // Detect markdown mode from first non-empty line of document
    bool markdownMode = DetectMarkdownMode(styler, endPos);

    // Determine if we're inside a !> block by checking previous lines
    Sci_Position startLine = styler.GetLine(startPos);
    bool inFileOpBlock = false;

    if (startLine > 0) {
        for (Sci_Position line = startLine - 1; line >= 0; line--) {
            if (IsEmptyLine(styler, line)) {
                inFileOpBlock = false;
                break;
            }
            if (IsFileOpStartLine(styler, line)) {
                inFileOpBlock = true;
                break;
            }
        }
    }

    styler.StartAt(startPos);
    styler.StartSegment(startPos);

    Sci_Position currentLine = startLine;
    Sci_PositionU lineStartPos = styler.LineStart(currentLine);
    Sci_PositionU i = startPos;

    while (i < endPos) {
        // Get current line boundaries
        Sci_PositionU lineEndPos = styler.LineStart(currentLine + 1);
        if (lineEndPos > endPos) lineEndPos = endPos;

        // Check if this is an empty line
        bool isEmptyLine = true;
        for (Sci_PositionU j = lineStartPos; j < lineEndPos; j++) {
            char ch = styler[j];
            if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
                isEmptyLine = false;
                break;
            }
        }

        if (isEmptyLine) {
            // Empty line ends file op block
            inFileOpBlock = false;
            styler.ColourTo(lineEndPos - 1, SCE_CPS_DEFAULT);
            i = lineEndPos;
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // Find first non-whitespace character
        Sci_PositionU linePos = lineStartPos;
        while (linePos < lineEndPos && (styler[linePos] == ' ' || styler[linePos] == '\t')) {
            linePos++;
        }

        char firstChar = (linePos < lineEndPos) ? styler[linePos] : '\0';
        char secondChar = (linePos + 1 < lineEndPos) ? styler[linePos + 1] : '\0';

        // ===== // line comment at line start =====
        if (firstChar == '/' && secondChar == '/') {
            inFileOpBlock = false;
            styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
            i = lineEndPos;
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== HTML comment: <! at line start =====
        if (firstChar == '<' && secondChar == '!') {
            inFileOpBlock = false;
            styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
            i = lineEndPos;
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== # at line start: comment or header =====
        if (firstChar == '#') {
            inFileOpBlock = false;
            if (markdownMode) {
                styler.ColourTo(lineEndPos - 1, SCE_CPS_HEADER);
            } else {
                styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
            }
            i = lineEndPos;
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== Code fence: ~~~ or ``` =====
        if (linePos + 2 < lineEndPos &&
            ((firstChar == '~' && styler[linePos + 1] == '~' && styler[linePos + 2] == '~') ||
             (firstChar == '`' && styler[linePos + 1] == '`' && styler[linePos + 2] == '`'))) {
            inFileOpBlock = false;
            styler.ColourTo(lineEndPos - 1, SCE_CPS_HEADER);
            i = lineEndPos;
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== Label definition at line start: :labelname =====
        if (firstChar == ':' && IsLabelStartChar(secondChar)) {
            inFileOpBlock = false;
            // Style leading whitespace
            i = lineStartPos;
            while (i < linePos) {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            // Find end of label
            Sci_PositionU labelEnd = linePos + 1;
            while (labelEnd < lineEndPos && IsLabelChar(styler[labelEnd])) {
                labelEnd++;
            }
            // Include one extra char as padding (dark gray background extends)
            if (labelEnd < lineEndPos) {
                labelEnd++;
            }
            styler.ColourTo(labelEnd - 1, SCE_CPS_LABEL);
            i = labelEnd;
            // Rest of line: check for // comment
            while (i < lineEndPos) {
                if (i + 1 < lineEndPos && styler[i] == '/' && styler[i + 1] == '/') {
                    styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
                    i = lineEndPos;
                    break;
                }
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== !< extract shortform (single line) =====
        if (firstChar == '!' && secondChar == '<') {
            inFileOpBlock = false;
            i = lineStartPos;
            while (i < linePos) {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            // Style !< as COMMAND
            styler.ColourTo(i + 1, SCE_CPS_COMMAND);
            i += 2;
            // Optional keyword directly after: json, xml
            if (i < lineEndPos && isalpha(styler[i])) {
                Sci_PositionU kwEnd = i;
                while (kwEnd < lineEndPos && isalpha(styler[kwEnd])) kwEnd++;
                styler.ColourTo(kwEnd - 1, SCE_CPS_COMMAND);
                i = kwEnd;
            }
            // Rest of line as FILEREAD with variable expansion
            StyleBlockContent(styler, i, lineEndPos, SCE_CPS_FILEREAD);
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== !> or !>> file/HTTP write block (multiline until empty line) =====
        if (firstChar == '!' && secondChar == '>') {
            inFileOpBlock = true;
            i = lineStartPos;
            while (i < linePos) {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            // Style !> or !>> as COMMAND
            styler.ColourTo(i + 1, SCE_CPS_COMMAND);
            i += 2;
            if (i < lineEndPos && styler[i] == '>') {
                styler.ColourTo(i, SCE_CPS_COMMAND);
                i++;
            }
            // Rest of line as FILEOP with variable expansion
            StyleBlockContent(styler, i, lineEndPos, SCE_CPS_FILEOP);
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== Inside !> block continuation lines =====
        if (inFileOpBlock) {
            i = lineStartPos;
            StyleBlockContent(styler, i, lineEndPos, SCE_CPS_FILEOP);
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== > or >> output =====
        if (firstChar == '>') {
            i = lineStartPos;
            while (i < linePos) {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            bool isDoubleGt = (i + 1 < lineEndPos && styler[i + 1] == '>');
            styler.ColourTo(i, SCE_CPS_COMMAND);
            i++;
            if (isDoubleGt) {
                styler.ColourTo(i, SCE_CPS_COMMAND);
                i++;
                // >> : no space gap, background starts immediately
            } else {
                // > : space as separator (no background)
                if (i < lineEndPos && styler[i] == ' ') {
                    styler.ColourTo(i, SCE_CPS_DEFAULT);
                    i++;
                }
            }
            // Rest of line: output text with color tags, variables, comments (eolfilled)
            while (i < lineEndPos) {
                if (i + 1 < lineEndPos && styler[i] == '/' && styler[i + 1] == '/') {
                    styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
                    i = lineEndPos;
                    break;
                } else if (styler[i] == '[') {
                    // Color tag: [...] - includes [red], [#RGB], [/color], [bold], [cls], escape sequences
                    Sci_PositionU tagEnd = i + 1;
                    while (tagEnd < lineEndPos && styler[tagEnd] != ']') tagEnd++;
                    if (tagEnd < lineEndPos) tagEnd++;
                    styler.ColourTo(tagEnd - 1, SCE_CPS_COLORTAG);
                    i = tagEnd;
                } else if (i + 1 < lineEndPos && styler[i] == '(' && styler[i + 1] == '%') {
                    StyleVarExpand(styler, i, lineEndPos);
                } else {
                    styler.ColourTo(i, SCE_CPS_OUTPUT);
                    i++;
                }
            }
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== %varname or $varname assignment at line start =====
        if ((firstChar == '%' || firstChar == '$') &&
            (IsVarStartChar(secondChar) || isdigit(secondChar))) {
            i = lineStartPos;
            while (i < linePos) {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            // Variable name (including the % or $ prefix)
            i++; // skip % or $
            while (i < lineEndPos && IsVarNameChar(styler[i])) i++;
            styler.ColourTo(i - 1, SCE_CPS_VARIABLE);
            // Skip whitespace
            while (i < lineEndPos && (styler[i] == ' ' || styler[i] == '\t')) {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
            }
            // Optional = operator
            if (i < lineEndPos && styler[i] == '=') {
                styler.ColourTo(i, SCE_CPS_OPERATOR);
                i++;
                while (i < lineEndPos && (styler[i] == ' ' || styler[i] == '\t')) {
                    styler.ColourTo(i, SCE_CPS_DEFAULT);
                    i++;
                }
            }
            // Value: default (black), only "..." strings highlighted, plus variables/comments
            while (i < lineEndPos) {
                if (i + 1 < lineEndPos && styler[i] == '/' && styler[i + 1] == '/' && IsInlineComment(styler, i)) {
                    styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
                    i = lineEndPos;
                    break;
                } else if (styler[i] == '"' || styler[i] == '\'') {
                    char quote = styler[i];
                    Sci_PositionU strEnd = i + 1;
                    while (strEnd < lineEndPos && styler[strEnd] != quote) strEnd++;
                    if (strEnd < lineEndPos) strEnd++;
                    styler.ColourTo(strEnd - 1, SCE_CPS_STRING);
                    i = strEnd;
                } else if (i + 1 < lineEndPos && styler[i] == '(' && styler[i + 1] == '%') {
                    StyleVarExpand(styler, i, lineEndPos);
                } else {
                    styler.ColourTo(i, SCE_CPS_DEFAULT);
                    i++;
                }
            }
            currentLine++;
            lineStartPos = lineEndPos;
            continue;
        }

        // ===== Regular line: word at start or other content =====
        i = lineStartPos;

        // Style leading whitespace
        while (i < lineEndPos && (styler[i] == ' ' || styler[i] == '\t')) {
            styler.ColourTo(i, SCE_CPS_DEFAULT);
            i++;
        }

        // Check for word at line start (command or keyword)
        if (i < lineEndPos && isalpha(styler[i])) {
            Sci_PositionU wordStart = i;
            while (i < lineEndPos && IsWordChar(styler[i])) i++;
            // Handle pause$ specially
            if (i < lineEndPos && styler[i] == '$') i++;

            char word[64];
            Sci_PositionU wordLen = i - wordStart;
            if (wordLen >= sizeof(word)) wordLen = sizeof(word) - 1;
            for (Sci_PositionU j = 0; j < wordLen; j++) {
                word[j] = static_cast<char>(tolower(styler[wordStart + j]));
            }
            word[wordLen] = '\0';

            // Determine style based on keyword type
            if (IsIfKeyword(word)) {
                styler.ColourTo(i - 1, SCE_CPS_KEYWORD);
            } else if (IsLineCommand(word)) {
                styler.ColourTo(i - 1, SCE_CPS_COMMAND);
            } else {
                styler.ColourTo(i - 1, SCE_CPS_DEFAULT);
            }
        }

        // Rest of line: inline elements
        while (i < lineEndPos) {
            char ch = styler[i];
            char chNext = (i + 1 < lineEndPos) ? styler[i + 1] : '\0';

            // Inline comment: // (but not :// in URLs)
            if (ch == '/' && chNext == '/' && IsInlineComment(styler, i)) {
                styler.ColourTo(lineEndPos - 1, SCE_CPS_COMMENT);
                i = lineEndPos;
                break;
            }

            // Whitespace
            if (ch == ' ' || ch == '\t') {
                styler.ColourTo(i, SCE_CPS_DEFAULT);
                i++;
                continue;
            }

            // Relative jump: +N (forward, 1-32)
            if (ch == '+' && isdigit(chNext)) {
                Sci_PositionU jumpStart = i;
                i++;
                while (i < lineEndPos && isdigit(styler[i])) i++;
                styler.ColourTo(i - 1, SCE_CPS_JUMP_REL);
                continue;
            }

            // Operators: ==, ~=, =, >, <
            if (ch == '=' && chNext == '=') {
                styler.ColourTo(i + 1, SCE_CPS_OPERATOR);
                i += 2;
                continue;
            }
            if (ch == '~' && chNext == '=') {
                styler.ColourTo(i + 1, SCE_CPS_OPERATOR);
                i += 2;
                continue;
            }
            if (ch == '=' || ch == '>' || ch == '<') {
                styler.ColourTo(i, SCE_CPS_OPERATOR);
                i++;
                continue;
            }

            // Label reference: :label (after whitespace)
            if (ch == ':' && IsLabelStartChar(chNext) &&
                (i == lineStartPos || styler[i - 1] == ' ' || styler[i - 1] == '\t')) {
                Sci_PositionU labelEnd = i + 1;
                while (labelEnd < lineEndPos && IsLabelChar(styler[labelEnd])) labelEnd++;
                // Include one extra char as padding
                if (labelEnd < lineEndPos) labelEnd++;
                styler.ColourTo(labelEnd - 1, SCE_CPS_LABEL);
                i = labelEnd;
                continue;
            }

            // Variable expansion: (%...)
            if (ch == '(' && chNext == '%') {
                StyleVarExpand(styler, i, lineEndPos);
                continue;
            }

            // Variable reference: %var (without parentheses, e.g. in IF)
            if (ch == '%' && IsVarStartChar(chNext)) {
                Sci_PositionU varEnd = i + 1;
                while (varEnd < lineEndPos && IsVarNameChar(styler[varEnd])) varEnd++;
                styler.ColourTo(varEnd - 1, SCE_CPS_VARIABLE);
                i = varEnd;
                continue;
            }

            // String: "..." or '...'
            if (ch == '"' || ch == '\'') {
                char quote = ch;
                Sci_PositionU strEnd = i + 1;
                while (strEnd < lineEndPos && styler[strEnd] != quote) strEnd++;
                if (strEnd < lineEndPos) strEnd++;
                styler.ColourTo(strEnd - 1, SCE_CPS_STRING);
                i = strEnd;
                continue;
            }

            // Keywords inline: if, ifnot, then, else, undef, next, in
            if (isalpha(ch)) {
                Sci_PositionU wordStart = i;
                while (i < lineEndPos && IsWordChar(styler[i])) i++;
                // Handle pause$ specially
                if (i < lineEndPos && styler[i] == '$') i++;

                char word[64];
                Sci_PositionU wordLen = i - wordStart;
                if (wordLen >= sizeof(word)) wordLen = sizeof(word) - 1;
                for (Sci_PositionU j = 0; j < wordLen; j++) {
                    word[j] = static_cast<char>(tolower(styler[wordStart + j]));
                }
                word[wordLen] = '\0';

                if (IsIfKeyword(word)) {
                    styler.ColourTo(i - 1, SCE_CPS_KEYWORD);
                } else {
                    styler.ColourTo(i - 1, SCE_CPS_DEFAULT);
                }
                continue;
            }

            // Default
            styler.ColourTo(i, SCE_CPS_DEFAULT);
            i++;
        }

        currentLine++;
        lineStartPos = lineEndPos;
    }
}

void FoldCPSDoc(Sci_PositionU startPos, Sci_Position length, int, WordList *[], Accessor &styler) {
    Sci_Position lineCurrent = styler.GetLine(startPos);
    Sci_PositionU endPos = startPos + length;
    int levelCurrent = SC_FOLDLEVELBASE;

    if (lineCurrent > 0) {
        levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
    }

    int levelNext = levelCurrent;

    for (Sci_PositionU i = startPos; i < endPos; i++) {
        char ch = styler[i];
        if (ch == '\n' || (ch == '\r' && styler.SafeGetCharAt(i + 1) != '\n')) {
            int lev = levelCurrent;
            if (levelNext > levelCurrent) {
                lev |= SC_FOLDLEVELHEADERFLAG;
            }
            styler.SetLevel(lineCurrent, lev | (levelNext << 16));
            lineCurrent++;
            levelCurrent = levelNext;
        }
    }
}

const char * const cpsWordListDesc[] = {
    "Commands",
    0
};

}

extern const LexerModule lmCPS(SCLEX_CPS, ColouriseCPSDoc, "cps", FoldCPSDoc, cpsWordListDesc);
