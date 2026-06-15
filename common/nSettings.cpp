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
char* nSettings::get_str(const char * key, char * buf, const char * def_){
        GetPrivateProfileString(subm, key, def_, buf, 128, file);
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
