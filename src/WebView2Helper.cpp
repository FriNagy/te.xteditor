// WebView2Helper.cpp - WebView2 integration for TU Markdown Preview
// Only compiled when BUILD_TE is defined

#ifdef BUILD_TE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl.h>
#include <string>

#include "WebView2.h"
#include "resource.h"

using namespace Microsoft::WRL;

// Global WebView2 objects
static ComPtr<ICoreWebView2Controller> g_webviewController;
static ComPtr<ICoreWebView2> g_webview;
static HWND g_hwndWebView = NULL;
static HWND g_hwndParent = NULL;
static bool g_webviewReady = false;
static bool g_webviewInitialized = false;  // Track if InitWebView2 was called
static std::wstring g_pendingHtml;
static std::wstring g_selectedText;
static std::wstring g_userDataFolder;
static const wchar_t* WEBVIEW_CLASS = L"TUWebViewHost";

// Forward declarations
static HRESULT OnCreateEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* env);
static HRESULT OnCreateCoreWebView2ControllerCompleted(HRESULT result, ICoreWebView2Controller* controller);

extern "C" {

// External declarations from TU.c
extern HWND hwndMain;
extern HWND hwndEdit;

HWND hwndWebView = NULL;
BOOL bPreviewMode = FALSE;
BOOL bStartInPreviewMode = FALSE;

// Context menu item IDs
#define IDM_PREVIEW_EXIT    50001
#define IDM_PREVIEW_REFRESH 50002
#define IDM_PREVIEW_FIND    50003
#define IDM_PREVIEW_SAVEHTML 50004
#define IDM_PREVIEW_SAVEPDF  50005
#define IDM_PREVIEW_COPY     50006

// Forward declarations
extern "C" void ToggleMarkdownPreview(HWND hwnd);
extern "C" void UpdateMarkdownPreview(void);
extern "C" BOOL ExportPreviewAsHtml(HWND hwnd);
extern WCHAR szCurFile[MAX_PATH+40];

// Window procedure for WebView2 host
static LRESULT CALLBACK WebViewHostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_SIZE:
            if (g_webviewController) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                g_webviewController->put_Bounds(rc);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Check if WebView2 has been initialized
BOOL WebView2IsInitialized(void)
{
    return g_webviewInitialized ? TRUE : FALSE;
}

// Initialize WebView2
BOOL InitWebView2(HWND hwndParent)
{
    if (g_webviewInitialized) {
        return TRUE;  // Already initialized
    }

    g_hwndParent = hwndParent;

    // Build userDataFolder path in %LOCALAPPDATA%\TU\WebView2
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        g_userDataFolder = localAppData;
        g_userDataFolder += L"\\TE\\WebView2";
        // Create directory if it doesn't exist
        CreateDirectoryW((g_userDataFolder.substr(0, g_userDataFolder.rfind(L'\\'))).c_str(), NULL);  // Create TE folder
        CreateDirectoryW(g_userDataFolder.c_str(), NULL);  // Create WebView2 folder
    }

    // Register window class for WebView2 host
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WebViewHostProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = WEBVIEW_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    // Create a host window for WebView2
    g_hwndWebView = CreateWindowExW(
        0,
        WEBVIEW_CLASS,
        L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE,
        0, 0, 100, 100,
        hwndParent,
        (HMENU)0xFB07,  // IDC_WEBVIEW
        GetModuleHandle(NULL),
        NULL);

    if (!g_hwndWebView) {
        MessageBoxW(NULL, L"Failed to create WebView2 host window", L"Debug", MB_OK);
        return FALSE;
    }

    // Initially hide it
    ShowWindow(g_hwndWebView, SW_HIDE);

    hwndWebView = g_hwndWebView;

    // Create WebView2 environment with userDataFolder in LOCALAPPDATA
    const wchar_t* userDataPath = g_userDataFolder.empty() ? nullptr : g_userDataFolder.c_str();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataPath, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                return OnCreateEnvironmentCompleted(result, env);
            }).Get());

    if (FAILED(hr)) {
        wchar_t msg[128];
        wsprintfW(msg, L"CreateCoreWebView2Environment failed: 0x%08X", hr);
        MessageBoxW(NULL, msg, L"Debug", MB_OK);
    } else {
        g_webviewInitialized = true;
    }

    return SUCCEEDED(hr);
}

// Cleanup WebView2
void CleanupWebView2(void)
{
    g_webview = nullptr;
    g_webviewController = nullptr;
    g_webviewReady = false;
    g_webviewInitialized = false;

    if (g_hwndWebView) {
        DestroyWindow(g_hwndWebView);
        g_hwndWebView = NULL;
        hwndWebView = NULL;
    }

    UnregisterClassW(WEBVIEW_CLASS, GetModuleHandle(NULL));
}

// Navigate to HTML content
void WebView2NavigateToHtml(const wchar_t* html)
{
    if (!g_webviewReady || !g_webview) {
        // Store for later when WebView2 is ready
        g_pendingHtml = html ? html : L"";
        return;
    }

    // Update bounds before navigation
    if (g_webviewController) {
        RECT rc;
        GetClientRect(g_hwndWebView, &rc);
        g_webviewController->put_Bounds(rc);
        g_webviewController->put_IsVisible(TRUE);
    }

    g_webview->NavigateToString(html);
}

// Resize WebView2 to fit parent
void WebView2Resize(int x, int y, int cx, int cy)
{
    if (g_hwndWebView) {
        SetWindowPos(g_hwndWebView, NULL, x, y, cx, cy, SWP_NOZORDER);
    }

    if (g_webviewController) {
        RECT bounds = { 0, 0, cx, cy };
        g_webviewController->put_Bounds(bounds);
        g_webviewController->put_IsVisible(TRUE);
    }
}

// Check if WebView2 is ready
BOOL WebView2IsReady(void)
{
    return g_webviewReady ? TRUE : FALSE;
}

// Get selected text from preview and clear it
const wchar_t* WebView2GetSelectedText(void)
{
    return g_selectedText.empty() ? NULL : g_selectedText.c_str();
}

void WebView2ClearSelectedText(void)
{
    g_selectedText.clear();
}

} // extern "C"

// Export preview as PDF via WebView2
static void ExportPreviewAsPdf(void)
{
    if (!g_webview) return;

    OPENFILENAME ofn;
    WCHAR szFile[MAX_PATH] = L"";

    if (lstrlen(szCurFile)) {
        lstrcpy(szFile, PathFindFileName(szCurFile));
        LPWSTR ext = PathFindExtension(szFile);
        if (ext) lstrcpy(ext, L".pdf");
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndParent;
    ofn.lpstrFilter = L"PDF (*.pdf)\0*.pdf\0Alle Dateien (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"pdf";

    if (!GetSaveFileName(&ofn)) return;

    ComPtr<ICoreWebView2_7> webview7;
    if (SUCCEEDED(g_webview.As(&webview7))) {
        webview7->PrintToPdf(szFile, nullptr,
            Callback<ICoreWebView2PrintToPdfCompletedHandler>(
                [](HRESULT result, BOOL isSuccessful) -> HRESULT {
                    if (FAILED(result) || !isSuccessful) {
                        MessageBoxW(g_hwndParent, L"PDF-Export fehlgeschlagen.",
                                    L"Fehler", MB_OK | MB_ICONERROR);
                    }
                    return S_OK;
                }).Get());
    }
}

// Callback when environment is created
static HRESULT OnCreateEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* env)
{
    if (FAILED(result) || !env) {
        wchar_t msg[128];
        wsprintfW(msg, L"OnCreateEnvironmentCompleted failed: 0x%08X", result);
        MessageBoxW(NULL, msg, L"Debug", MB_OK);
        return result;
    }

    // Create WebView2 controller
    HRESULT hr = env->CreateCoreWebView2Controller(
        g_hwndWebView,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                return OnCreateCoreWebView2ControllerCompleted(result, controller);
            }).Get());

    if (FAILED(hr)) {
        wchar_t msg[128];
        wsprintfW(msg, L"CreateCoreWebView2Controller failed: 0x%08X", hr);
        MessageBoxW(NULL, msg, L"Debug", MB_OK);
    }

    return S_OK;
}

// Callback when controller is created
static HRESULT OnCreateCoreWebView2ControllerCompleted(HRESULT result, ICoreWebView2Controller* controller)
{
    if (FAILED(result) || !controller) {
        wchar_t msg[128];
        wsprintfW(msg, L"OnCreateCoreWebView2ControllerCompleted failed: 0x%08X", result);
        MessageBoxW(NULL, msg, L"Debug", MB_OK);
        return result;
    }

    g_webviewController = controller;
    g_webviewController->get_CoreWebView2(&g_webview);

    // Configure WebView2
    if (g_webview) {
        ComPtr<ICoreWebView2Settings> settings;
        g_webview->get_Settings(&settings);

        if (settings) {
            settings->put_IsScriptEnabled(TRUE);
            settings->put_AreDefaultScriptDialogsEnabled(FALSE);
            settings->put_AreDevToolsEnabled(FALSE);
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
        }

        // Set initial bounds and make visible
        RECT rc;
        GetClientRect(g_hwndWebView, &rc);
        g_webviewController->put_Bounds(rc);
        g_webviewController->put_IsVisible(TRUE);

        // Register accelerator key handler to capture F12, ESC and Ctrl+Alt+P
        g_webviewController->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [](ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
                    (void)sender;
                    COREWEBVIEW2_KEY_EVENT_KIND kind;
                    args->get_KeyEventKind(&kind);

                    // Only handle key down events
                    if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN &&
                        kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
                        return S_OK;
                    }

                    UINT key;
                    args->get_VirtualKey(&key);

                    // F12 key - toggle preview (return to editor)
                    if (key == VK_F12) {
                        args->put_Handled(TRUE);
                        PostMessage(g_hwndParent, WM_COMMAND, IDM_VIEW_PREVIEW, 0);
                        return S_OK;
                    }

                    // ESC key - exit preview
                    if (key == VK_ESCAPE) {
                        args->put_Handled(TRUE);
                        PostMessage(g_hwndParent, WM_COMMAND, IDM_VIEW_PREVIEW, 0);
                        return S_OK;
                    }

                    BOOL ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    BOOL altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;

                    // Ctrl+C - copy selected text to clipboard
                    if (key == 'C' && ctrlDown && !altDown) {
                        args->put_Handled(TRUE);
                        if (g_webview) {
                            g_webview->ExecuteScript(
                                L"window.chrome.webview.postMessage({type:'copy', text:window.getSelection().toString()});",
                                nullptr);
                        }
                        return S_OK;
                    }

                    // Ctrl+Alt+P - toggle preview
                    if (key == 'P' && ctrlDown && altDown) {
                        args->put_Handled(TRUE);
                        PostMessage(g_hwndParent, WM_COMMAND, IDM_VIEW_PREVIEW, 0);
                        return S_OK;
                    }

                    // Let WebView2 handle all other keys normally
                    return S_OK;
                }).Get(),
            nullptr);

        // Enable web message handling for context menu
        settings->put_IsWebMessageEnabled(TRUE);

        // Register web message handler for context menu
        g_webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    (void)sender;
                    LPWSTR message;
                    args->get_WebMessageAsJson(&message);

                    // Handle copy request (Ctrl+C)
                    if (wcsstr(message, L"\"type\":\"copy\"")) {
                        wchar_t* ptxt = wcsstr(message, L"\"text\":\"");
                        if (ptxt) {
                            ptxt += 8; // skip "text":"
                            wchar_t* pend = wcschr(ptxt, L'"');
                            if (pend && pend > ptxt) {
                                std::wstring copyText(ptxt, pend - ptxt);
                                // Unescape JSON \\n to real newlines
                                std::wstring unescaped;
                                for (size_t i = 0; i < copyText.size(); i++) {
                                    if (copyText[i] == L'\\' && i + 1 < copyText.size()) {
                                        if (copyText[i+1] == L'n') { unescaped += L'\n'; i++; }
                                        else if (copyText[i+1] == L'r') { unescaped += L'\r'; i++; }
                                        else if (copyText[i+1] == L't') { unescaped += L'\t'; i++; }
                                        else if (copyText[i+1] == L'\\') { unescaped += L'\\'; i++; }
                                        else if (copyText[i+1] == L'"') { unescaped += L'"'; i++; }
                                        else { unescaped += copyText[i]; }
                                    } else {
                                        unescaped += copyText[i];
                                    }
                                }
                                // Copy to clipboard
                                if (OpenClipboard(g_hwndParent)) {
                                    EmptyClipboard();
                                    size_t cbSize = (unescaped.size() + 1) * sizeof(wchar_t);
                                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cbSize);
                                    if (hMem) {
                                        wchar_t* pDest = (wchar_t*)GlobalLock(hMem);
                                        memcpy(pDest, unescaped.c_str(), cbSize);
                                        GlobalUnlock(hMem);
                                        SetClipboardData(CF_UNICODETEXT, hMem);
                                    }
                                    CloseClipboard();
                                }
                            }
                        }
                        CoTaskMemFree(message);
                        return S_OK;
                    }

                    // Check if it's a context menu request
                    if (wcsstr(message, L"contextmenu")) {
                        // Parse selection from message
                        g_selectedText.clear();
                        wchar_t* psel = wcsstr(message, L"\"selection\":\"");
                        if (psel) {
                            psel += 13; // skip "selection":"
                            wchar_t* pend = wcschr(psel, L'"');
                            if (pend && pend > psel) {
                                g_selectedText.assign(psel, pend - psel);
                            }
                        }

                        // Get current mouse position
                        POINT pt;
                        GetCursorPos(&pt);

                        // Show context menu
                        HMENU hMenu = CreatePopupMenu();
                        if (hMenu) {
                            // Show copy/find only if text is selected
                            if (!g_selectedText.empty()) {
                                AppendMenuW(hMenu, MF_STRING, IDM_PREVIEW_COPY, L"&Kopieren\tCtrl+C");
                                AppendMenuW(hMenu, MF_STRING, IDM_PREVIEW_FIND, L"Im &Editor suchen");
                                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                            }
                            AppendMenuW(hMenu, MF_STRING, IDM_PREVIEW_EXIT, L"Zurück zum &Editor\tEsc");
                            AppendMenuW(hMenu, MF_STRING, IDM_PREVIEW_REFRESH, L"&Aktualisieren");
                            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                            AppendMenuW(hMenu, MF_STRING, IDM_PREVIEW_SAVEHTML, L"Als &HTML speichern...");
                            AppendMenuW(hMenu, MF_STRING, IDM_PREVIEW_SAVEPDF, L"Als &PDF speichern...");

                            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                                     pt.x, pt.y, 0, g_hwndParent, NULL);
                            DestroyMenu(hMenu);

                            if (cmd == IDM_PREVIEW_COPY) {
                                // Copy selected text to clipboard
                                if (OpenClipboard(g_hwndParent)) {
                                    EmptyClipboard();
                                    size_t cbSize = (g_selectedText.size() + 1) * sizeof(wchar_t);
                                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cbSize);
                                    if (hMem) {
                                        wchar_t* pDest = (wchar_t*)GlobalLock(hMem);
                                        memcpy(pDest, g_selectedText.c_str(), cbSize);
                                        GlobalUnlock(hMem);
                                        SetClipboardData(CF_UNICODETEXT, hMem);
                                    }
                                    CloseClipboard();
                                }
                            } else if (cmd == IDM_PREVIEW_FIND) {
                                // Switch to editor and search for selected text
                                PostMessage(g_hwndParent, WM_COMMAND, IDM_VIEW_PREVIEW, 0);
                                // Search will be done after toggle
                            } else if (cmd == IDM_PREVIEW_EXIT) {
                                g_selectedText.clear(); // Don't search
                                PostMessage(g_hwndParent, WM_COMMAND, IDM_VIEW_PREVIEW, 0);
                            } else if (cmd == IDM_PREVIEW_REFRESH) {
                                UpdateMarkdownPreview();
                            } else if (cmd == IDM_PREVIEW_SAVEHTML) {
                                ExportPreviewAsHtml(g_hwndParent);
                            } else if (cmd == IDM_PREVIEW_SAVEPDF) {
                                ExportPreviewAsPdf();
                            }
                        }
                    }
                    CoTaskMemFree(message);
                    return S_OK;
                }).Get(),
            nullptr);

        g_webviewReady = true;

        // Navigate to pending content if any
        if (!g_pendingHtml.empty()) {
            g_webview->NavigateToString(g_pendingHtml.c_str());
            g_pendingHtml.clear();
        }
    }

    return S_OK;
}

#endif // BUILD_TE
