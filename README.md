# TE - Text Editor

A lightweight, open-source text editor for Windows with syntax highlighting, based on the [Scintilla](https://www.scintilla.org/) editing component. Primarily designed for the cURL Programming Script language (.CPS), with additional support for DataFlex, display of lists, RTF, Markdown, and HTML in view mode. TE also serves as a modern Notepad replacement for NT-based Windows systems.

## Features

- Syntax highlighting for 20+ languages (C/C++, HTML, CSS, JavaScript, SQL, JSON, Markdown, and more)
- Multiple encoding support (UTF-8, UTF-16 LE/BE, ANSI, OEM, 50+ code pages)
- Find and Replace with regular expressions
- Code folding
- Line numbering and bookmarks
- Auto-indentation
- Markdown preview (via WebView2)
- Customizable color schemes and fonts
- Portable - no installation required
- Low memory footprint

## Building

**Requirements:**
- Visual Studio 2026 (v145 toolset)
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
