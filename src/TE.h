/******************************************************************************
*
* TE - Text Editor
*
* TE.h - Global definitions and declarations
*
* Based on Notepad2 by Florian Balmer
*
******************************************************************************/

// Jeweilige setzte.bat aufrufen  zB, setze TW
// danach noch output Datei umstellen 

//==== Main Window ============================================================

#include "version.h"


#define WC_TE Lproname

#ifdef winutf8
#define nbsb 0xA0
#endif

#ifdef winutf16
 #define nbsb 0xA0
#endif


//==== Data Type for WM_COPYDATA ==============================================
#define DATA_TE_PARAMS 0xFB10
typedef struct np2params {

  int   flagFileSpecified;
  int   flagChangeNotify;
  int   flagLexerSpecified;
  int   iInitialLexer;
  int   flagQuietCreate;
  int   flagJumpTo;
  int   iInitialLine;
  int   iInitialColumn;
  int   iSrcEncoding;
  int   flagSetEncoding;
  int   flagSetEOLMode;
  int   flagTitleExcerpt;
  WCHAR wchData;

} NP2PARAMS, *LPNP2PARAMS;


//==== Toolbar Style ==========================================================
#define WS_TOOLBAR (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | \
                    TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_ALTDRAG | \
                    TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN | \
                    CCS_ADJUSTABLE)


//==== ReBar Style ============================================================
#define WS_REBAR (WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | \
                  RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOPARENTALIGN)


//==== Ids ====================================================================
#define IDC_STATUSBAR    0xFB00
#define IDC_TOOLBAR      0xFB01
#define IDC_REBAR        0xFB02
#define IDC_EDIT         0xFB03
#define IDC_EDITFRAME    0xFB04
#define IDC_FILENAME     0xFB05
#define IDC_REUSELOCK    0xFB06
#ifdef BUILD_TE
#define IDC_WEBVIEW      0xFB07
#endif


//==== Statusbar ==============================================================
#define STATUS_DOCPOS    0
#define STATUS_DOCSIZE   1
#define STATUS_CODEPAGE  2
#define STATUS_EOLMODE   3
#define STATUS_OVRMODE   4
#define STATUS_LEXER     5
#define STATUS_HELP    255


//==== Change Notifications ===================================================
#define ID_WATCHTIMER 0xA000
#define WM_CHANGENOTIFY WM_USER+1
//#define WM_CHANGENOTIFYCLEAR WM_USER+2


//==== Paste Board Timer ======================================================
#define ID_PASTEBOARDTIMER 0xA001


//==== Reuse Window Lock Timeout ==============================================
#define REUSEWINDOWLOCKTIMEOUT 1000


//==== Function Declarations ==================================================
BOOL InitApplication(HINSTANCE);
HWND InitInstance(HINSTANCE,LPSTR,int);
BOOL ActivatePrevInst();
BOOL RelaunchMultiInst();
BOOL RelaunchElevated();
void SnapToDefaultPos(HWND);
void CALLBACK PasteBoardTimer(HWND,UINT,UINT_PTR,DWORD);


void LoadSettings();
void SaveSettings(BOOL);
void ParseCommandLine();
void LoadFlags();
int  CheckIniFile(LPWSTR,LPCWSTR);
int  CheckIniFileRedirect(LPWSTR,LPCWSTR);
int  FindIniFile();
int  TestIniFile();
int  CreateIniFile();
int  CreateIniFileEx(LPCWSTR);


void UpdateStatusbar();
void UpdateLineNumberWidth();


BOOL FileIO(BOOL,LPCWSTR,BOOL,int*,int*,BOOL*,BOOL*,BOOL*,BOOL);
BOOL FileLoad(BOOL,BOOL,BOOL,BOOL,LPCWSTR);
BOOL FileSave(BOOL,BOOL,BOOL,BOOL);
BOOL OpenFileDlg(HWND,LPWSTR,int,LPCWSTR);
BOOL OpenFileDlgWithPattern(HWND,LPWSTR,int,LPCWSTR,LPCWSTR);
BOOL SaveFileDlg(HWND,LPWSTR,int,LPCWSTR);


LRESULT CALLBACK MainWndProc(HWND,UINT,WPARAM,LPARAM);
LRESULT MsgCreate(HWND,WPARAM,LPARAM);
void    CreateBars(HWND,HINSTANCE);
void    MsgThemeChanged(HWND,WPARAM,LPARAM);
void    MsgSize(HWND,WPARAM,LPARAM);
void    MsgInitMenu(HWND,WPARAM,LPARAM);
LRESULT MsgCommand(HWND,WPARAM,LPARAM);
LRESULT MsgNotify(HWND,WPARAM,LPARAM);


#ifdef BUILD_TE
//==== Markdown Preview (nur TE) ==============================================
BOOL InitWebView2(HWND hwndParent);
BOOL WebView2IsInitialized(void);
void CleanupWebView2(void);
void UpdateMarkdownPreview(void);
void UpdateHtmlPreview(void);
void ToggleMarkdownPreview(HWND hwnd);
char* MarkdownToHtml(const char* markdown, size_t len);
BOOL IsHtmlPreviewMode(void);
#endif


///   End of TE.h   \\\
