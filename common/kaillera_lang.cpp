/*
 * kaillera_lang.cpp - Multilingual string system for N02 Kaillera Client
 *
 * Implements a lightweight key=value language file loader.
 * The .lng format is INI-like: KEY=VALUE, with ; comments and blank lines.
 * All data is stored in a single contiguous allocation for cache-friendliness
 * and zero-leak cleanup.
 */

#include "kaillera_lang.h"
#include "nSettings.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal data structures
 * --------------------------------------------------------------------------- */

/* A single key-value pair. Both pointers point into the shared string buffer. */
struct LangEntry {
    const char* key;
    const char* value;
};

/* The global language state. */
static struct {
    LangEntry*  entries;      /* Array of key-value pairs              */
    int         count;        /* Number of entries                     */
    int         capacity;     /* Allocated capacity of entries array   */
    char*       stringPool;   /* Single allocation holding all strings */
    size_t      stringPoolSz; /* Size of stringPool                    */
    char        langName[64]; /* Display name of loaded language       */
    bool        initialized;  /* Whether LangInit() succeeded          */
} g_lang = {0};

/* ---------------------------------------------------------------------------
 * Helper: trim whitespace in-place
 * --------------------------------------------------------------------------- */
static char* TrimInPlace(char* s) {
    if (s == NULL) return s;
    /* Trim leading */
    while (*s == ' ' || *s == '\t') s++;
    if (*s == 0) return s;
    /* Trim trailing */
    char* end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = 0;
        end--;
    }
    return s;
}

/* ---------------------------------------------------------------------------
 * Helper: decode escape sequences in-place (\n -> newline, etc.)
 * --------------------------------------------------------------------------- */
static void DecodeEscapes(char* s) {
    if (s == NULL) return;
    char* dst = s;
    while (*s) {
        if (*s == '\\' && *(s + 1)) {
            switch (*(s + 1)) {
                case 'n':  *dst++ = '\n'; s += 2; break;
                case 'r':  *dst++ = '\r'; s += 2; break;
                case 't':  *dst++ = '\t'; s += 2; break;
                case '\\': *dst++ = '\\'; s += 2; break;
                default:   *dst++ = *s++;          break;
            }
        } else {
            *dst++ = *s++;
        }
    }
    *dst = 0;
}

/* ---------------------------------------------------------------------------
 * Helper: comparison function for qsort / bsearch (case-sensitive)
 * --------------------------------------------------------------------------- */
static int LangEntryCompare(const void* a, const void* b) {
    return strcmp(((const LangEntry*)a)->key, ((const LangEntry*)b)->key);
}

/* ---------------------------------------------------------------------------
 * Internal: free all allocated memory
 * --------------------------------------------------------------------------- */
static void LangCleanup() {
    if (g_lang.entries) {
        free(g_lang.entries);
        g_lang.entries = NULL;
    }
    if (g_lang.stringPool) {
        free(g_lang.stringPool);
        g_lang.stringPool = NULL;
    }
    g_lang.count = 0;
    g_lang.capacity = 0;
    g_lang.stringPoolSz = 0;
    g_lang.langName[0] = 0;
    g_lang.initialized = false;
}

/* ---------------------------------------------------------------------------
 * Internal: load a .lng file and populate the entries array
 * --------------------------------------------------------------------------- */
static bool LangLoadFile(const char* filePath) {
    FILE* f = NULL;
    if (fopen_s(&f, filePath, "rb") != 0 || f == NULL) {
        return false;
    }

    /* Determine file size */
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 2 * 1024 * 1024) { /* Max 2MB */
        fclose(f);
        return false;
    }

    /* Read entire file into a temporary buffer */
    char* buf = (char*)malloc((size_t)fileSize + 1);
    if (buf == NULL) {
        fclose(f);
        return false;
    }

    size_t bytesRead = fread(buf, 1, (size_t)fileSize, f);
    fclose(f);
    buf[bytesRead] = 0;

    /* Count lines to estimate capacity */
    int lineCount = 0;
    for (size_t i = 0; i < bytesRead; i++) {
        if (buf[i] == '\n') lineCount++;
    }
    lineCount += 2; /* safety margin */

    /* Allocate entries array */
    LangEntry* newEntries = (LangEntry*)malloc(sizeof(LangEntry) * (size_t)lineCount);
    if (newEntries == NULL) {
        free(buf);
        return false;
    }

    /* Parse line by line. We reuse the file buffer as the string pool
       by NUL-terminating each key and value in-place. */
    int entryCount = 0;
    char* line = buf;
    char* fileEnd = buf + bytesRead;

    while (line < fileEnd) {
        /* Find end of line */
        char* lineEnd = line;
        while (lineEnd < fileEnd && *lineEnd != '\r' && *lineEnd != '\n') {
            lineEnd++;
        }
        *lineEnd = 0;

        /* Skip to next line */
        char* nextLine = lineEnd + 1;
        while (nextLine < fileEnd && (*nextLine == '\r' || *nextLine == '\n')) {
            nextLine++;
        }

        /* Trim the line */
        char* trimmed = TrimInPlace(line);

        /* Skip empty lines and comments */
        if (*trimmed == 0 || *trimmed == ';') {
            line = nextLine;
            continue;
        }

        /* Find the = separator */
        char* eq = strchr(trimmed, '=');
        if (eq == NULL) {
            line = nextLine;
            continue;
        }

        /* Split into key and value */
        *eq = 0;
        char* key = TrimInPlace(trimmed);
        char* value = TrimInPlace(eq + 1);

        /* Decode escape sequences in value */
        DecodeEscapes(value);

        /* Handle LANG_NAME specially */
        if (strcmp(key, "LANG_NAME") == 0) {
            strncpy_s(g_lang.langName, value, _TRUNCATE);
            line = nextLine;
            continue;
        }

        /* Store the entry */
        if (entryCount < lineCount) {
            newEntries[entryCount].key = key;
            newEntries[entryCount].value = value;
            entryCount++;
        }

        line = nextLine;
    }

    /* Now we need to make a persistent copy of the string data.
       The temporary buffer (buf) contains all the strings, but we need
       to keep them alive. We'll copy the entire buffer and remap pointers. */
    size_t poolSize = bytesRead + 1;
    char* pool = (char*)malloc(poolSize);
    if (pool == NULL) {
        free(newEntries);
        free(buf);
        return false;
    }
    memcpy(pool, buf, bytesRead + 1);

    /* Remap pointers from buf-relative to pool-relative */
    ptrdiff_t offset = pool - buf;
    for (int i = 0; i < entryCount; i++) {
        newEntries[i].key += offset;
        newEntries[i].value += offset;
    }

    /* Sort entries by key for binary search */
    qsort(newEntries, (size_t)entryCount, sizeof(LangEntry), LangEntryCompare);

    /* Commit */
    LangCleanup(); /* Free any previous data */
    g_lang.entries = newEntries;
    g_lang.count = entryCount;
    g_lang.capacity = lineCount;
    g_lang.stringPool = pool;
    g_lang.stringPoolSz = poolSize;
    g_lang.initialized = true;

    /* Free the temporary parse buffer */
    free(buf);

    return true;
}

/* ---------------------------------------------------------------------------
 * Internal: get the KailleraLang folder path (next to the DLL)
 * --------------------------------------------------------------------------- */
extern HINSTANCE hx; /* DLL instance, from kailleraclient.cpp */

static bool GetLangFolderPath(char* outPath, size_t outSize) {
    char dllPath[MAX_PATH];
    GetModuleFileNameA(hx, dllPath, MAX_PATH);

    /* Strip filename to get directory */
    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = 0;
    } else {
        dllPath[0] = 0;
    }

    /* Append KailleraLang\\ */
    if (strlen(dllPath) + strlen("KailleraLang\\") + 1 >= outSize) {
        return false;
    }
    strcpy_s(outPath, outSize, dllPath);
    strcat_s(outPath, outSize, "KailleraLang\\");

    return true;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void LangInit() {
    /* Read the preferred language from settings */
    nSettings::Initialize("okai");
    char langFile[64];
    nSettings::get_str("Lang", langFile, "English");
    nSettings::Terminate();

    /* Build the path to the .lng file */
    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return;
    }

    /* Ensure the KailleraLang directory exists */
    CreateDirectoryA(langPath, NULL);

    size_t basePathLen = strlen(langPath);
    strcat_s(langPath, sizeof(langPath), langFile);
    strcat_s(langPath, sizeof(langPath), ".lng");

    /* Try to load the requested language */
    if (!LangLoadFile(langPath)) {
        /* Fall back to English */
        langPath[basePathLen] = 0;
        strcat_s(langPath, sizeof(langPath), "English.lng");
        if (!LangLoadFile(langPath)) {
            /* Last resort: continue with empty strings; LNG() will return keys */
        }
    }
}

void LangShutdown() {
    LangCleanup();
}

const char* LangGet(const char* key) {
    if (key == NULL) return "";

    if (g_lang.initialized && g_lang.count > 0) {
        /* Binary search for the key */
        LangEntry needle;
        needle.key = key;
        needle.value = NULL;

        LangEntry* found = (LangEntry*)bsearch(
            &needle,
            g_lang.entries,
            (size_t)g_lang.count,
            sizeof(LangEntry),
            LangEntryCompare
        );

        if (found) {
            return found->value;
        }
    }

    /* Fallback: return the key itself so the UI shows something readable */
    return key;
}

const char* LangGetName() {
    if (g_lang.langName[0] != 0) {
        return g_lang.langName;
    }
    return "English";
}

int LangGetCount() {
    return g_lang.count;
}

int LangEnumerate(LangEnumCallback callback, void* userdata) {
    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return 0;
    }

    int count = 0;
    char searchPath[MAX_PATH];
    strcpy_s(searchPath, sizeof(searchPath), langPath);
    strcat_s(searchPath, sizeof(searchPath), "*.lng");

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            /* Strip .lng extension */
            char name[MAX_PATH];
            strcpy_s(name, sizeof(name), findData.cFileName);
            char* dot = strrchr(name, '.');
            if (dot && _stricmp(dot, ".lng") == 0) {
                *dot = 0;
            }
            if (callback) {
                callback(name, userdata);
            }
            count++;
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return count;
}

bool LangSetLanguage(const char* langName) {
    if (langName == NULL) return false;

    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return false;
    }

    size_t basePathLen = strlen(langPath);
    strcat_s(langPath, sizeof(langPath), langName);
    strcat_s(langPath, sizeof(langPath), ".lng");

    if (LangLoadFile(langPath)) {
        /* Save the preference */
        nSettings::Initialize("okai");
        nSettings::set_str("Lang", (char*)langName);
        nSettings::Terminate();
        return true;
    }

    return false;
}
