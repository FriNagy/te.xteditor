# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TE is a lightweight, open-source text editor for Windows with syntax highlighting, based on the Scintilla editing component. It serves as a Notepad replacement for NT-based Windows systems. Written in C/C++ using Win32 APIs.

## Development Environment

**Visual Studio 2026** Installation:
- Path: `C:\Program Files\Microsoft Visual Studio\18\Community`
- MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`

## Build Commands

**Solution:** `TE.sln` (Visual Studio 2026, C++17)

Build configurations:
- **Release-TE** (x64): UTF-8 default, mit Schalter für Widechar Windows → `TE.exe`

Build output directory: `x64\Release\`

Präprozessor-Define: `BUILD_TE`.

## Architecture

```
te/
├── src/                    # Main application source
│   ├── TE.c/h             # Main window, entry point, command handling, settings I/O
│   ├── Edit.c/h           # Scintilla wrapper, file I/O, encoding, find/replace, text transforms
│   ├── Styles.c/h         # Syntax highlighting, lexer management, font/color config
│   ├── Dialogs.c/h        # All dialog boxes (find, replace, settings, about)
│   ├── Helpers.c/h        # Utilities (paths, strings, INI handling, MRU lists)
│   ├── Dlapi.c/h          # Directory listing, shell integration
│   └── Print.cpp          # Print functionality
├── scintilla/             # Embedded Scintilla source code editing component
├── lexilla/               # Lexer library for Scintilla 5+
└── res/                   # Icons, bitmaps, manifests
```

## Key Source Files

- **TE.c** (~4500 lines): `MainWndProc` handles all window messages; `MsgCommand` processes menu commands; `LoadSettings`/`SaveSettings` manage INI persistence
- **Edit.c** (~6100 lines): `EditCreate` initializes Scintilla; `EditLoadFile`/`EditSaveFile` handle file I/O with encoding detection; text transformation functions (sort, align, case convert)
- **Styles.c** (~2700 lines): `Style_SetLexer` applies syntax highlighting; supports 20+ languages
- **Helpers.c** (~1400 lines): INI wrapper functions, path utilities, MRU management

## Encoding Support

The editor handles multiple encodings: UTF-8, UTF-16 LE/BE, UTF-7, Windows ANSI, OEM, and 50+ code pages. Encoding detection happens in `EditLoadFile`, conversion in `EditSaveFile`.

## Documentation

- `te\TE.txt`: Comprehensive feature docs, changelog, keyboard shortcuts, regex syntax, command-line switches
