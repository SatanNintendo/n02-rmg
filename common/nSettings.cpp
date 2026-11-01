/******************************************************************************
***  n02 v0.3 winnt                                                         ***
**   Open Kaillera Client Core                                               **
***  For latest sources, visit http://sf.net/projects/okai                  ***
******************************************************************************/
#include "nSettings.h"

#include <windows.h>

char file[5000];
const char * subm;

// DLL instance handle from kailleraclient.cpp
extern HINSTANCE hx;

// Builds the full path of n02.ini (DLL directory) if Initialize() has not done it yet.
static void ensure_file(){
        if (file[0] != 0)
                return;
        GetModuleFileName(hx, file, 5000);
        char* lastSlash = strrchr(file, '\\');
        if (lastSlash) {
                *(lastSlash + 1) = 0;
        } else {
                file[0] = 0;
        }
        strcat(file, "n02.ini");
}

void nSettings::Initialize(const char * submo, bool global){
        file[0] = 0;
        if (!global) {
                // Get the DLL's directory instead of current working directory
                GetModuleFileName(hx, file, 5000);
                // Strip the filename to get just the directory
                char* lastSlash = strrchr(file, '\\');
                if (lastSlash) {
                        *(lastSlash + 1) = 0;
                } else {
                        file[0] = 0;
                }
        }
        strcat(file, "n02.ini");
        subm = submo;
}

void nSettings::Terminate(){

}
int nSettings::get_int(const char * key, int def_){
        return GetPrivateProfileInt(subm, key, def_, file);
}
char* nSettings::get_str(const char * key, char * buf, const char * def_, int bufsize){
        if (buf == 0 || bufsize <= 0)
                return buf;
        GetPrivateProfileString(subm, key, def_, buf, bufsize, file);
        return buf;
}
void nSettings::set_int(const char * key, int val){
        char bft [128];
        wsprintf(bft, "%i", val);
        WritePrivateProfileString(subm, key, bft, file);
}
void nSettings::set_str(const char * key, const char * val){
        WritePrivateProfileString(subm, key, val, file);
}

// ---- explicit-section access ------------------------------------------------
// These do NOT touch the "current section" selected by Initialize(), so they can
// be used from anywhere without disturbing code that relies on it.
int nSettings::get_int_in(const char * section, const char * key, int def_){
        ensure_file();
        return GetPrivateProfileInt(section, key, def_, file);
}
void nSettings::set_int_in(const char * section, const char * key, int val){
        ensure_file();
        char bft [128];
        wsprintf(bft, "%i", val);
        WritePrivateProfileString(section, key, bft, file);
}
void nSettings::clear_section(const char * section){
        ensure_file();
        // lpKeyName == NULL deletes the whole section
        WritePrivateProfileString(section, NULL, NULL, file);
}
