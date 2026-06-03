// Scintilla source code edit control
/** @file LexF2P.cxx
 ** Lexer for Flex2PDF template files (.f2p).
 **
 ** Unlike LexF2T (Flex2Text, colours lines by their last two characters) this
 ** lexer handles the Flex2PDF template language: a line is normal text mixed
 ** with any number of inline [...] control codes, each with its own style.
 **
 ** Three code classes (see spec F2P_Lexer_Spec.md):
 **   1. F2P control codes  - [ + special char: [= [== [x [? [: [. [l [p ...
 **   2. format/block letters - [ + one letter, no ']' : [F [f [0 [S [P [7 ...
 **   3. VPE inline codes   - complete [ ... ] with a closing ']'
 **
 ** Design decisions where the spec is ambiguous (documented here):
 **   - [P/[Pp overlap (spec lists [Pp in both block-call and linefeed tables):
 **       [pp / [Pp / [PP (double p/P) -> LINEFEED, [P alone -> BLOCKCALL,
 **       [p alone -> LINEFEED.
 **   - CONDITION ([?...]) and VPECODE ([ ... ]) end colouring at the first ']'
 **       (not at segment end), so e.g. "[?\1]L,0,R,=,rtff-2,{\rtf...}" splits
 **       into CONDITION + default params + an embedded RTF block.
 **   - FORMAT / BLOCKCALL / LINEFEED shortcuts colour only the bracket+letter(s)
 **       (e.g. [F = 2 chars); the text up to the next '[' stays DEFAULT, matching
 **       "[FGesamtsumme[f".
 **   - COLUMN ([:X, / [.X,) colours only the prefix up to and including the first
 **       comma; the remaining cell text is scanned as DEFAULT (so %a/%g inside
 **       become VARIABLE).
 **   - Container codes (PDF/BLOCKDEF/VARIABLE-def/SETUP/OBJECT) colour the whole
 **       segment, with inline override for placeholders (%a %g %%%).
 **   - RTF: brace-balanced from {\rtf to the matching '}' or end of line.
 **   - Per-line restartable: every construct stays within one line (RTF ends at
 **       the line end at the latest), so Scintilla can re-lex from any line start.
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

// Custom SCLEX for F2P - high number to avoid conflicts (F2T=201, CPS=200)
#define SCLEX_F2P 202

// Style definitions for F2P (must match Styles.c)
#define SCE_F2P_DEFAULT    0   // body text (printed verbatim)
#define SCE_F2P_COMMENT    1   // [=/ ... comment line
#define SCE_F2P_HEADER     2   // [=) ... head/setup line (first line)
#define SCE_F2P_SETUP      3   // [= ... positioning/setup
#define SCE_F2P_PDF        4   // [=pdf, ... metadata/bookmarks
#define SCE_F2P_BLOCKDEF   5   // [==,X, ... template definition
#define SCE_F2P_OBJECT     6   // [x ... graphic object
#define SCE_F2P_COLUMN     7   // [: / [.  columns/tabs
#define SCE_F2P_CONDITION  8   // [? ... multi-column / space layout
#define SCE_F2P_FORMAT     9   // format shortcuts  [F [f [B [S [0 ...
#define SCE_F2P_BLOCKCALL 10   // block/template call  [P [7 [Z [I ...
#define SCE_F2P_LINEFEED  11   // [l [l- [pp [Pp [PP  (feed/page break)
#define SCE_F2P_VPECODE   12   // VPE inline code  [ ... ]  (with ])
#define SCE_F2P_VARIABLE  13   // [=%, definition + placeholders %a %g %%% [%x
#define SCE_F2P_RTF       14   // embedded RTF string  {\rtf1 ... }
#define SCE_F2P_FLOWBLOCK 15   // \==  ...  /==  keep-together marker
#define SCE_F2P_OPERATOR  16   // reserved (unused) - comma/coordinate separators

namespace {

// Format-shortcut letters: [F [f [U [u [Q [q [B [b [S [s [0 [&
inline bool IsFormatChar(int c) {
    return strchr("FfUuQqBbSs0&", c) != nullptr;
}

// Block/template-call letters: [P [I [E [N [L [Z [1..[9
inline bool IsBlockChar(int c) {
    return strchr("PIENLZ123456789", c) != nullptr;
}

// Does the document text at 'pos' begin with the literal string s (within limit)?
bool Matches(Accessor &styler, Sci_PositionU pos, const char *s, Sci_PositionU limit) {
    for (Sci_PositionU k = 0; s[k]; k++) {
        if (pos + k >= limit) return false;
        if (styler[pos + k] != s[k]) return false;
    }
    return true;
}

// Find first occurrence of ch in [from, to); returns (Sci_PositionU)-1 if absent.
Sci_PositionU FindChar(Accessor &styler, Sci_PositionU from, Sci_PositionU to, char ch) {
    for (Sci_PositionU k = from; k < to; k++) {
        if (styler[k] == ch) return k;
    }
    return (Sci_PositionU)-1;
}

// A placeholder begins at '%' followed by another '%' (path prefix %%%) or by an
// alphanumeric (page numbers %a %g and the like).
bool IsPlaceholderStart(Accessor &styler, Sci_PositionU i, Sci_PositionU limit) {
    if (i >= limit || styler[i] != '%') return false;
    if (i + 1 >= limit) return false;
    char n = styler[i + 1];
    return (n == '%') || (isalnum(static_cast<unsigned char>(n)) != 0);
}

// Colour a placeholder starting at i as VARIABLE; advances i past it.
void StylePlaceholder(Accessor &styler, Sci_PositionU &i, Sci_PositionU limit) {
    if (i + 1 < limit && styler[i + 1] == '%') {
        // run of '%' (covers %%% path prefix)
        Sci_PositionU end = i;
        while (end < limit && styler[end] == '%') end++;
        styler.ColourTo(end - 1, SCE_F2P_VARIABLE);
        i = end;
    } else {
        // %<alnum...>  e.g. %a %g
        Sci_PositionU end = i + 1;
        while (end < limit && isalnum(static_cast<unsigned char>(styler[end]))) end++;
        styler.ColourTo(end - 1, SCE_F2P_VARIABLE);
        i = end;
    }
}

// Colour an embedded RTF block starting at i ("{\rtf...") as RTF, brace-balanced
// up to the matching '}' or end of line. Advances i past the block.
void StyleRtf(Accessor &styler, Sci_PositionU &i, Sci_PositionU lineEnd) {
    int depth = 0;
    Sci_PositionU k = i;
    for (; k < lineEnd; k++) {
        char ch = styler[k];
        if (ch == '{') depth++;
        else if (ch == '}') {
            depth--;
            if (depth <= 0) { k++; break; }
        }
    }
    if (k > lineEnd) k = lineEnd;
    styler.ColourTo(k - 1, SCE_F2P_RTF);
    i = k;
}

// Colour a container segment [from, to) with baseStyle, overriding inline
// placeholders (%a %g %%%) and embedded RTF blocks.
void StyleContainer(Accessor &styler, Sci_PositionU from, Sci_PositionU to,
                    Sci_PositionU lineEnd, int baseStyle) {
    Sci_PositionU i = from;
    while (i < to) {
        if (Matches(styler, i, "{\\rtf", lineEnd)) {
            StyleRtf(styler, i, lineEnd);          // may run past 'to' to line end
        } else if (IsPlaceholderStart(styler, i, to)) {
            StylePlaceholder(styler, i, to);
        } else {
            styler.ColourTo(i, baseStyle);
            i++;
        }
    }
}

// Handle one '[' segment starting at i. Advances i to the position after the
// part this routine has coloured (the remainder, if any, is left to the caller).
void StyleBracket(Accessor &styler, Sci_PositionU &i, Sci_PositionU lineEnd) {
    // Segment end = next '[' or line end.
    Sci_PositionU segEnd = i + 1;
    while (segEnd < lineEnd && styler[segEnd] != '[') segEnd++;

    Sci_PositionU closePos = FindChar(styler, i + 1, segEnd, ']');
    bool hasClose = (closePos != (Sci_PositionU)-1);

    char c1 = (i + 1 < lineEnd) ? styler[i + 1] : '\0';
    char c2 = (i + 2 < lineEnd) ? styler[i + 2] : '\0';

    // --- Multi-char prefixes first (order matters, see spec 4b) ---
    if (Matches(styler, i + 1, "=pdf,", lineEnd)) {
        StyleContainer(styler, i, segEnd, lineEnd, SCE_F2P_PDF);
        i = (i < segEnd) ? segEnd : i + 1;
        return;
    }
    if (c1 == '=' && c2 == '=') {                       // [==,  template def
        StyleContainer(styler, i, segEnd, lineEnd, SCE_F2P_BLOCKDEF);
        i = segEnd;
        return;
    }
    if (c1 == '=' && c2 == '%') {                       // [=%,  variable def
        StyleContainer(styler, i, segEnd, lineEnd, SCE_F2P_VARIABLE);
        i = segEnd;
        return;
    }
    if (c1 == '=') {                                    // [= ... setup
        StyleContainer(styler, i, segEnd, lineEnd, SCE_F2P_SETUP);
        i = segEnd;
        return;
    }
    if (c1 == 'x' || c1 == 'X') {                       // [x ... object
        StyleContainer(styler, i, segEnd, lineEnd, SCE_F2P_OBJECT);
        i = segEnd;
        return;
    }
    if (c1 == '?') {                                    // [? ... condition
        Sci_PositionU end = hasClose ? closePos : segEnd - 1;
        styler.ColourTo(end, SCE_F2P_CONDITION);
        i = end + 1;
        return;
    }
    if (c1 == ':' || c1 == '.') {                       // [:X, / [.X, column
        Sci_PositionU comma = FindChar(styler, i + 1, segEnd, ',');
        if (comma != (Sci_PositionU)-1) {
            styler.ColourTo(comma, SCE_F2P_COLUMN);     // prefix incl. comma
            i = comma + 1;                              // caller scans the rest
        } else {
            styler.ColourTo(segEnd - 1, SCE_F2P_COLUMN);
            i = segEnd;
        }
        return;
    }
    if (c1 == 'l') {                                    // [l / [l-  linefeed
        Sci_PositionU end = i + 1;                      // '[' + 'l'
        if (i + 2 < lineEnd && styler[i + 2] == '-') end = i + 2;
        styler.ColourTo(end, SCE_F2P_LINEFEED);
        i = end + 1;
        return;
    }
    if (c1 == 'p' || c1 == 'P') {
        if (hasClose) {                                 // [p...] -> VPE inline
            styler.ColourTo(closePos, SCE_F2P_VPECODE);
            i = closePos + 1;
        } else if (c2 == 'p' || c2 == 'P') {            // [pp [Pp [PP -> linefeed
            styler.ColourTo(i + 2, SCE_F2P_LINEFEED);
            i = i + 3;
        } else if (c1 == 'P') {                         // [P alone -> block call
            styler.ColourTo(i + 1, SCE_F2P_BLOCKCALL);
            i = i + 2;
        } else {                                        // [p alone -> linefeed
            styler.ColourTo(i + 1, SCE_F2P_LINEFEED);
            i = i + 2;
        }
        return;
    }
    if (hasClose) {                                     // [ ... ] -> VPE inline
        styler.ColourTo(closePos, SCE_F2P_VPECODE);
        i = closePos + 1;
        return;
    }
    if (c1 == '%') {                                    // [%x -> variable
        Sci_PositionU end = i + 2;
        while (end < segEnd &&
               (isalnum(static_cast<unsigned char>(styler[end])) || styler[end] == '_'))
            end++;
        styler.ColourTo(end - 1, SCE_F2P_VARIABLE);
        i = end;
        return;
    }
    if (IsFormatChar(c1)) {                             // [F [0 [S ...
        styler.ColourTo(i + 1, SCE_F2P_FORMAT);
        i = i + 2;
        return;
    }
    if (IsBlockChar(c1)) {                              // [7 [Z [I ...
        styler.ColourTo(i + 1, SCE_F2P_BLOCKCALL);
        i = i + 2;
        return;
    }
    // Lone '[' or unknown
    styler.ColourTo(i, SCE_F2P_DEFAULT);
    i++;
}

// Skip leading whitespace of a line; returns position of first non-ws char.
Sci_PositionU SkipLineWhitespace(Accessor &styler, Sci_PositionU from, Sci_PositionU lineEnd) {
    Sci_PositionU p = from;
    while (p < lineEnd && (styler[p] == ' ' || styler[p] == '\t')) p++;
    return p;
}

void ColouriseF2PDoc(
    Sci_PositionU startPos,
    Sci_Position length,
    int /*initStyle*/,
    WordList * /*keywordlists*/[],
    Accessor &styler) {

    Sci_PositionU endPos = startPos + length;

    styler.StartAt(startPos);
    styler.StartSegment(startPos);

    Sci_Position currentLine = styler.GetLine(startPos);
    Sci_PositionU i = startPos;   // Scintilla guarantees startPos is a line start

    while (i < endPos) {
        Sci_PositionU lineStartPos = styler.LineStart(currentLine);
        Sci_PositionU lineEnd = styler.LineStart(currentLine + 1);
        if (lineEnd > endPos) lineEnd = endPos;

        // Always (re)start a line at its beginning for restartability.
        i = lineStartPos;

        Sci_PositionU p = SkipLineWhitespace(styler, lineStartPos, lineEnd);

        // ===== 4a line special cases (whole line in one style) =====
        if (Matches(styler, p, "[=)", lineEnd)) {
            styler.ColourTo(lineEnd - 1, SCE_F2P_HEADER);
            i = lineEnd; currentLine++; continue;
        }
        if (Matches(styler, p, "[=/", lineEnd)) {
            styler.ColourTo(lineEnd - 1, SCE_F2P_COMMENT);
            i = lineEnd; currentLine++; continue;
        }
        if (Matches(styler, p, "\\==", lineEnd) || Matches(styler, p, "/==", lineEnd)) {
            styler.ColourTo(lineEnd - 1, SCE_F2P_FLOWBLOCK);
            i = lineEnd; currentLine++; continue;
        }

        // ===== 4b segment scan =====
        while (i < lineEnd) {
            char ch = styler[i];
            if (ch == '[') {
                StyleBracket(styler, i, lineEnd);
            } else if (Matches(styler, i, "{\\rtf", lineEnd)) {
                StyleRtf(styler, i, lineEnd);
            } else if (IsPlaceholderStart(styler, i, lineEnd)) {
                StylePlaceholder(styler, i, lineEnd);
            } else {
                styler.ColourTo(i, SCE_F2P_DEFAULT);
                i++;
            }
        }

        currentLine++;
    }
}

void FoldF2PDoc(Sci_PositionU startPos, Sci_Position length, int, WordList *[], Accessor &styler) {
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

const char * const f2pWordListDesc[] = {
    0
};

}

extern const LexerModule lmF2P(SCLEX_F2P, ColouriseF2PDoc, "f2p", FoldF2PDoc, f2pWordListDesc);
