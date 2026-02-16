// SciCompat.h - Compatibility layer for Scintilla 3.x to 5.x migration
// Maps old struct names to new Sci_ prefixed names

#ifndef SCICOMPAT_H
#define SCICOMPAT_H

// Struct name mappings for Scintilla 5.x compatibility
#define TextRange Sci_TextRange
#define TextToFind Sci_TextToFind
#define RangeToFormat Sci_RangeToFormat
#define CharacterRange Sci_CharacterRange

#endif // SCICOMPAT_H
