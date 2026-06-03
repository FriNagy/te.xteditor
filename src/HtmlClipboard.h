/******************************************************************************
*
* TE - Text Editor
*
* HtmlClipboard.h - Copy selection as HTML-formatted clipboard data
*
******************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Places both CF_UNICODETEXT and CF_HTML ("HTML Format") onto the clipboard.
// Syntax colors are read live from Scintilla (SCI_STYLEGETFORE/BACK/BOLD/ITALIC).
//   hwndSci   : the Scintilla editing window
//   hwndOwner : owner window for OpenClipboard (typically hwndMain)
// Returns TRUE on success; caller should fall back to SCI_COPY on FALSE.
BOOL CopySelectionAsHtml(HWND hwndSci, HWND hwndOwner);

#ifdef __cplusplus
}
#endif
