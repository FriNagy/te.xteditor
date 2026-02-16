// MarkdownPreview.c - Markdown to HTML conversion and preview for TU
// Only compiled when BUILD_TE is defined

#ifdef BUILD_TE

#include <windows.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <Richedit.h>
#include "Scintilla.h"
#include "TE.h"
#include "resource.h"

// cmark-gfm headers
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"

// External declarations
extern HWND hwndMain;
extern HWND hwndEdit;
extern HWND hwndEditFrame;
extern HWND hwndWebView;
extern BOOL bPreviewMode;
extern void UpdateStatusbar(void);
extern WCHAR szCurFile[MAX_PATH+40];

// RTF Preview globals
static HWND hwndRtfPreview = NULL;
static BOOL bRtfPreviewMode = FALSE;
static BOOL bHtmlPreviewMode = FALSE;
static HMENU hSavedMenu = NULL;

// External WebView2 functions (from WebView2Helper.cpp)
extern BOOL InitWebView2(HWND hwndParent);
extern BOOL WebView2IsInitialized(void);
extern void WebView2NavigateToHtml(const wchar_t* html);
extern void WebView2Resize(int x, int y, int cx, int cy);
extern BOOL WebView2IsReady(void);
extern const wchar_t* WebView2GetSelectedText(void);
extern void WebView2ClearSelectedText(void);

// CSS for GitHub-like styling
static const char* g_cssStyle =
"<style>"
"body { "
"  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif; "
"  font-size: 14px; "
"  line-height: 1.2; "
"  padding: 20px; "
"  max-width: 900px; "
"  margin: 0 auto; "
"  color: #24292e; "
"  background-color: #ffffff; "
"}"
"h1, h2, h3, h4, h5, h6 { "
"  margin-top: 24px; "
"  margin-bottom: 16px; "
"  font-weight: 600; "
"  line-height: 1.25; "
"}"
"h1 { font-size: 2em; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }"
"h2 { font-size: 1.5em; border-bottom: 1px solid #eaecef; padding-bottom: 0.3em; }"
"h3 { font-size: 1.25em; }"
"code { "
"  font-family: 'Consolas', 'Monaco', monospace; "
"  font-size: 85%; "
"  background-color: rgba(27,31,35,0.05); "
"  padding: 0.2em 0.4em; "
"  border-radius: 3px; "
"}"
"pre { "
"  background-color: #f6f8fa; "
"  padding: 16px; "
"  overflow: auto; "
"  border-radius: 6px; "
"  line-height: 1.2; "
"}"
"pre code { "
"  background-color: transparent; "
"  padding: 0; "
"}"
"blockquote { "
"  padding: 0 1em; "
"  color: #6a737d; "
"  border-left: 0.25em solid #dfe2e5; "
"  margin: 0 0 16px 0; "
"}"
"table { "
"  border-collapse: collapse; "
"  margin: 16px 0; "
"}"
"table th, table td { "
"  padding: 6px 13px; "
"  border: 1px solid #dfe2e5; "
"}"
"table tr:nth-child(2n) { "
"  background-color: #f6f8fa; "
"}"
"table th { "
"  font-weight: 600; "
"  background-color: #f1f3f5; "
"}"
"ul, ol { "
"  padding-left: 2em; "
"}"
"li { "
"  margin: 0.25em 0; "
"}"
"a { "
"  color: #0366d6; "
"  text-decoration: none; "
"}"
"a:hover { "
"  text-decoration: underline; "
"}"
"hr { "
"  height: 0.25em; "
"  padding: 0; "
"  margin: 24px 0; "
"  background-color: #e1e4e8; "
"  border: 0; "
"}"
"img { "
"  max-width: 100%; "
"}"
"input[type='checkbox'] { "
"  margin-right: 0.5em; "
"}"
".task-list-item { "
"  list-style-type: none; "
"}"
"/* CPS syntax highlighting */"
".cps-cm{color:#008000;font-style:italic}"           /* # comment, // comment */
".cps-hd{font-weight:bold;color:#800080}"             /* # header (markdown mode), ~~~ fence */
".cps-lb{color:#00008B;background:#E3ECFF}"           /* :labelname */
".cps-va{color:#C80000;font-weight:bold}"             /* %var / $var assignment */
".cps-vd{color:#FF0000;background:#FFFF00}"           /* (%var) delimiters */
".cps-vc{background:#FFFF00}"                         /* (%var proc) content */
".cps-st{color:#A31515}"                              /* "string" values */
".cps-ou{background:#E0F7FA}"                         /* > output text */
".cps-co{color:#0000FF;font-weight:bold}"             /* commands: goto, shell, etc. */
".cps-kw{color:#0000FF;font-weight:bold}"             /* if, ifnot, then, else, etc. */
".cps-op{color:#000000}"                              /* operators: = == ~= > < */
".cps-ct{color:#808080;background:#E0F7FA}"           /* [color] tags in output */
".cps-fw{color:#000000;background:#F0F0F0}"           /* !> block content */
".cps-fr{color:#000000;background:#FDDCB2}"           /* !< extract */
".cps-jr{color:#0000C0;font-weight:bold}"             /* +N relative jump */
"</style>";

// CPS syntax highlighting script for markdown preview
static const char* g_cpsHighlightScript =
"<script>"
"(function(){"
"function E(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
"function S(c,s){return '<span class=\"cps-'+c+'\">'+E(s)+'</span>';}"
/* FC: find comment - returns index of // outside quotes and not after : (URLs) */
"function FC(s){"
"  var q=0,qc='';"
"  for(var i=0;i<s.length-1;i++){"
"    var c=s[i];"
"    if(q){if(c===qc)q=0;}"
"    else if(c==='\"'||c===\"'\"){q=1;qc=c;}"
"    else if(c==='/'&&s[i+1]==='/'&&(i===0||s[i-1]!==':'))return i;}"
"  return -1;}"
/* IL: inline highlight with placeholder approach to avoid regex/HTML conflicts */
"function IL(raw,isOut){"
"  var ci=FC(raw);"
"  var code=ci>=0?raw.substring(0,ci):raw;"
"  var cmt=ci>=0?raw.substring(ci):'';"
/* Step 1: extract (%...) into placeholders BEFORE escaping */
"  var vx=[];"
"  code=code.replace(/\\(%([^)]*)\\)/g,function(m,inner){"
"    var r='<span class=\"cps-vd\">(%</span>';"
"    if(inner){var sp=inner.indexOf(' ');"
"      if(sp>=0){r+='<span class=\"cps-vd\">'+E(inner.substring(0,sp))+'</span>';"
"        r+='<span class=\"cps-vc\">'+E(inner.substring(sp))+'</span>';}"
"      else{r+='<span class=\"cps-vd\">'+E(inner)+'</span>';}}"
"    r+='<span class=\"cps-vd\">)</span>';"
"    vx.push(r);return '\\x01'+vx.length+'\\x01';});"
/* Step 2: escape remaining text */
"  var s=E(code);"
/* Step 3: apply regexes on clean escaped text (no HTML tags yet) */
"  s=s.replace(/\"[^\"]*\"/g,'<span class=\"cps-st\">$&</span>');"
"  s=s.replace(/'[^']*'/g,'<span class=\"cps-st\">$&</span>');"
"  s=s.replace(/^(\\s*)([$%][a-zA-Z_]\\w*)/,'$1<span class=\"cps-va\">$2</span>');"
"  s=s.replace(/(\\s)(%[a-zA-Z_]\\w*)/g,'$1<span class=\"cps-va\">$2</span>');"
"  s=s.replace(/^(\\s*)(goto|gosub|include|shell|run|runw|pause\\$?|setlog|exit|return|call|extract|pmdump|sleep)\\b/i,"
"    '$1<span class=\"cps-co\">$2</span>');"
"  s=s.replace(/\\b(if|ifnot|then|else|undef|next|in)\\b/gi,'<span class=\"cps-kw\">$&</span>');"
"  s=s.replace(/(^|\\s):([a-zA-Z_#][a-zA-Z0-9_#]*)/g,'$1<span class=\"cps-lb\">:$2</span>');"
"  s=s.replace(/(==|~=)/g,'<span class=\"cps-op\">$1</span>');"
"  s=s.replace(/(\\s)(\\+\\d+)\\b/g,'$1<span class=\"cps-jr\">$2</span>');"
"  if(isOut)s=s.replace(/\\[[^\\]]+\\]/g,'<span class=\"cps-ct\">$&</span>');"
/* Step 4: restore (%...) placeholders */
"  s=s.replace(/\\x01(\\d+)\\x01/g,function(m,i){return vx[parseInt(i)-1];});"
/* Step 5: append comment */
"  if(cmt)s+='<span class=\"cps-cm\">'+E(cmt)+'</span>';"
"  return s;"
"}"
"document.querySelectorAll('code.language-cps').forEach(function(block){"
"  var lines=block.textContent.split('\\n');"
"  var out=[],inFO=false;"
"  lines.forEach(function(raw){"
"    var t=raw.replace(/^\\s+/,'');"
/* empty line ends block */
"    if(!t){inFO=false;out.push(E(raw));return;}"
/* // comment at line start */
"    if(/^\\/\\//.test(t)){inFO=false;out.push(S('cm',raw));return;}"
/* # at line start: comment (in code blocks, not markdown mode) */
"    if(/^#/.test(t)){inFO=false;out.push(S('cm',raw));return;}"
/* ~~~ or ``` code fence */
"    if(/^(~~~|```)/.test(t)){inFO=false;out.push(S('hd',raw));return;}"
/* :label definition (with optional // comment) */
"    if(/^:[a-zA-Z_#]/.test(t)){inFO=false;"
"      var lci=raw.indexOf('//');"
"      if(lci>=0){out.push(S('lb',raw.substring(0,lci))+'<span class=\"cps-cm\">'+E(raw.substring(lci))+'</span>');}"
"      else{out.push(S('lb',raw));}"
"      return;}"
/* !< extract */
"    if(/^!</.test(t)){inFO=false;out.push('<span class=\"cps-fr\">'+IL(raw,false)+'</span>');return;}"
/* !> file/HTTP block start */
"    if(/^!>/.test(t)){inFO=true;out.push('<span class=\"cps-fw\">'+IL(raw,false)+'</span>');return;}"
/* inside !> block */
"    if(inFO){out.push('<span class=\"cps-fw\">'+IL(raw,false)+'</span>');return;}"
/* > output */
"    if(/^>/.test(t)){out.push('<span class=\"cps-ou\">'+IL(raw,true)+'</span>');return;}"
/* regular line */
"    out.push(IL(raw,false));"
"  });"
"  block.innerHTML=out.join('\\n');"
"});"
"})();"
"</script>";

// Convert Markdown to HTML using cmark-gfm
char* MarkdownToHtml(const char* markdown, size_t len)
{
    if (!markdown || len == 0) {
        return NULL;
    }

    // Pre-process: replace ´ (U+00B4, UTF-8: 0xC2 0xB4) with ` (0x60)
    char* preprocessed = (char*)malloc(len + 1);
    if (!preprocessed) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)markdown[i] == 0xC2 && i + 1 < len && (unsigned char)markdown[i+1] == 0xB4) {
            preprocessed[j++] = '`';
            i++; // skip second byte
        } else {
            preprocessed[j++] = markdown[i];
        }
    }
    preprocessed[j] = '\0';
    markdown = preprocessed;
    len = j;

    // Register GFM extensions
    cmark_gfm_core_extensions_ensure_registered();

    // Create parser with extensions
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    if (!parser) {
        return NULL;
    }

    // Attach GFM extensions
    cmark_syntax_extension* table_ext = cmark_find_syntax_extension("table");
    cmark_syntax_extension* autolink_ext = cmark_find_syntax_extension("autolink");
    cmark_syntax_extension* strikethrough_ext = cmark_find_syntax_extension("strikethrough");
    cmark_syntax_extension* tasklist_ext = cmark_find_syntax_extension("tasklist");

    if (table_ext) cmark_parser_attach_syntax_extension(parser, table_ext);
    if (autolink_ext) cmark_parser_attach_syntax_extension(parser, autolink_ext);
    if (strikethrough_ext) cmark_parser_attach_syntax_extension(parser, strikethrough_ext);
    if (tasklist_ext) cmark_parser_attach_syntax_extension(parser, tasklist_ext);

    // Parse document
    cmark_parser_feed(parser, markdown, len);
    free(preprocessed);
    cmark_node* doc = cmark_parser_finish(parser);

    if (!doc) {
        cmark_parser_free(parser);
        return NULL;
    }

    // Render to HTML with extensions so tables/strikethrough/tasklists work
    cmark_llist* extensions = cmark_parser_get_syntax_extensions(parser);
    char* html = cmark_render_html(doc, CMARK_OPT_DEFAULT, extensions);
    cmark_parser_free(parser);
    cmark_node_free(doc);

    return html;  // Caller must free with free()
}

// JavaScript for context menu and selection handling
static const char* g_contextMenuScript =
"<script>"
"document.addEventListener('contextmenu', function(e) {"
"  e.preventDefault();"
"  var sel = window.getSelection().toString();"
"  window.chrome.webview.postMessage({type:'contextmenu', x:e.clientX, y:e.clientY, selection:sel});"
"});"
"</script>";

// Wrap HTML content with full page structure and CSS
static wchar_t* WrapHtmlWithStyle(const char* htmlBody)
{
    if (!htmlBody) {
        htmlBody = "";
    }

    // Calculate required buffer size
    size_t cssLen = strlen(g_cssStyle);
    size_t bodyLen = strlen(htmlBody);
    size_t scriptLen = strlen(g_contextMenuScript);
    size_t cpsLen = strlen(g_cpsHighlightScript);
    size_t totalLen = cssLen + bodyLen + scriptLen + cpsLen + 200;

    // Create full HTML document
    char* fullHtml = (char*)GlobalAlloc(GPTR, totalLen);
    if (!fullHtml) {
        return NULL;
    }

    // Build HTML string manually (wsprintfA has 1024 char limit!)
    char* p = fullHtml;
    strcpy(p, "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">");
    strcat(p, g_cssStyle);
    strcat(p, "</head><body>");
    strcat(p, htmlBody);
    strcat(p, g_contextMenuScript);
    strcat(p, g_cpsHighlightScript);
    strcat(p, "</body></html>");

    // Convert to wide string for WebView2
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, fullHtml, -1, NULL, 0);
    wchar_t* wideHtml = (wchar_t*)GlobalAlloc(GPTR, wideLen * sizeof(wchar_t));
    if (wideHtml) {
        MultiByteToWideChar(CP_UTF8, 0, fullHtml, -1, wideHtml, wideLen);
    }

    GlobalFree(fullHtml);
    return wideHtml;  // Caller must free with GlobalFree
}

// RTF Preview stream callback
static DWORD CALLBACK RtfStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb)
{
    HANDLE hFile = (HANDLE)dwCookie;
    return !ReadFile(hFile, pbBuff, cb, (DWORD*)pcb, NULL);
}

// Check if current file is RTF
static BOOL IsRtfFile(void)
{
    if (lstrlen(szCurFile) < 5) return FALSE;
    LPCWSTR ext = PathFindExtension(szCurFile);
    return (ext && _wcsicmp(ext, L".rtf") == 0);
}

// Check if current file is HTML
static BOOL IsHtmlFile(void)
{
    if (lstrlen(szCurFile) < 5) return FALSE;
    LPCWSTR ext = PathFindExtension(szCurFile);
    if (!ext) return FALSE;
    return (_wcsicmp(ext, L".html") == 0 ||
            _wcsicmp(ext, L".htm") == 0 ||
            _wcsicmp(ext, L".shtml") == 0 ||
            _wcsicmp(ext, L".xhtml") == 0);
}

// Check if currently in HTML preview mode
BOOL IsHtmlPreviewMode(void)
{
    return bHtmlPreviewMode;
}

// Create RTF preview control
static BOOL CreateRtfPreview(HWND hwndParent)
{
    if (hwndRtfPreview) return TRUE;  // Already created

    LoadLibrary(L"Msftedit.dll");

    RECT rc;
    GetClientRect(hwndParent, &rc);

    hwndRtfPreview = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS,
        L"",
        WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, rc.right, rc.bottom,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    if (!hwndRtfPreview) return FALSE;

    // Enable URL detection and link clicks
    SendMessage(hwndRtfPreview, EM_SETEVENTMASK, 0,
        SendMessage(hwndRtfPreview, EM_GETEVENTMASK, 0, 0) | ENM_LINK);
    SendMessage(hwndRtfPreview, EM_AUTOURLDETECT, TRUE, 0);

    // Set background color
    SendMessage(hwndRtfPreview, EM_SETBKGNDCOLOR, 0, (LPARAM)GetSysColor(COLOR_WINDOW));

    return TRUE;
}

// Load RTF file into preview
static BOOL LoadRtfPreview(void)
{
    if (!hwndRtfPreview || lstrlen(szCurFile) == 0) return FALSE;

    HANDLE hFile = CreateFile(szCurFile, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    EDITSTREAM es = { 0 };
    es.pfnCallback = RtfStreamCallback;
    es.dwCookie = (DWORD_PTR)hFile;

    SendMessage(hwndRtfPreview, EM_STREAMIN, SF_RTF, (LPARAM)&es);

    CloseHandle(hFile);

    // Scroll to top
    SendMessage(hwndRtfPreview, WM_VSCROLL, SB_TOP, 0);

    return (es.dwError == 0);
}

// Resize RTF preview
void RtfPreviewResize(int x, int y, int cx, int cy)
{
    if (hwndRtfPreview) {
        SetWindowPos(hwndRtfPreview, NULL, x, y, cx, cy, SWP_NOZORDER);
    }
}

// Check if currently in RTF preview mode
BOOL IsRtfPreviewMode(void)
{
    return bRtfPreviewMode;
}

// Update the Markdown preview
void UpdateMarkdownPreview(void)
{
    if (!hwndEdit || !hwndWebView) {
        return;
    }

    // Get text length from Scintilla
    int len = (int)SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
    if (len <= 0) {
        // Empty document - show empty preview
        WebView2NavigateToHtml(L"<!DOCTYPE html><html><body></body></html>");
        return;
    }

    // Allocate buffer and get text
    char* markdown = (char*)GlobalAlloc(GPTR, len + 1);
    if (!markdown) {
        return;
    }

    SendMessage(hwndEdit, SCI_GETTEXT, len + 1, (LPARAM)markdown);

    // Convert Markdown to HTML
    char* htmlBody = MarkdownToHtml(markdown, len);
    GlobalFree(markdown);

    // Wrap with CSS and convert to wide string
    wchar_t* fullHtml = WrapHtmlWithStyle(htmlBody);
    if (htmlBody) {
        free(htmlBody);  // cmark uses standard malloc
    }

    // Navigate WebView2 to the HTML
    if (fullHtml) {
        WebView2NavigateToHtml(fullHtml);
        GlobalFree(fullHtml);
    }
}

// Export preview as HTML file
BOOL ExportPreviewAsHtml(HWND hwnd)
{
    if (!hwndEdit) return FALSE;

    int len = (int)SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
    if (len <= 0) return FALSE;

    // Build default filename from current file
    OPENFILENAME ofn;
    WCHAR szFile[MAX_PATH] = L"";

    if (lstrlen(szCurFile)) {
        lstrcpy(szFile, PathFindFileName(szCurFile));
        LPWSTR ext = PathFindExtension(szFile);
        if (ext) lstrcpy(ext, L".html");
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"HTML (*.html)\0*.html\0Alle Dateien (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"html";

    if (!GetSaveFileName(&ofn)) return FALSE;

    // Get markdown text
    char* markdown = (char*)GlobalAlloc(GPTR, len + 1);
    if (!markdown) return FALSE;

    SendMessage(hwndEdit, SCI_GETTEXT, len + 1, (LPARAM)markdown);

    // Convert to HTML
    char* htmlBody = MarkdownToHtml(markdown, len);
    GlobalFree(markdown);
    if (!htmlBody) return FALSE;

    // Build clean HTML (with CSS + CPS script, without context menu script)
    size_t cssLen = strlen(g_cssStyle);
    size_t bodyLen = strlen(htmlBody);
    size_t cpsLen = strlen(g_cpsHighlightScript);
    size_t totalLen = cssLen + bodyLen + cpsLen + 200;

    char* fullHtml = (char*)GlobalAlloc(GPTR, totalLen);
    if (!fullHtml) {
        free(htmlBody);
        return FALSE;
    }

    strcpy(fullHtml, "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">");
    strcat(fullHtml, g_cssStyle);
    strcat(fullHtml, "</head><body>");
    strcat(fullHtml, htmlBody);
    strcat(fullHtml, g_cpsHighlightScript);
    strcat(fullHtml, "</body></html>");
    free(htmlBody);

    // Write as UTF-8
    HANDLE hFile = CreateFile(szFile, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        GlobalFree(fullHtml);
        return FALSE;
    }

    DWORD written;
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    WriteFile(hFile, bom, 3, &written, NULL);
    WriteFile(hFile, fullHtml, (DWORD)strlen(fullHtml), &written, NULL);
    CloseHandle(hFile);
    GlobalFree(fullHtml);

    return TRUE;
}

// Update the HTML preview (load raw HTML from editor)
void UpdateHtmlPreview(void)
{
    if (!hwndEdit || !hwndWebView) {
        return;
    }

    // Get text length from Scintilla
    int len = (int)SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
    if (len <= 0) {
        WebView2NavigateToHtml(L"<!DOCTYPE html><html><body></body></html>");
        return;
    }

    // Allocate buffer and get text
    char* html = (char*)GlobalAlloc(GPTR, len + 1);
    if (!html) {
        return;
    }

    SendMessage(hwndEdit, SCI_GETTEXT, len + 1, (LPARAM)html);

    // Inject context menu script before </body> or at end
    size_t htmlLen = strlen(html);
    size_t scriptLen = strlen(g_contextMenuScript);
    char* fullHtml = (char*)GlobalAlloc(GPTR, htmlLen + scriptLen + 1);
    if (!fullHtml) {
        GlobalFree(html);
        return;
    }

    // Try to insert script before </body>
    char* bodyEnd = strstr(html, "</body>");
    if (!bodyEnd) bodyEnd = strstr(html, "</BODY>");

    if (bodyEnd) {
        size_t prefix = bodyEnd - html;
        memcpy(fullHtml, html, prefix);
        memcpy(fullHtml + prefix, g_contextMenuScript, scriptLen);
        strcpy(fullHtml + prefix + scriptLen, bodyEnd);
    } else {
        strcpy(fullHtml, html);
        strcat(fullHtml, g_contextMenuScript);
    }

    GlobalFree(html);

    // Convert to wide string for WebView2
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, fullHtml, -1, NULL, 0);
    wchar_t* wideHtml = (wchar_t*)GlobalAlloc(GPTR, wideLen * sizeof(wchar_t));
    if (wideHtml) {
        MultiByteToWideChar(CP_UTF8, 0, fullHtml, -1, wideHtml, wideLen);
        WebView2NavigateToHtml(wideHtml);
        GlobalFree(wideHtml);
    }

    GlobalFree(fullHtml);
}

// Toggle between edit and preview mode
void ToggleMarkdownPreview(HWND hwnd)
{
    BOOL isRtf = IsRtfFile();
    BOOL isHtml = IsHtmlFile();

    // For RTF files, use RichEdit preview
    if (isRtf) {
        bPreviewMode = !bPreviewMode;
        bRtfPreviewMode = bPreviewMode;
        bHtmlPreviewMode = FALSE;

        if (bPreviewMode) {
            // Create RTF preview if needed
            if (!CreateRtfPreview(hwnd)) {
                MessageBoxW(hwnd, L"RTF-Vorschau konnte nicht erstellt werden.",
                            L"TE", MB_OK | MB_ICONWARNING);
                bPreviewMode = FALSE;
                bRtfPreviewMode = FALSE;
                return;
            }

            // Load and show RTF
            LoadRtfPreview();
            ShowWindow(hwndEdit, SW_HIDE);
            ShowWindow(hwndEditFrame, SW_HIDE);
            if (hwndWebView) ShowWindow(hwndWebView, SW_HIDE);
            ShowWindow(hwndRtfPreview, SW_SHOW);
            SetFocus(hwndRtfPreview);
        } else {
            // Switch back to edit mode
            ShowWindow(hwndRtfPreview, SW_HIDE);
            ShowWindow(hwndEditFrame, SW_SHOW);
            ShowWindow(hwndEdit, SW_SHOW);
            SetFocus(hwndEdit);
        }
    } else {
        // For HTML and Markdown/other files, use WebView2 preview
        bRtfPreviewMode = FALSE;

        // Lazy-load WebView2 on first use
        if (!WebView2IsInitialized()) {
            if (!InitWebView2(hwnd)) {
                MessageBoxW(hwnd, L"WebView2 konnte nicht initialisiert werden.\nMarkdown-Vorschau nicht verfügbar.",
                            L"TE", MB_OK | MB_ICONWARNING);
                return;
            }
        }

        bPreviewMode = !bPreviewMode;
        bHtmlPreviewMode = bPreviewMode && isHtml;

        if (bPreviewMode) {
            // Switch to preview mode
            if (isHtml)
                UpdateHtmlPreview();
            else
                UpdateMarkdownPreview();
            ShowWindow(hwndEdit, SW_HIDE);
            ShowWindow(hwndEditFrame, SW_HIDE);
            if (hwndRtfPreview) ShowWindow(hwndRtfPreview, SW_HIDE);
            ShowWindow(hwndWebView, SW_SHOW);
        } else {
            // Switch to edit mode
            ShowWindow(hwndWebView, SW_HIDE);
            ShowWindow(hwndEditFrame, SW_SHOW);
            ShowWindow(hwndEdit, SW_SHOW);
            SetFocus(hwndEdit);

            // Search for selected text from preview
            const wchar_t* selectedText = WebView2GetSelectedText();
            if (selectedText && *selectedText) {
                // Convert to UTF-8 for Scintilla
                int len = WideCharToMultiByte(CP_UTF8, 0, selectedText, -1, NULL, 0, NULL, NULL);
                if (len > 0) {
                    char* searchText = (char*)GlobalAlloc(GPTR, len);
                    if (searchText) {
                        WideCharToMultiByte(CP_UTF8, 0, selectedText, -1, searchText, len, NULL, NULL);

                        // Search from beginning
                        int docLen = (int)SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
                        SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
                        SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);
                        SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, 0, 0);

                        int pos = (int)SendMessage(hwndEdit, SCI_SEARCHINTARGET, len - 1, (LPARAM)searchText);
                        if (pos >= 0) {
                            // Found - select and scroll to it
                            SendMessage(hwndEdit, SCI_SETSEL, pos, pos + len - 1);
                            SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
                        }

                        GlobalFree(searchText);
                    }
                }
                WebView2ClearSelectedText();
            }
        }
    }

    // Hide menu in preview mode, restore when leaving
    if (bPreviewMode) {
        hSavedMenu = GetMenu(hwnd);
        SetMenu(hwnd, NULL);
    } else if (hSavedMenu) {
        SetMenu(hwnd, hSavedMenu);
        hSavedMenu = NULL;
    }

    // Trigger resize to update layout
    RECT rc;
    GetClientRect(hwnd, &rc);
    SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));

    // Update statusbar for preview mode
    UpdateStatusbar();
}

#endif // BUILD_TE
