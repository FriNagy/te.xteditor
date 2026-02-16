/******************************************************************************
*
* TU - Text Editor
*
* Dialogs.h - Dialog box definitions
*
* Based on Notepad2 by Florian Balmer
*
******************************************************************************/


#define MBINFO         0
#define MBWARN         1
#define MBYESNO        2
#define MBYESNOWARN    3
#define MBYESNOCANCEL  4
#define MBOKCANCEL     8

int  MsgBox(int,UINT,...);
void DisplayCmdLineHelp();
BOOL GetDirectory(HWND,int,LPWSTR,LPCWSTR,BOOL);
INT_PTR CALLBACK AboutDlgProc(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK RtfDlgProc(HWND, UINT, WPARAM, LPARAM);
void RunDlg(HWND,LPCWSTR);
BOOL OpenWithDlg(HWND,LPCWSTR);
BOOL FavoritesDlg(HWND,LPWSTR);
BOOL AddToFavDlg(HWND,LPCWSTR,LPCWSTR);
BOOL FileMRUDlg(HWND,LPWSTR);
BOOL ColumnWrapDlg(HWND,UINT,int *);
BOOL WordWrapSettingsDlg(HWND,UINT,int *);
BOOL LongLineSettingsDlg(HWND,UINT,int *);
BOOL TabSettingsDlg(HWND,UINT,int *);
// BOOL SelectDefEncodingDlg(HWND,int *);
// BOOL SelectEncodingDlg(HWND,int *);
// BOOL RecodeDlg(HWND,int *);
BOOL SelectDefLineEndingDlg(HWND,int *);
INT_PTR InfoBox(int,LPCWSTR,int,...);


// End of Dialogs.h
