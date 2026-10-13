#include "kailleraclient.h"
#include <windows.h>
#include <time.h>
#include <cstdio>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include "string.h"


#include "p2p_ui.h"
#include "kaillera_ui.h"
#include "player.h"

#include "common/nThread.h"
#include "common/k_socket.h"
#include "common/nSettings.h"

#include "errr.h"
#include <limits.h>
#include <stddef.h>
#include "common/kaillera_lang.h"

#include "n02_theme.h"


#define KAILLERA_DLLEXP __declspec(dllexport) __stdcall

#define KAILLERA

#define RECORDER


HINSTANCE hx;

//void __cdecl kprintf(char * arg_0, ...) {
//      char V8[1024];
//      time_t V4 = time(0);
//      tm * ecx = localtime(&V4);
//      sprintf(V8, "%02d/%02d/%02d-%02d:%02d:%02d> %s\r\n",ecx->tm_mday,ecx->tm_mon,ecx->tm_year % 0x64,ecx->tm_hour,ecx->tm_min,ecx->tm_sec, arg_0);
//      va_list args;
//      va_start (args, arg_0);
//      vprintf (V8, args);
//      va_end (args);
//}

//void OutputHexx(const void * inb, int len, bool usespace){
//      char outbb[2000];
//      char * outb = outbb;
//      char HEXDIGITS [] = {
//              '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
//      };
//      char * xx = (char*)inb;
//      for (int x = 0; x <     len; x++) {
//              int dx = *xx++;
//              int hib = (dx & 0xF0)>>4;
//              int lob = (dx & 0x0F);
//              *outb++ = HEXDIGITS[hib];
//              *outb++ = HEXDIGITS[lob];
//              if (usespace)
//                      *outb++ = ' ';
//      }
//      *outb = 0;
//      printf(outbb);
//}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

kailleraInfos infos_copy;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef KAILLERA
typedef struct {
        void (*GUI)();
        bool (*SSDSTEP)();
        int  (*MPV)(void*,int);
        void (*ChatSend)(const char*);
        void (*EndGame)();
        bool (*RecordingEnabled)();
}n02_MODULE;

n02_MODULE active_mod;

n02_MODULE mod_kaillera;
n02_MODULE mod_p2p;
n02_MODULE mod_playback;
int mod_active_no = -1;
int active_mod_index;
bool mod_rerun;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef RECORDER
char convert[256];
HANDLE out = INVALID_HANDLE_VALUE;
char recording_player_names[4][32];

// Protects 'out' and RecordingBuffer. The chat / drop callbacks run on the GUI
// thread while kailleraModifyPlayValues() runs on the emulator thread, so both
// used to write into the same buffer concurrently.
class RecordingLockC {
public:
        CRITICAL_SECTION cs;
        RecordingLockC() { InitializeCriticalSection(&cs); }
        void lock() { EnterCriticalSection(&cs); }
        void unlock() { LeaveCriticalSection(&cs); }
};
static RecordingLockC RecordingLock;

class RecordingLocker {
public:
        RecordingLocker() { RecordingLock.lock(); }
        ~RecordingLocker() { RecordingLock.unlock(); }
};

class RecordingBufferC {
public:
        // Was 500 bytes with no bounds checking at all: a long chat message
        // (nick + text) overflowed it and corrupted the heap/globals.
        char buffer[4096];

        char* position;

        void reset(){
                position = buffer;
        }

        int len(){
                ptrdiff_t diff = position - buffer;
                if (diff < 0)
                        return 0;
                if (diff > INT_MAX)
                        return INT_MAX;
                return (int)diff;
        }

        int space(){
                return (int)sizeof(buffer) - len();
        }

        void write(){
                if (out != INVALID_HANDLE_VALUE && len() > 0) {
                        DWORD written;
                        WriteFile(out, buffer, len(), &written, NULL);
                }
                reset();
        }

        // Appends n bytes to the recording stream. Never overruns 'buffer':
        // pending data is flushed first, and data bigger than the whole buffer
        // is written straight to the file.
        void put_record(const void* data, int n){
                if (data == NULL || n <= 0 || out == INVALID_HANDLE_VALUE)
                        return;
                if (n > space())
                        write();
                if (n > (int)sizeof(buffer)) {
                        DWORD written;
                        WriteFile(out, data, (DWORD)n, &written, NULL);
                        return;
                }
                memcpy(position, data, (size_t)n);
                position += n;
        }
}RecordingBuffer;

static void close_recording() {
        RecordingLocker lock;
        if (out != INVALID_HANDLE_VALUE) {
                if (RecordingBuffer.len() > 0)
                        RecordingBuffer.write();
                CloseHandle(out);
                out = INVALID_HANDLE_VALUE;
        }
}

int WINAPI _gameCallback(char *game, int player, int numplayers){

        close_recording();

        char recErr[1100];
        recErr[0] = 0;
        if (active_mod.RecordingEnabled()) {
                RecordingLocker lock;
                n02_TRACE();
                RecordingBuffer.reset();

                char FileName[2000];
                CreateDirectory("records", 0);

                // Build filename: YYMMDDHHMMSS.krec (player/game info is in the KRC1 header)
                time_t t = time(0);
                tm * lt = localtime(&t);
                char datePart[16];
                strftime(datePart, sizeof(datePart), "%y%m%d%H%M%S", lt);

                wsprintf(FileName,".\\records\\%s.krec", datePart);


                for(unsigned int x=0; x < strlen(FileName); x++){
                        FileName[x] = convert[(unsigned char)FileName[x]];
                }

                //open file
                out = CreateFile(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                if(out==INVALID_HANDLE_VALUE){
                        // Shown after the lock is released (a modal box must not stall
                        // the emulator thread, which needs the same lock every frame).
                        _snprintf_s(recErr, sizeof(recErr), _TRUNCATE, LNG(REC_FAILED), FileName, GetLastError());
                }
                else {

                DWORD written;
                char GameName[128];

                strncpy(GameName, (game != NULL) ? game : "", sizeof(GameName) - 1);
                GameName[sizeof(GameName) - 1] = 0;

                // The header field is a fixed 128 bytes. Reading 128 bytes straight
                // from the emulator's appName pointer over-reads short strings (and
                // can fault at a page boundary); APP is a zero padded 128 byte copy.
                WriteFile(out, "KRC1", 4, &written, NULL);
                WriteFile(out, APP, 128, &written, NULL);
                WriteFile(out, GameName, 128, &written, NULL);
                time_t mytime = time(NULL);
                WriteFile(out, (char*)&mytime, 4, &written, NULL);
                WriteFile(out, (char*)&player, 4, &written, NULL);
                WriteFile(out, (char*)&numplayers, 4, &written, NULL);
                WriteFile(out, (char*)recording_player_names, 128, &written, NULL);
                }
                n02_TRACE();
        }

        if (recErr[0] != 0)
                MessageBox(0, recErr, LNG(REC_ERROR_TITLE), 0);

        if (infos_copy.gameCallback)
                return infos_copy.gameCallback(game, player, numplayers);
        return 0;

}

        void WINAPI _chatReceivedCallback(char *nick, char *text){
                {
                        RecordingLocker lock;
                        if (out != INVALID_HANDLE_VALUE) {
                                // record: 8, nick\0, text\0 (text is truncated if it does not fit)
                                char rec[1 + 256 + 1024];
                                int n = 0;
                                rec[n++] = 8;
                                const char* nk = (nick != NULL) ? nick : "";
                                const char* tx = (text != NULL) ? text : "";
                                size_t nl = strlen(nk); if (nl > 255) nl = 255;
                                memcpy(rec + n, nk, nl); n += (int)nl; rec[n++] = 0;
                                size_t tl = strlen(tx); if (tl > 1023) tl = 1023;
                                memcpy(rec + n, tx, tl); n += (int)tl; rec[n++] = 0;
                                RecordingBuffer.put_record(rec, n);
                        }
                }
                if (infos_copy.chatReceivedCallback)
                        infos_copy.chatReceivedCallback(nick, text);
        }
        void WINAPI _clientDroppedCallback(char *nick, int playernb){
                {
                        RecordingLocker lock;
                        if (out != INVALID_HANDLE_VALUE) {
                                // record: 20, nick\0, playernb (4 bytes)
                                char rec[1 + 256 + 4];
                                int n = 0;
                                rec[n++] = 20;
                                const char* nk = (nick != NULL) ? nick : "";
                                size_t nl = strlen(nk); if (nl > 255) nl = 255;
                                memcpy(rec + n, nk, nl); n += (int)nl; rec[n++] = 0;
                                memcpy(rec + n, &playernb, 4); n += 4;
                                RecordingBuffer.put_record(rec, n);
                        }
                }
                if (infos_copy.clientDroppedCallback)
                        infos_copy.clientDroppedCallback(nick, playernb);
        }




#endif
#endif

void initialize_mode_cb(HWND hDlg){
#ifdef KAILLERA
        SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)LNG(MODE_1_P2P));
        SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)LNG(MODE_2_SERVER));
        SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)LNG(MODE_3_PLAYBACK));
        SendMessage(hDlg, CB_SETCURSEL, mod_active_no, 0);
        active_mod_index = mod_active_no;
#else
        SendMessage(hDlg, CB_ADDSTRING, 0, (WPARAM)LNG(MODE_P2P_ONLY));
        SendMessage(hDlg, CB_SETCURSEL, 0, 0);
#endif
}

bool activate_mode(int mode){
#ifdef KAILLERA
        if (mode != mod_active_no) {
                mod_active_no = mode;
                active_mod_index = mode;
                mod_rerun = true;

                if (mode == 0)
                        active_mod = mod_p2p;
                else if (mode == 1)
                        active_mod = mod_kaillera;
                else
                        active_mod = mod_playback;

                return true;
        }
#endif
        return false;
}

int get_active_mode_index(){
        return active_mod_index;
}

void loadSettings() {
        nSettings::Initialize("okai");
        active_mod_index = nSettings::get_int("AM", 1);
        nSettings::Terminate();
}

void saveSettings() {
        nSettings::Initialize("okai");
        nSettings::set_int("AM", active_mod_index);
        nSettings::Terminate();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
char * gamelist = 0;




int KSSDFAST [16] = {
        0, 0, 1, 1,
                1, 0, 1, 0,
                2, 0, 2, 0,
                3, 3, 3, 3
};

KSSDFA_ KSSDFA;


class GuiThread : public nThread {
public:
        bool running;
        void run(void) {

                running = true;

#ifdef KAILLERA

                /*

                BYTE kst[256];
                GetKeyboardState(kst);

                if (kst[VK_CAPITAL]==0)
                        active_mod = mod_p2p;
                else
                        active_mod = mod_kaillera;

                */

                __try {
                        mod_rerun = true;
                        do {
                                mod_rerun = false;
                                active_mod.GUI();
                        } while (mod_rerun);
                }
                __except (n02ExceptionFilterFunction(GetExceptionInformation())) {
                        MessageBox(GetActiveWindow(), LNG(GUI_THREAD_ERR_BODY), LNG(GUI_THREAD_ERR_TITLE), 0);
                }

#else
                p2p_GUI();
#endif

                KSSDFA.state = 3;
                running = false;
                //*/
        }
        void st(void * parent) {
                KSSDFA.state = create()!=0? 0:3;
        }
} gui_thread;



extern "C" {
        kailleraInfos infos;

        void KAILLERA_DLLEXP kailleraGetVersion(char *version){
                memcpy(version, "0.9", 4);
        }
        int KAILLERA_DLLEXP kailleraInit(){
                k_socket::Initialize();
                LangInit();
                Theme_Init();
                loadSettings();

#ifdef KAILLERA

                mod_playback.MPV = player_MPV;
                mod_playback.GUI = player_GUI;
                mod_playback.EndGame = player_EndGame;
                mod_playback.SSDSTEP = player_SSDSTEP;
                mod_playback.ChatSend = player_ChatSend;
                mod_playback.RecordingEnabled = player_RecordingEnabled;

                mod_kaillera.GUI = kaillera_GUI;
                mod_kaillera.SSDSTEP = kaillera_SelectServerDlgStep;
                mod_kaillera.MPV = kaillera_modify_play_values;
                mod_kaillera.ChatSend = kaillera_game_chat_send;
                mod_kaillera.EndGame = kaillera_end_game;
                mod_kaillera.RecordingEnabled = kaillera_RecordingEnabled;

                mod_p2p.GUI = p2p_GUI;
                mod_p2p.SSDSTEP = p2p_SelectServerDlgStep;
                mod_p2p.MPV = p2p_modify_play_values;
                mod_p2p.ChatSend = p2p_send_chat;
                mod_p2p.EndGame = p2p_EndGame;
                mod_p2p.RecordingEnabled = p2p_RecordingEnabled;

                activate_mode(active_mod_index);

#ifdef RECORDER


                {
                        for(int x = 0 ; x < 256; x++){
                                convert[x] = '_';
                        }
                }
                {
                        for(char x = 'A' ; x <= 'Z'; x++){
                                convert[x] = x;
                        }
                }
                {
                        for(char x = '0' ; x <= '9'; x++){
                                convert[x] = x;
                        }
                }
                {
                        for(char x = 'a' ; x <= 'z'; x++){
                                convert[x] = x;
                        }
                }
                        convert['['] = '[';
                        convert[0] = 0;
                        convert[']'] = ']';
                        convert['.'] = '.';
                        convert['\\'] = '\\';
                        convert['-'] = '-';



#endif



#endif

                return 0;
        }
        void KAILLERA_DLLEXP kailleraShutdown(){
                k_socket::Cleanup();
                LangShutdown();
                Theme_Save();
                Theme_Shutdown();
                saveSettings();
                if (gamelist != 0)
                        free(gamelist);
                gamelist = 0;
#ifdef KAILLERA
#ifdef RECORDER
                close_recording();
#endif
#endif
        }
                        void KAILLERA_DLLEXP kailleraSetInfos(kailleraInfos *infos_){
                                infos = *infos_;
                                strncpy(APP, (infos.appName != NULL) ? infos.appName : "", 127);
                                APP[127] = 0;

                                if (gamelist != 0)
                                        free(gamelist);
                        gamelist = 0;

                        char * xx = infos.gameList;
                        size_t l = 0;
                        if (xx != 0) {
                                size_t p;
                                while ((p=strlen(xx))!= 0){
                                        l += p + 1;
                                        xx += p + 1;
                                }
                                l++;
                                gamelist = (char*)malloc(l);
                        if (gamelist != 0)
                                memcpy(gamelist, infos.gameList, l);
                }
                infos.gameList = gamelist;
                infos_copy = infos;
#ifdef KAILLERA
#ifdef RECORDER
                infos.chatReceivedCallback = _chatReceivedCallback;
                infos.clientDroppedCallback = _clientDroppedCallback;
                infos.gameCallback = _gameCallback;
#endif
#endif
        }
        void KAILLERA_DLLEXP kailleraSelectServerDialog(void* parent){
                KSSDFA.state = 0;
                KSSDFA.input = 0;

                gui_thread.st(parent);

                nThread game_thread;
                game_thread.capture();
                game_thread.sleep(1);

                if (gui_thread.running) {
                        do {
                                __try {
                                        while (KSSDFA.state != 3) {
                                                int prev_state = KSSDFA.state;
                                                KSSDFA.state = KSSDFAST[((KSSDFA.state) << 2) | KSSDFA.input];
                                                KSSDFA.input = 0;
#ifdef RECORDER
                                                if (prev_state == 2 && KSSDFA.state != 2) {
                                                        close_recording();
                                                }
#endif
                                                if (KSSDFA.state == 2) {
                                                        MSG message;
                                                        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                                                                TranslateMessage(&message);
                                                                DispatchMessage(&message);
                                                        }
                                                }
                                                else if (KSSDFA.state == 1) {
                                                        //kprintf("call gamecallback");
                                                        KSSDFA.state = 2;
                                                        if (infos.gameCallback)
                                                                infos.gameCallback(GAME, playerno, numplayers);
                                                }
                                                else if (KSSDFA.state == 0) {
#ifdef RECORDER
                                                        close_recording();
#endif
                                                        MSG message;
                                                        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                                                                TranslateMessage(&message);
                                                                DispatchMessage(&message);
                                                        }
#ifndef KAILLERA
                                                        if (p2p_SelectServerDlgStep()) {
#else
                                                        if (active_mod.SSDSTEP()) {
#endif
                                                                continue;
                                                        }
                                                }

                                                // Use MsgWaitForMultipleObjects instead of sleep to avoid
                                                // sluggish window dragging while still yielding CPU
                                                MsgWaitForMultipleObjects(0, NULL, FALSE, 1, QS_ALLINPUT);
                                        }
                                }
                                __except (n02ExceptionFilterFunction(GetExceptionInformation())) {
                                        KSSDFA.state = 0;
                                }
                        } while (KSSDFA.state != 3);
                }

                // game_thread lives on this stack frame, but capture() put its address
                // into nThread::list and nothing ever removed it: a dangling pointer per
                // call, and after NTHREAD_MAX (256) calls the list overflowed.
                game_thread.release();
        }

        int  KAILLERA_DLLEXP kailleraModifyPlayValues(void *values, int size){
                //kaillera_core_debug("x");
                if (KSSDFA.state==2) {
#ifdef KAILLERA

#ifdef RECORDER

                        short siz = active_mod.MPV(values, size);

                        {
                                RecordingLocker lock;
                                if (out != INVALID_HANDLE_VALUE) {
                                        char hdr[3];
                                        hdr[0] = 0x12;
                                        memcpy(hdr + 1, &siz, 2);
                                        RecordingBuffer.put_record(hdr, 3);
                                        if (siz > 0)
                                                RecordingBuffer.put_record(values, siz);
                                        RecordingBuffer.write();
                                }
                        }
                        return siz;

#else
                        return active_mod.MPV(values, size);
#endif

#else
                        return p2p_modify_play_values(values, size);
#endif
                } else return -1;
        }
        void KAILLERA_DLLEXP kailleraChatSend(char *text){
#ifdef KAILLERA
                active_mod.ChatSend(text);
#else
                p2p_ui_chat_send(text);
#endif
        }
        void KAILLERA_DLLEXP kailleraEndGame(){
#ifdef KAILLERA
#ifdef RECORDER
                close_recording();
#endif
                active_mod.EndGame();
#else
                p2p_EndGame();
#endif

        }
};


BOOL APIENTRY DllMain (HINSTANCE hInst,
        DWORD reason,
        LPVOID){
        if(reason==DLL_PROCESS_ATTACH)
                hx = hInst;
        return TRUE;
}


extern "C" {
        __declspec(dllexport) int z00_With = 0;
        __declspec(dllexport) int z01_stupidity = 0;
        __declspec(dllexport) int z02_even = 0;
        __declspec(dllexport) int z03_the = 0;
        __declspec(dllexport) int z04_gods = 0;
        __declspec(dllexport) int z05_contend = 0;
        __declspec(dllexport) int z06_in = 0;
        __declspec(dllexport) int z07_vain = 0;
}
