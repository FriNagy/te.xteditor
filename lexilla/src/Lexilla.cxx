// Lexilla lexer library - Minimal version for TU
// Includes: CPP, SQL, Diff, Batch, Properties, HTML/XML, JSON, VB

#include <cstring>
#include <vector>
#include <initializer_list>

#if defined(_WIN32)
#define EXPORT_FUNCTION __declspec(dllexport)
#define CALLING_CONVENTION __stdcall
#else
#define EXPORT_FUNCTION __attribute__((visibility("default")))
#define CALLING_CONVENTION
#endif

#include "ILexer.h"
#include "LexerModule.h"
#include "CatalogueModules.h"

using namespace Lexilla;

// Only the lexers we need
extern const LexerModule lmCPP;
extern const LexerModule lmSQL;
extern const LexerModule lmBatch;
extern const LexerModule lmMake;
extern const LexerModule lmHTML;
extern const LexerModule lmJSON;
extern const LexerModule lmF2T;     // Custom F2T/Flex2Text lexer
#ifdef BUILD_TE
extern const LexerModule lmCPS;     // Custom CPS/Curli script lexer (nur TU)
extern const LexerModule lmMarkdown; // nur TU
#endif

static std::vector<const LexerModule *> lexerCatalogue;

static void AddModule(const LexerModule *plm) {
	lexerCatalogue.push_back(plm);
}

static void InitCatalogue() {
	if (lexerCatalogue.empty()) {
		AddModule(&lmCPP);
		AddModule(&lmSQL);
		AddModule(&lmBatch);
		AddModule(&lmMake);
		AddModule(&lmHTML);
		AddModule(&lmJSON);
		AddModule(&lmF2T);
#ifdef BUILD_TE
		AddModule(&lmCPS);
		AddModule(&lmMarkdown);
#endif
	}
}

extern "C" {

EXPORT_FUNCTION int CALLING_CONVENTION GetLexerCount() {
	InitCatalogue();
	return static_cast<int>(lexerCatalogue.size());
}

EXPORT_FUNCTION void CALLING_CONVENTION GetLexerName(unsigned int index, char *name, int buflength) {
	InitCatalogue();
	*name = '\0';
	if (index < lexerCatalogue.size()) {
		const char *lexerName = lexerCatalogue[index]->languageName;
		if (lexerName) {
			strncpy(name, lexerName, buflength - 1);
			name[buflength - 1] = '\0';
		}
	}
}

EXPORT_FUNCTION LexerFactoryFunction CALLING_CONVENTION GetLexerFactory(unsigned int index) {
	InitCatalogue();
	if (index < lexerCatalogue.size()) {
		return nullptr; // fnFactory is protected, use CreateLexer instead
	}
	return nullptr;
}

EXPORT_FUNCTION Scintilla::ILexer5 * CALLING_CONVENTION CreateLexer(const char *name) {
	InitCatalogue();
	for (const LexerModule *plm : lexerCatalogue) {
		if (plm->languageName && (strcmp(plm->languageName, name) == 0)) {
			return plm->Create(); // Use public Create() method
		}
	}
	return nullptr;
}

EXPORT_FUNCTION const char * CALLING_CONVENTION LexerNameFromID(int identifier) {
	InitCatalogue();
	for (const LexerModule *plm : lexerCatalogue) {
		if (plm->GetLanguage() == identifier) {
			return plm->languageName;
		}
	}
	return nullptr;
}

EXPORT_FUNCTION const char * CALLING_CONVENTION GetLibraryPropertyNames() {
	return "";
}

EXPORT_FUNCTION void CALLING_CONVENTION SetLibraryProperty(const char *, const char *) {
}

EXPORT_FUNCTION const char * CALLING_CONVENTION GetNameSpace() {
	return "lexilla";
}

}

void AddStaticLexerModule(const LexerModule *plm) {
	InitCatalogue();
	lexerCatalogue.push_back(plm);
}
