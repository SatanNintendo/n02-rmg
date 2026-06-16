/*
 * n02_theme.cpp - Dark/Light theme implementation for N02 Kaillera Client
 *
 * Implements a non-destructive dark theme that overlays on top of
 * the existing Win32 dialog system. Light theme (default) makes zero
 * changes to the original appearance.
 */

#include "n02_theme.h"
#include "common/nSettings.h"
#include "common/kaillera_lang.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

/* ============================================================================
 * Color Palette
 * ============================================================================ */

namespace ThemeColors {
        /* Dark theme palette - inspired by VS Code / Windows Dark Mode */
        const COLORREF DarkBg         = RGB(30, 30, 30);       /* #1E1E1E */
        const COLORREF DarkBgAlt      = RGB(45, 45, 45);       /* #2D2D2D - group boxes, headers */
        const COLORREF DarkText        = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkTextDim     = RGB(153, 153, 153);    /* #999999 - disabled/secondary */
        const COLORREF DarkBorder      = RGB(63, 63, 70);       /* #3F3F46 */
        const COLORREF DarkHighlight   = RGB(0, 122, 204);      /* #007ACC - accent */
        const COLORREF DarkEditBg      = RGB(37, 37, 38);       /* #252526 */
        const COLORREF DarkBtnBg       = RGB(51, 51, 51);       /* #333333 */
        const COLORREF DarkBtnText     = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkSelBg       = RGB(0, 122, 204);      /* #007ACC */
        const COLORREF DarkSelText     = RGB(255, 255, 255);    /* #FFFFFF */
        const COLORREF DarkListBg      = RGB(30, 30, 30);       /* #1E1E1E */
        const COLORREF DarkListText    = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkListSelBg   = RGB(0, 122, 204);      /* #007ACC */
        const COLORREF DarkListSelText = RGB(255, 255, 255);    /* #FFFFFF */
        const COLORREF DarkHeaderBg    = RGB(45, 45, 45);       /* #2D2D2D */
        const COLORREF DarkHeaderText  = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkRichBg      = RGB(30, 30, 30);       /* #1E1E1E */
        const COLORREF DarkRichText    = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkTabBg       = RGB(45, 45, 45);       /* #2D2D2D */
        const COLORREF DarkTabActiveBg = RGB(30, 30, 30);       /* #1E1E1E */
        const COLORREF DarkTabText     = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkComboBg     = RGB(37, 37, 38);       /* #252526 */
        const COLORREF DarkComboText   = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkGroupBox    = RGB(212, 212, 212);    /* #D4D4D4 */
        const COLORREF DarkCheckBoxBg  = RGB(30, 30, 30);       /* #1E1E1E */
        const COLORREF DarkStaticText  = RGB(212, 212, 212);    /* #D4D4D4 */
}

/* ============================================================================
 * Global State
 * ============================================================================ */

bool g_dark_mode = false;

/* Cached brushes - created lazily, deleted in Theme_Shutdown() */
static HBRUSH s_brushDarkBg       = NULL;
static HBRUSH s_brushDarkEditBg   = NULL;
static HBRUSH s_brushDarkBtnBg    = NULL;
static HBRUSH s_brushDarkListBg   = NULL;
static HBRUSH s_brushDarkHeaderBg = NULL;
static HBRUSH s_brushDarkTabBg    = NULL;
static HBRUSH s_brushDarkComboBg  = NULL;
static HBRUSH s_brushDarkRichBg   = NULL;
static HBRUSH s_brushDarkChkBg    = NULL;
static bool   s_initialized       = false;

/* Custom WM_USER message for theme change notification */
#define WM_THEME_CHANGED (WM_USER + 0x200)

/* ============================================================================
 * Brush Management
 * ============================================================================ */

static void EnsureBrushes() {
        if (s_brushDarkBg != NULL)
                return;
        s_brushDarkBg       = CreateSolidBrush(ThemeColors::DarkBg);
        s_brushDarkEditBg   = CreateSolidBrush(ThemeColors::DarkEditBg);
        s_brushDarkBtnBg    = CreateSolidBrush(ThemeColors::DarkBtnBg);
        s_brushDarkListBg   = CreateSolidBrush(ThemeColors::DarkListBg);
        s_brushDarkHeaderBg = CreateSolidBrush(ThemeColors::DarkHeaderBg);
        s_brushDarkTabBg    = CreateSolidBrush(ThemeColors::DarkTabBg);
        s_brushDarkComboBg  = CreateSolidBrush(ThemeColors::DarkComboBg);
        s_brushDarkRichBg   = CreateSolidBrush(ThemeColors::DarkRichBg);
        s_brushDarkChkBg    = CreateSolidBrush(ThemeColors::DarkCheckBoxBg);
}

static void DeleteBrushes() {
        if (s_brushDarkBg)       { DeleteObject(s_brushDarkBg);       s_brushDarkBg = NULL; }
        if (s_brushDarkEditBg)   { DeleteObject(s_brushDarkEditBg);   s_brushDarkEditBg = NULL; }
        if (s_brushDarkBtnBg)    { DeleteObject(s_brushDarkBtnBg);    s_brushDarkBtnBg = NULL; }
        if (s_brushDarkListBg)   { DeleteObject(s_brushDarkListBg);   s_brushDarkListBg = NULL; }
        if (s_brushDarkHeaderBg) { DeleteObject(s_brushDarkHeaderBg); s_brushDarkHeaderBg = NULL; }
        if (s_brushDarkTabBg)    { DeleteObject(s_brushDarkTabBg);    s_brushDarkTabBg = NULL; }
        if (s_brushDarkComboBg)  { DeleteObject(s_brushDarkComboBg);  s_brushDarkComboBg = NULL; }
        if (s_brushDarkRichBg)   { DeleteObject(s_brushDarkRichBg);   s_brushDarkRichBg = NULL; }
        if (s_brushDarkChkBg)    { DeleteObject(s_brushDarkChkBg);    s_brushDarkChkBg = NULL; }
}

/* ============================================================================
 * Control Type Detection
 * ============================================================================ */

static bool IsEditControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "Edit") == 0);
}

static bool IsRichEditControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strncmp(className, "RichEdit", 8) == 0 ||
                strncmp(className, "RICHEDIT", 8) == 0);
}

static bool IsListViewControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "SysListView32") == 0);
}

static bool IsButtonControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "Button") == 0);
}

static bool IsComboBoxControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "ComboBox") == 0 ||
                strcmp(className, "ComboBoxEx32") == 0);
}

static bool IsTabControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "SysTabControl32") == 0);
}

static bool IsStaticControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "Static") == 0);
}

static bool IsScrollBarControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "ScrollBar") == 0);
}

static bool IsListBoxControl(HWND hwnd) {
        char className[32];
        GetClassNameA(hwnd, className, sizeof(className));
        return (strcmp(className, "ListBox") == 0);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void Theme_Init() {
        nSettings::Initialize("okai");
        g_dark_mode = (nSettings::get_int("DarkTheme", 0) != 0);
        nSettings::Terminate();
        s_initialized = true;
        if (g_dark_mode) {
                EnsureBrushes();
        }
}

void Theme_Save() {
        nSettings::Initialize("okai");
        nSettings::set_int("DarkTheme", g_dark_mode ? 1 : 0);
        nSettings::Terminate();
}

void Theme_Shutdown() {
        DeleteBrushes();
        s_initialized = false;
}

bool Theme_IsDark() {
        return g_dark_mode;
}

void Theme_Toggle() {
        g_dark_mode = !g_dark_mode;
        if (g_dark_mode) {
                EnsureBrushes();
        }
        Theme_Save();
        Theme_NotifyChanged();
}

void Theme_NotifyChanged() {
        /* Send WM_THEME_CHANGED to all top-level windows on the current thread */
        EnumThreadWindows(GetCurrentThreadId(), [](HWND hwnd, LPARAM lParam) -> BOOL {
                SendMessageTimeout(hwnd, WM_THEME_CHANGED, 0, 0, SMTO_ABORTIFHUNG, 100, NULL);
                /* Also send to children */
                EnumChildWindows(hwnd, [](HWND child, LPARAM lParam2) -> BOOL {
                        /* Some controls need to know about theme change */
                        InvalidateRect(child, NULL, TRUE);
                        return TRUE;
                }, 0);
                InvalidateRect(hwnd, NULL, TRUE);
                return TRUE;
        }, 0);
}

/* ============================================================================
 * Apply Theme to Specific Controls
 * ============================================================================ */

void Theme_ApplyToListView(HWND hListView) {
        if (!g_dark_mode || hListView == NULL)
                return;

        ListView_SetBkColor(hListView, ThemeColors::DarkListBg);
        ListView_SetTextColor(hListView, ThemeColors::DarkListText);
        ListView_SetTextBkColor(hListView, ThemeColors::DarkListBg);

        /* Set selected item colors */
        ListView_SetSelectedColumn(hListView, -1);

        /* Header control dark theme */
        HWND hHeader = ListView_GetHeader(hListView);
        if (hHeader != NULL) {
                /* Use custom draw for header instead of SetBkColor which may not work on all versions */
        }
}

void Theme_ApplyToRichEdit(HWND hRichEdit) {
        if (!g_dark_mode || hRichEdit == NULL)
                return;

        SendMessage(hRichEdit, EM_SETBKGNDCOLOR, 0, (LPARAM)ThemeColors::DarkRichBg);
        /* Default text color for new text */
        CHARFORMATA cf;
        memset(&cf, 0, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = ThemeColors::DarkRichText;
        SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

void Theme_ApplyToDialogChildren(HWND hDlg) {
        if (!g_dark_mode || hDlg == NULL)
                return;

        EnumChildWindows(hDlg, [](HWND child, LPARAM lParam) -> BOOL {
                if (IsListViewControl(child)) {
                        Theme_ApplyToListView(child);
                } else if (IsRichEditControl(child)) {
                        Theme_ApplyToRichEdit(child);
                } else if (IsEditControl(child)) {
                        /* Edit controls get styled via WM_CTLCOLOR */
                } else if (IsTabControl(child)) {
                        /* Tab controls are styled via WM_CTLCOLOR */
                }
                return TRUE;
        }, 0);
}

void Theme_OnInitDialog(HWND hDlg) {
        if (!g_dark_mode || hDlg == NULL)
                return;

        EnsureBrushes();

        /* Apply dark theme to all child controls */
        Theme_ApplyToDialogChildren(hDlg);

        /* Force a repaint of the entire dialog */
        InvalidateRect(hDlg, NULL, TRUE);
}

void Theme_OnDestroy(HWND hDlg) {
        /* Currently no per-dialog cleanup needed */
        (void)hDlg;
}

/* ============================================================================
 * WM_CTLCOLOR Handler
 * ============================================================================ */

HBRUSH Theme_HandleCtlColor(HWND hDlg, HDC hdc, HWND childHwnd) {
        if (!g_dark_mode)
                return NULL;

        EnsureBrushes();

        /* RichEdit controls */
        if (IsRichEditControl(childHwnd)) {
                SetBkColor(hdc, ThemeColors::DarkRichBg);
                SetTextColor(hdc, ThemeColors::DarkRichText);
                return s_brushDarkRichBg;
        }

        /* Edit controls (single-line, password, etc.) */
        if (IsEditControl(childHwnd)) {
                SetBkColor(hdc, ThemeColors::DarkEditBg);
                SetTextColor(hdc, ThemeColors::DarkText);
                return s_brushDarkEditBg;
        }

        /* ListView controls */
        if (IsListViewControl(childHwnd)) {
                SetBkColor(hdc, ThemeColors::DarkListBg);
                SetTextColor(hdc, ThemeColors::DarkListText);
                return s_brushDarkListBg;
        }

        /* ComboBox controls */
        if (IsComboBoxControl(childHwnd)) {
                SetBkColor(hdc, ThemeColors::DarkComboBg);
                SetTextColor(hdc, ThemeColors::DarkComboText);
                return s_brushDarkComboBg;
        }

        /* ListBox controls */
        if (IsListBoxControl(childHwnd)) {
                SetBkColor(hdc, ThemeColors::DarkListBg);
                SetTextColor(hdc, ThemeColors::DarkListText);
                return s_brushDarkListBg;
        }

        /* Button controls (push buttons, checkboxes, radio buttons, group boxes) */
        if (IsButtonControl(childHwnd)) {
                LONG style = GetWindowLong(childHwnd, GWL_STYLE) & 0xF;
                if (style == BS_GROUPBOX) {
                        /* Group boxes: just set text color, brush for parent background */
                        SetTextColor(hdc, ThemeColors::DarkGroupBox);
                        SetBkColor(hdc, ThemeColors::DarkBg);
                        return s_brushDarkBg;
                } else if (style == BS_AUTOCHECKBOX || style == BS_CHECKBOX ||
                           style == BS_AUTORADIOBUTTON || style == BS_RADIOBUTTON) {
                        /* Checkboxes and radio buttons */
                        SetTextColor(hdc, ThemeColors::DarkText);
                        SetBkColor(hdc, ThemeColors::DarkCheckBoxBg);
                        return s_brushDarkChkBg;
                } else {
                        /* Push buttons */
                        SetTextColor(hdc, ThemeColors::DarkBtnText);
                        SetBkColor(hdc, ThemeColors::DarkBtnBg);
                        return s_brushDarkBtnBg;
                }
        }

        /* Tab controls */
        if (IsTabControl(childHwnd)) {
                SetTextColor(hdc, ThemeColors::DarkTabText);
                SetBkColor(hdc, ThemeColors::DarkTabBg);
                return s_brushDarkTabBg;
        }

        /* Static text controls */
        if (IsStaticControl(childHwnd)) {
                SetTextColor(hdc, ThemeColors::DarkStaticText);
                SetBkColor(hdc, ThemeColors::DarkBg);
                /* Make the static control background transparent so it blends with the dialog */
                SetBkMode(hdc, TRANSPARENT);
                return s_brushDarkBg;
        }

        /* ScrollBar controls - just set background */
        if (IsScrollBarControl(childHwnd)) {
                SetBkColor(hdc, ThemeColors::DarkBg);
                return s_brushDarkBg;
        }

        /* Default: use dialog background color */
        SetTextColor(hdc, ThemeColors::DarkText);
        SetBkColor(hdc, ThemeColors::DarkBg);
        return s_brushDarkBg;
}

/* ============================================================================
 * WM_ERASEBKGND Handler
 * ============================================================================ */

bool Theme_HandleEraseBkgnd(HWND hDlg, HDC hdc) {
        if (!g_dark_mode)
                return false;

        EnsureBrushes();

        RECT rc;
        GetClientRect(hDlg, &rc);
        FillRect(hdc, &rc, s_brushDarkBg);
        return true;
}
