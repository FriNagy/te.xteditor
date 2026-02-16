# TE - Text Editor

A lightweight, open-source text editor for Windows with syntax highlighting, based on the [Scintilla](https://www.scintilla.org/) editing component. Primarily designed for the cURL Programming Script language (.CPS), with additional support as viewer for RTF, Markdown, HTML and Text-Lists. Adapted with the help of AI.

## Features

- Designed for my custom Curl scripting engine with built-in run-and-try execution
- Syntax highlighting  (SQL, JSON, XML, Markdown, BAT, CPS, F2T) 
- Multiple encoding support (UTF-8, UTF-16 )
- Find and Replace with regular expressions
- Line numbering and bookmarks
- Markdown preview (via WebView2) HTML, RTF
- Customizable color schemes and fonts
- Portable - no installation required
- Low memory footprint

## Building

**Requirements:**
- Visual Studio 2026 (v145 toolset) Community or ...
- Windows 10 SDK

**Build:**
1. Open `TE.sln` in Visual Studio
2. Select configuration **Release-TE | x64**
3. Build Solution (Ctrl+Shift+B)

Output: `x64\Release\TE.exe`

## Architecture

| File | Description |
|------|-------------|
| `src/TE.c` | Main window, entry point, command handling, settings I/O |
| `src/Edit.c` | Scintilla wrapper, file I/O, encoding, find/replace |
| `src/Styles.c` | Syntax highlighting, lexer management |
| `src/Dialogs.c` | All dialog boxes |
| `src/Helpers.c` | Utilities, INI handling, MRU lists |
| `src/Print.cpp` | Print functionality |

## Based On

TE is based on [Notepad2](https://www.flos-freeware.ch/notepad2.html) by Florian Balmer, extended with additional features and modern Scintilla/Lexilla integration. 

## License

[MIT License](LICENSE)
