// Scintilla source code edit control
/** @file LexF2T.cxx
 ** Lexer for F2T (Flex2Text) files.
 ** Line style is determined by the last 2 "characters" of each line.
 ** Characters: Space (0x20), Tab (0x09), NBSpace
 ** NBSpace can be: Latin-1 (0xA0), OEM (0xFF), or UTF-8 (0xC2 0xA0)
 **/
// Copyright 2024

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

// Custom SCLEX for F2T
#define SCLEX_F2T 201

// Style definitions for F2T - based on last 2 chars of line
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

namespace {

// Character types for F2T line endings
enum F2TChar {
    F2T_NONE = 0,
    F2T_SPACE = 1,    // Regular space (0x20)
    F2T_TAB = 2,      // Tab (0x09)
    F2T_NBSP = 3      // Non-breaking space (any encoding)
};

// Check if position has UTF-8 NBSP (0xC2 0xA0)
// Returns true if bytes at pos and pos+1 form UTF-8 NBSP
bool IsUtf8Nbsp(Accessor &styler, Sci_PositionU pos, Sci_PositionU endPos) {
    if (pos + 1 < endPos) {
        unsigned char b1 = styler[pos];
        unsigned char b2 = styler[pos + 1];
        return (b1 == 0xC2 && b2 == 0xA0);
    }
    return false;
}

// Get the F2T character type and byte length at a position (reading forward)
// Returns the character type and sets byteLen to 1 or 2
F2TChar GetCharTypeForward(Accessor &styler, Sci_PositionU pos, Sci_PositionU endPos, int &byteLen) {
    if (pos >= endPos) {
        byteLen = 0;
        return F2T_NONE;
    }

    unsigned char ch = styler[pos];

    // Check for UTF-8 NBSP (2 bytes: 0xC2 0xA0)
    if (ch == 0xC2 && IsUtf8Nbsp(styler, pos, endPos)) {
        byteLen = 2;
        return F2T_NBSP;
    }

    byteLen = 1;

    if (ch == ' ') return F2T_SPACE;
    if (ch == '\t') return F2T_TAB;
    if (ch == 0xA0 || ch == 0xFF) return F2T_NBSP;  // Latin-1 or OEM NBSP

    return F2T_NONE;
}

// Get the last F2T character before position (reading backward)
// Returns the character type and sets startPos to where it begins
F2TChar GetCharTypeBackward(Accessor &styler, Sci_PositionU pos, Sci_PositionU lineStart, Sci_PositionU &charStart) {
    if (pos <= lineStart) {
        charStart = pos;
        return F2T_NONE;
    }

    unsigned char ch = styler[pos - 1];

    // Check for UTF-8 NBSP: if last byte is 0xA0, check if preceded by 0xC2
    if (ch == 0xA0 && pos >= lineStart + 2) {
        unsigned char prev = styler[pos - 2];
        if (prev == 0xC2) {
            charStart = pos - 2;
            return F2T_NBSP;
        }
    }

    charStart = pos - 1;

    if (ch == ' ') return F2T_SPACE;
    if (ch == '\t') return F2T_TAB;
    if (ch == 0xA0 || ch == 0xFF) return F2T_NBSP;  // Latin-1 or OEM NBSP (single byte)

    return F2T_NONE;
}

// Determine style based on last 2 logical characters
// If makeBig is true, return the BIG variant (120% size)
int GetLineStyle(F2TChar char1, F2TChar char2, bool makeBig) {
    // char1 is second-to-last, char2 is last character

    if (char1 == F2T_TAB) {
        if (char2 == F2T_NBSP)   return makeBig ? SCE_F2T_NOKEND_BIG : SCE_F2T_NOKEND;
        if (char2 == F2T_SPACE)  return makeBig ? SCE_F2T_DKBLUE_BIG : SCE_F2T_DKBLUE;
        if (char2 == F2T_TAB)    return makeBig ? SCE_F2T_LTBLUE_BIG : SCE_F2T_LTBLUE;
    }
    else if (char1 == F2T_NBSP) {
        if (char2 == F2T_SPACE)  return makeBig ? SCE_F2T_OKEND_BIG : SCE_F2T_OKEND;
        if (char2 == F2T_TAB)    return makeBig ? SCE_F2T_VARI1_BIG : SCE_F2T_VARI1;
        if (char2 == F2T_NBSP)   return makeBig ? SCE_F2T_VARI2_BIG : SCE_F2T_VARI2;
    }
    else if (char1 == F2T_SPACE) {
        if (char2 == F2T_NBSP)   return makeBig ? SCE_F2T_HOKEND_BIG : SCE_F2T_HOKEND;
        if (char2 == F2T_TAB)    return makeBig ? SCE_F2T_LTGRAY_BIG : SCE_F2T_LTGRAY;
    }

    return SCE_F2T_DEFAULT;
}

void ColouriseF2TDoc(
    Sci_PositionU startPos,
    Sci_Position length,
    int /*initStyle*/,
    WordList * /*keywordlists*/[],
    Accessor &styler) {

    Sci_PositionU endPos = startPos + length;

    // Process line by line
    Sci_Position currentLine = styler.GetLine(startPos);
    Sci_PositionU lineStartPos = styler.LineStart(currentLine);

    styler.StartAt(startPos);
    styler.StartSegment(startPos);

    while (lineStartPos < endPos) {
        Sci_PositionU lineEndPos = styler.LineStart(currentLine + 1);
        if (lineEndPos > endPos) {
            lineEndPos = endPos;
        }

        // Find actual line end (excluding CR/LF)
        Sci_PositionU contentEnd = lineEndPos;
        while (contentEnd > lineStartPos) {
            unsigned char ch = styler[contentEnd - 1];
            if (ch != '\r' && ch != '\n') {
                break;
            }
            contentEnd--;
        }

        // Get last 2-3 logical characters (handling UTF-8 NBSP)
        // 3rd char from end = NBS triggers BIG style (120% size)
        int lineStyle = SCE_F2T_DEFAULT;

        if (contentEnd > lineStartPos) {
            Sci_PositionU char2Start;
            F2TChar char2 = GetCharTypeBackward(styler, contentEnd, lineStartPos, char2Start);

            if (char2 != F2T_NONE && char2Start > lineStartPos) {
                Sci_PositionU char1Start;
                F2TChar char1 = GetCharTypeBackward(styler, char2Start, lineStartPos, char1Start);

                // Check for 3rd character (size modifier)
                bool makeBig = false;
                if (char1 != F2T_NONE && char1Start > lineStartPos) {
                    Sci_PositionU char0Start;
                    F2TChar char0 = GetCharTypeBackward(styler, char1Start, lineStartPos, char0Start);
                    // NBS as 3rd char from end = BIG style
                    if (char0 == F2T_NBSP) {
                        makeBig = true;
                    }
                }

                lineStyle = GetLineStyle(char1, char2, makeBig);
            }
        }

        // Find first NBS in line content (not the control chars at end)
        // Text before NBS = DEFAULT, text from NBS = lineStyle
        Sci_PositionU colorStart = lineStartPos;

        if (lineStyle != SCE_F2T_DEFAULT && contentEnd > lineStartPos) {
            // Find where control chars start (last 2-3 chars)
            Sci_PositionU ctrlStart = contentEnd;
            Sci_PositionU char2Start;
            F2TChar char2 = GetCharTypeBackward(styler, contentEnd, lineStartPos, char2Start);
            if (char2 != F2T_NONE && char2Start > lineStartPos) {
                Sci_PositionU char1Start;
                F2TChar char1 = GetCharTypeBackward(styler, char2Start, lineStartPos, char1Start);
                if (char1 != F2T_NONE) {
                    ctrlStart = char1Start;
                    // Check for 3rd control char (BIG marker)
                    if (char1Start > lineStartPos) {
                        Sci_PositionU char0Start;
                        F2TChar char0 = GetCharTypeBackward(styler, char1Start, lineStartPos, char0Start);
                        if (char0 == F2T_NBSP) {
                            ctrlStart = char0Start;
                        }
                    }
                }
            }

            // Search for NBS before the control chars
            for (Sci_PositionU pos = lineStartPos; pos < ctrlStart; pos++) {
                unsigned char ch = styler[pos];
                // Check for UTF-8 NBSP (0xC2 0xA0)
                if (ch == 0xC2 && pos + 1 < ctrlStart && (unsigned char)styler[pos + 1] == 0xA0) {
                    colorStart = pos;
                    break;
                }
                // Check for Latin-1 or OEM NBSP
                if (ch == 0xA0 || ch == 0xFF) {
                    colorStart = pos;
                    break;
                }
            }
        }

        // Apply styles
        if (colorStart > lineStartPos) {
            // Text before NBS = DEFAULT
            styler.ColourTo(colorStart - 1, SCE_F2T_DEFAULT);
        }
        if (lineEndPos > colorStart) {
            // Text from NBS (or line start) = lineStyle
            styler.ColourTo(lineEndPos - 1, lineStyle);
        }

        currentLine++;
        lineStartPos = lineEndPos;
    }
}

void FoldF2TDoc(
    Sci_PositionU startPos,
    Sci_Position length,
    int /*initStyle*/,
    WordList * /*keywordlists*/[],
    Accessor &styler) {

    // Simple folding - no folding for F2T files
    Sci_PositionU endPos = startPos + length;
    Sci_Position lineCurrent = styler.GetLine(startPos);

    while (startPos < endPos) {
        Sci_PositionU lineEnd = styler.LineStart(lineCurrent + 1) - 1;
        int level = SC_FOLDLEVELBASE;
        styler.SetLevel(lineCurrent, level);
        lineCurrent++;
        startPos = lineEnd + 1;
    }
}

}

extern const LexerModule lmF2T(SCLEX_F2T, ColouriseF2TDoc, "f2t", FoldF2TDoc);
