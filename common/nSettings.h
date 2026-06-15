/******************************************************************************
***  n02 v0.3 winnt                                                         ***
**   Open Kaillera Client Core                                               **
***  For latest sources, visit http://sf.net/projects/okai                  ***
******************************************************************************/
#pragma once

class nSettings {
public:
        static void Initialize(const char * submo = "p2p", bool global = false);
        static void Terminate();
        static int get_int(const char * key, int def_ = -1);
        static char* get_str(const char * key, char * buf, const char * def_=0);
        static void set_int(const char * key, int val);
        static void set_str(const char * key, const char * val);
};
