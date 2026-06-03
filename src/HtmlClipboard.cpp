/******************************************************************************
*
* TE - Text Editor
*
* HtmlClipboard.cpp - "Copy with HTML syntax highlighting"
*
* Places both CF_UNICODETEXT and CF_HTML ("HTML Format") on the clipboard so
* rich editors (Outlook, Word, Gmail, Teams) paste with syntax colors.
*
* Colors/bold/italic are read live from Scintilla – no hardcoded mapping.
*
******************************************************************************/
#include <windows.h>
#include <string>
#include <cstdio>
#include <cstring>

#include "scintilla.h"
#include "SciCompat.h"

extern "C" {
#include "TE.h"
}

// ── anonymous namespace – internal helpers ───────────────────────────────────
namespace {

// ── UTF-8 → UTF-16 ──────────────────────────────────────────────────────────
std::wstring Utf8ToWide(const char* p, int cb)
{
    if (cb <= 0) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, p, cb, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, p, cb, &w[0], n);
    return w;
}

// ── Scintilla COLORREF (0x00BBGGRR) → "#RRGGBB" appended to out ─────────────
void AppendColorHex(std::string& out, COLORREF c)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X",
        (unsigned)(c        & 0xFF),
        (unsigned)((c >> 8) & 0xFF),
        (unsigned)((c >>16) & 0xFF));
    out += buf;
}

// ── HTML-escape one byte (non-ASCII bytes pass through for UTF-8 HTML) ───────
void AppendEscaped(std::string& out, unsigned char ch)
{
    switch (ch) {
        case '&': out += "&amp;";  return;
        case '<': out += "&lt;";   return;
        case '>': out += "&gt;";   return;
        case '"': out += "&quot;"; return;
        default:  out += (char)ch; return;
    }
}

// ── Build the CF_HTML clipboard payload with correct byte-offset header ───────
//
// CF_HTML format (https://learn.microsoft.com/windows/win32/dataxchg/html-clipboard-format):
//   The entire payload is a UTF-8 string starting with a text header whose
//   fields give byte offsets measured from payload byte 0.
//
// Header line lengths (8-digit zero-padded values):
//   "Version:0.9\r\n"              = 13 bytes
//   "StartHTML:00000000\r\n"       = 20 bytes
//   "EndHTML:00000000\r\n"         = 18 bytes
//   "StartFragment:00000000\r\n"   = 24 bytes
//   "EndFragment:00000000\r\n"     = 22 bytes
//                            total = 97 bytes  ← HEADER_LEN
//
// Layout:
//   [97-byte header]
//   <html><head><meta charset="utf-8"></head><body>   (47 bytes)
//   <!--StartFragment-->                               (20 bytes) ← startFragment
//   [fragment]
//   <!--EndFragment-->                                 (18 bytes) ← endFragment
//   </body></html>                                     (14 bytes)
//
std::string BuildCfHtml(const std::string& fragment)
{
    constexpr size_t HEADER_LEN = 97;

    // PREFIX: everything up to and including <!--StartFragment-->
    // startFragment byte is the first byte of [fragment] (right after PREFIX).
    static const char PREFIX[] =
        "<html><head><meta charset=\"utf-8\"></head><body>"  // 47 bytes
        "<!--StartFragment-->";                               // 20 bytes  → 67 total

    // SUFFIX: everything from <!--EndFragment--> to end.
    // endFragment byte is the '<' of <!--EndFragment-->.
    static const char SUFFIX[] =
        "<!--EndFragment-->"   // 18 bytes
        "</body></html>";      // 14 bytes  → 32 total

    constexpr size_t PFX = sizeof(PREFIX) - 1;  // 67
    constexpr size_t SFX = sizeof(SUFFIX) - 1;  // 32

    const size_t startHTML     = HEADER_LEN;
    const size_t startFragment = HEADER_LEN + PFX;
    const size_t endFragment   = startFragment + fragment.size();
    const size_t endHTML       = endFragment   + SFX;

    char hdr[128];
    snprintf(hdr, sizeof(hdr),
        "Version:0.9\r\n"
        "StartHTML:%08llu\r\n"
        "EndHTML:%08llu\r\n"
        "StartFragment:%08llu\r\n"
        "EndFragment:%08llu\r\n",
        (unsigned long long)startHTML,
        (unsigned long long)endHTML,
        (unsigned long long)startFragment,
        (unsigned long long)endFragment);

    std::string payload;
    payload.reserve(HEADER_LEN + PFX + fragment.size() + SFX + 1);
    payload  = hdr;       // strlen(hdr) == HEADER_LEN for values < 10^8
    payload += PREFIX;
    payload += fragment;
    payload += SUFFIX;
    return payload;
}

// ── Per-style attribute cache (styles 0..255, populated lazily) ──────────────
struct StyleInfo {
    bool     loaded;
    COLORREF fore, back;
    bool     bold, italic;
};
StyleInfo g_styleCache[256];

void EnsureStyle(HWND hwndSci, int s)
{
    if (s < 0 || s > 255 || g_styleCache[s].loaded) return;
    g_styleCache[s].fore   = (COLORREF)SendMessage(hwndSci, SCI_STYLEGETFORE,   s, 0);
    g_styleCache[s].back   = (COLORREF)SendMessage(hwndSci, SCI_STYLEGETBACK,   s, 0);
    g_styleCache[s].bold   =    (BOOL) SendMessage(hwndSci, SCI_STYLEGETBOLD,   s, 0) != 0;
    g_styleCache[s].italic =    (BOOL) SendMessage(hwndSci, SCI_STYLEGETITALIC, s, 0) != 0;
    g_styleCache[s].loaded = true;
}

} // namespace


// ── Public entry point ───────────────────────────────────────────────────────
extern "C" BOOL CopySelectionAsHtml(HWND hwndSci, HWND hwndOwner)
{
    // ── A. Selection bounds ───────────────────────────────────────────────────
    auto selStart = (Sci_Position)SendMessage(hwndSci, SCI_GETSELECTIONSTART, 0, 0);
    auto selEnd   = (Sci_Position)SendMessage(hwndSci, SCI_GETSELECTIONEND,   0, 0);
    if (selStart >= selEnd) return FALSE;

    auto selLen = (size_t)(selEnd - selStart);

    // ── B. UTF-8 text (for both CF_UNICODETEXT and HTML body) ────────────────
    std::string utf8(selLen + 1, '\0');
    {
        Sci_TextRange tr{};
        tr.chrg.cpMin = selStart;
        tr.chrg.cpMax = selEnd;
        tr.lpstrText  = &utf8[0];
        SendMessage(hwndSci, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
    }
    utf8.resize(selLen);

    std::wstring wide = Utf8ToWide(utf8.c_str(), (int)selLen);
    if (wide.empty()) return FALSE;

    // ── C. Default style → <pre> container attributes ────────────────────────
    memset(g_styleCache, 0, sizeof(g_styleCache));
    EnsureStyle(hwndSci, STYLE_DEFAULT);

    char fontName[128] = "Consolas";
    SendMessage(hwndSci, SCI_STYLEGETFONT, STYLE_DEFAULT, (LPARAM)fontName);
    int fontSize = (int)SendMessage(hwndSci, SCI_STYLEGETSIZE, STYLE_DEFAULT, 0);
    if (fontSize <= 0) fontSize = 10;
    COLORREF defBg = g_styleCache[STYLE_DEFAULT].back;

    // ── D. Build HTML fragment ────────────────────────────────────────────────
    std::string frag;
    frag.reserve(selLen * 7);

    // <pre> opening — carries background, font, line-height
    {
        std::string bgHex;
        AppendColorHex(bgHex, defBg);
        char pre[512];
        snprintf(pre, sizeof(pre),
            "<pre style=\""
            "font-family:'%s',Consolas,monospace;"
            "font-size:%dpt;"
            "background:%s;"
            "margin:0;padding:2px;"
            "line-height:1.3;"
            "white-space:pre;\">",
            fontName, fontSize, bgHex.c_str());
        frag += pre;
    }

    int  prevStyle = -1;
    bool inSpan    = false;

    for (Sci_Position pos = selStart; pos < selEnd; ++pos) {

        int style = (int)SendMessage(hwndSci, SCI_GETSTYLEAT, (WPARAM)pos, 0);
        if (style < 0 || style > 255) style = 0;

        // Open a new <span> on style transitions
        if (style != prevStyle) {
            if (inSpan) frag += "</span>";

            EnsureStyle(hwndSci, style);
            const StyleInfo& si = g_styleCache[style];

            frag += "<span style=\"color:";
            AppendColorHex(frag, si.fore);
            frag += ";background:";
            AppendColorHex(frag, si.back);
            if (si.bold)   frag += ";font-weight:bold";
            if (si.italic) frag += ";font-style:italic";
            frag += "\">";

            inSpan    = true;
            prevStyle = style;
        }

        AppendEscaped(frag, (unsigned char)utf8[static_cast<size_t>(pos - selStart)]);
    }

    if (inSpan) frag += "</span>";
    frag += "</pre>";

    // ── E. Wrap in CF_HTML payload ────────────────────────────────────────────
    std::string cfHtml = BuildCfHtml(frag);
    UINT cfHtmlFmt = RegisterClipboardFormatA("HTML Format");

    // ── F. Write to clipboard ─────────────────────────────────────────────────
    if (!cfHtmlFmt || !OpenClipboard(hwndOwner))
        return FALSE;

    EmptyClipboard();
    BOOL ok = FALSE;

    // CF_UNICODETEXT (for apps that don't understand CF_HTML)
    {
        size_t  cb   = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (hMem) {
            void* p = GlobalLock(hMem);
            if (p) {
                memcpy(p, wide.c_str(), cb);
                GlobalUnlock(hMem);
                ok = (SetClipboardData(CF_UNICODETEXT, hMem) != nullptr);
                if (!ok) GlobalFree(hMem);
            } else {
                GlobalFree(hMem);
            }
        }
    }

    // CF_HTML (for rich editors: Outlook, Word, Gmail, Teams)
    if (ok) {
        size_t  cb   = cfHtml.size() + 1;   // include NUL
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
        if (hMem) {
            void* p = GlobalLock(hMem);
            if (p) {
                memcpy(p, cfHtml.c_str(), cb);
                GlobalUnlock(hMem);
                if (!SetClipboardData(cfHtmlFmt, hMem))
                    GlobalFree(hMem);
                // CF_UNICODETEXT already set — CF_HTML failure is non-fatal
            } else {
                GlobalFree(hMem);
            }
        }
    }

    CloseClipboard();
    return ok;
}
