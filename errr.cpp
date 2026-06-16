/******************************************************************************
***********   .d8888b.   .d8888b    *******************************************
**********   d88P  Y88b d88P  Y88b   ******************************************
*            888    888        888   ******************************************
*    8888b.  888    888      .d88P   ******************************************
*   888 "88b 888    888  .od888P"  ********************************************
*   888  888 888    888 d88P"    **********************************************
*   888  888 Y88b  d88P 888"        *******************************************
*   888  888  "Y8888P"  888888888            Open Kaillera: http://okai.sf.net
******************************************************************************/
#include "errr.h"
#ifndef KAILLERA
#define KAILLERA
#endif
#include "uihlp.h"
#include "resource.h"
#include "common/kaillera_lang.h"
#include "common/kaillera_lang_dlg.h"
#include "n02_theme.h"


#include <cstdio>
#include <limits.h>

//#include "common/nprintf.h"
// our most sexing debug/logging function
void __cdecl kprintf(const char * arg_0, ...) {
        //print on console
#if 1
        char V88[2048];
        va_list args;
        va_start(args, arg_0);
        V88[0] = 0;
        if (arg_0 != NULL)
                vsnprintf_s(V88, sizeof(V88), _TRUNCATE, arg_0, args);
        va_end(args);

        //log to file
#if 1
        static HFILE hx = HFILE_ERROR;

                if (hx == HFILE_ERROR) {
                        OFSTRUCT of;
                        hx = OpenFile("keaa.txt", &of, OF_CREATE|OF_WRITE);
                }
                size_t len = strlen(V88);
                UINT writeLen = (len > (size_t)UINT_MAX) ? UINT_MAX : (UINT)len;
                _lwrite(hx, V88, writeLen);
        #endif
        #endif
        }

typedef struct {
        const char * file;
        int line;
} n02_TRACE_st;

#define n02_TRACE_stack_len 16
#define n02_TRACE_stack_mask 15

n02_TRACE_st n02_TRACE_stack[n02_TRACE_stack_len];
unsigned int n02_TRACE_stack_pos = 0;

void _n02_TRACE(const char * file, int line){
        n02_TRACE_stack[n02_TRACE_stack_pos&n02_TRACE_stack_mask].file = file;
        n02_TRACE_stack[n02_TRACE_stack_pos&n02_TRACE_stack_mask].line = line;
        n02_TRACE_stack_pos++;
}


//void DumpTrace(){
//      char EBX[2000];
//      kprintf("Dumping trace");
//      for (unsigned int x = 1; x <= n02_TRACE_stack_len && x <=n02_TRACE_stack_pos ; x++ ) {
//              wsprintf(EBX, "%u: %s[%i]\r\n", (n02_TRACE_stack_pos-x), n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].file, n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].line);
//              kprintf(EBX);
//      }
//}

extern HINSTANCE hx;

static char ExceptionCodeToStr_t[64];

const char * ExceptionCodeToStr(int code) {
        if (code==EXCEPTION_ACCESS_VIOLATION) return LNG(EXCEPTION_ACCESS_VIOLATION);
        if (code==EXCEPTION_ARRAY_BOUNDS_EXCEEDED) return LNG(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
        if (code==EXCEPTION_BREAKPOINT) return LNG(EXCEPTION_BREAKPOINT);
        if (code==EXCEPTION_DATATYPE_MISALIGNMENT) return LNG(EXCEPTION_DATATYPE_MISALIGNMENT);
        if (code==EXCEPTION_FLT_DENORMAL_OPERAND) return LNG(EXCEPTION_FLT_DENORMAL_OPERAND);
        if (code==EXCEPTION_FLT_DIVIDE_BY_ZERO) return LNG(EXCEPTION_FLT_DIVIDE_BY_ZERO);
        if (code==EXCEPTION_FLT_INEXACT_RESULT) return LNG(EXCEPTION_FLT_INEXACT_RESULT);
        if (code==EXCEPTION_FLT_INVALID_OPERATION) return LNG(EXCEPTION_FLT_INVALID_OPERATION);
        if (code==EXCEPTION_FLT_OVERFLOW) return LNG(EXCEPTION_FLT_OVERFLOW);
        if (code==EXCEPTION_FLT_STACK_CHECK) return LNG(EXCEPTION_FLT_STACK_CHECK);
        if (code==EXCEPTION_FLT_UNDERFLOW) return LNG(EXCEPTION_FLT_UNDERFLOW);
        if (code==EXCEPTION_ILLEGAL_INSTRUCTION) return LNG(EXCEPTION_ILLEGAL_INSTRUCTION);
        if (code==EXCEPTION_IN_PAGE_ERROR) return LNG(EXCEPTION_IN_PAGE_ERROR);
        if (code==EXCEPTION_INT_DIVIDE_BY_ZERO) return LNG(EXCEPTION_INT_DIVIDE_BY_ZERO);
        if (code==EXCEPTION_INT_OVERFLOW) return LNG(EXCEPTION_INT_OVERFLOW);
        if (code==EXCEPTION_INVALID_DISPOSITION) return LNG(EXCEPTION_INVALID_DISPOSITION);
        if (code==EXCEPTION_NONCONTINUABLE_EXCEPTION) return LNG(EXCEPTION_NONCONTINUABLE_EXCEPTION);
        if (code==EXCEPTION_PRIV_INSTRUCTION) return LNG(EXCEPTION_PRIV_INSTRUCTION);
        if (code==EXCEPTION_SINGLE_STEP) return LNG(EXCEPTION_SINGLE_STEP);
        if (code==EXCEPTION_STACK_OVERFLOW) return LNG(EXCEPTION_STACK_OVERFLOW);
        wsprintf(ExceptionCodeToStr_t, "%i", code);
        return ExceptionCodeToStr_t;
}

LRESULT CALLBACK ErrorReporterDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
                case WM_INITDIALOG:
                        ApplyDialogLanguage(hDlg, N02_ERRORDLG);
                        if (lParam != 0) {
                                _EXCEPTION_POINTERS *EI = (_EXCEPTION_POINTERS*)lParam;
                                char EBX[2000];
                                wsprintf(EBX, LNG(EXCEPTION_RECORD_FORMAT),
                                                                ExceptionCodeToStr(EI->ExceptionRecord->ExceptionCode),
                                                                EI->ExceptionRecord->ExceptionFlags,
                                                                EI->ExceptionRecord->ExceptionAddress,
                                                                (EI->ExceptionRecord->ExceptionInformation!=0 && EI->ExceptionRecord->NumberParameters>0)? EI->ExceptionRecord->ExceptionInformation[0]:-1
                                                                );

                                HWND ct = GetDlgItem(hDlg, TXT_CHAT);

                                re_append(ct, EBX, 0x000000FF);
                                kprintf(EBX);
//#ifdef TRACE
                                re_append(ct, LNG(TRACE_STACK_HEADER), 0x0000FF00);
                                kprintf(LNG(TRACE_STACK_HEADER));
                                for (unsigned int x = 1; x <= n02_TRACE_stack_len && x <=n02_TRACE_stack_pos ; x++ ) {
                                        wsprintf(EBX, "%u: %s[%i]\r\n", (n02_TRACE_stack_pos-x), n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].file, n02_TRACE_stack[(n02_TRACE_stack_pos-x)&n02_TRACE_stack_mask].line);
                                        kprintf(EBX);
                                        re_append(ct, EBX, 0x00FF0000);
                                }
//#endif
                                //flash_window(hDlg);

                                SendMessage(ct, WM_VSCROLL, SB_TOP, 0);

                        }
                        Theme_OnInitDialog(hDlg);
                        break;
                case WM_CLOSE:
                        EndDialog(hDlg, 0);
                        break;
                case WM_COMMAND:
                        if (LOWORD(wParam)==BTN_REPORT){

                        } else if (LOWORD(wParam)==BTN_CLOSE) {
                                SendMessage(hDlg, WM_CLOSE, 0, 0);
                        } else if (LOWORD(wParam)==BTN_LEAVE){
                                ExitProcess(1);
                        }
                        break;
        case WM_LANG_CHANGED:
                ApplyDialogLanguage(hDlg, N02_ERRORDLG);
                break;
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLOREDIT:
                {
                        HBRUSH hBrush = Theme_HandleCtlColor(hDlg, (HDC)wParam, (HWND)lParam);
                        if (hBrush) return (LRESULT)hBrush;
                }
                break;
        case WM_ERASEBKGND:
                if (Theme_HandleEraseBkgnd(hDlg, (HDC)wParam))
                        return 1;
                break;
        case WM_DRAWITEM:
                /* Owner-draw push buttons (dark mode). */
                if (Theme_DrawButton((LPDRAWITEMSTRUCT)lParam))
                        return TRUE;
                break;
        case WM_THEME_CHANGED:
                /* Live theme toggle — re-apply dark title bar, control
                 * styles and repaint. */
                return Theme_HandleThemeChanged(hDlg);
        };
        return 0;
}

int n02ExceptionFilterFunction(_EXCEPTION_POINTERS *ExceptionInfo) {
        DialogBoxParam(hx, (LPCTSTR)N02_ERRORDLG, 0, (DLGPROC)ErrorReporterDialogProc, (LPARAM)ExceptionInfo);
        return EXCEPTION_EXECUTE_HANDLER;
}
