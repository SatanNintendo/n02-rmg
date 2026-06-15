#pragma once
/*
 * kaillera_lang.h - Multilingual string system for N02 Kaillera Client
 *
 * Usage:
 *   - Language files are stored in the "KailleraLang" folder next to the DLL.
 *   - Each file is named "<LanguageName>.lng" (e.g. "English.lng", "Russian.lng").
 *   - The system loads the language specified in n02.ini under [okai] key "Lang",
 *     or falls back to "English" if not set or if the file is not found.
 *   - All user-facing strings are accessed via the LNG() macro, which returns
 *     a const char* to the loaded string value.
 *   - To add a new translation: copy English.lng, translate values, save as
 *     "<LanguageName>.lng" in the KailleraLang folder.
 *
 * Thread safety:
 *   - LangInit() and LangShutdown() should be called from a single thread
 *     (typically during DLL init/shutdown).
 *   - Once loaded, LNG() lookups are read-only and safe for concurrent access.
 *
 * Memory:
 *   - All strings are loaded into a single heap allocation on LangInit().
 *   - LangShutdown() frees everything. No per-string allocations or leaks.
 */

#include <windows.h>

/* Initialize the language system. Loads the .lng file specified in n02.ini
   or falls back to English. Call once at startup. */
void LangInit();

/* Shutdown the language system. Frees all allocated memory. Call once at cleanup. */
void LangShutdown();

/* Look up a string by key. Returns the value from the loaded .lng file.
   If the key is not found, returns the key itself as a fallback.
   This is safe to call from any thread after LangInit(). */
const char* LangGet(const char* key);

/* Convenience macro for accessing localized strings.
   Usage: LNG(BTN_OK)  expands to  LangGet("BTN_OK") */
#define LNG(key) LangGet(#key)

/* Get the display name of the currently loaded language (e.g. "English", "Русский"). */
const char* LangGetName();

/* Get the file-based identifier of the currently loaded language (e.g. "English", "Russian").
   This matches the filenames in KailleraLang/ and is suitable for combo box selection. */
const char* LangGetFile();

/* Get the number of strings loaded from the .lng file. */
int LangGetCount();

/* Enumerate available .lng files in the KailleraLang folder.
   Calls callback(name, userdata) for each file found (without .lng extension).
   Returns the number of languages found. */
typedef void (*LangEnumCallback)(const char* name, void* userdata);
int LangEnumerate(LangEnumCallback callback, void* userdata);

/* Reload the language system with a specific language file.
   Useful for changing language at runtime. Returns true on success. */
bool LangSetLanguage(const char* langName);
