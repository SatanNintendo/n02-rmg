/*
 * n02_theme.cpp - Dark/Light theme implementation for N02 Kaillera Client
 *
 * Implements a non-destructive dark theme that overlays on top of
 * the existing Win32 dialog system. Light theme (default) makes zero
 * changes to the original appearance.
 *
 * Key techniques used (per DARK_THEME_GUIDE.md):
 *  - DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE) for the title bar.
 *  - SetWindowTheme(child, L"", L"") for Edit/ListBox/ComboBox/ScrollBar/Tab
 *    so that WM_CTLCOLOR* actually paints them (otherwise UxTheme wins).
 *  - BS_OWNERDRAW conversion for push buttons so the parent dialog can
 *    paint them via WM_DRAWITEM (push buttons ignore WM_CTLCOLORBTN).
 *  - NM_CUSTOMDRAW for SysTabControl32 so tab items get dark colors.
 *  - Static controls no longer use SetBkMode(TRANSPARENT); instead
 *    SetBkColor() + matching brush is used so text doesn't merge.
 *  - WM_THEME_CHANGED (WM_USER + 0x200) is broadcast on toggle and
 *    handled by every dialog proc via Theme_HandleThemeChanged().
 */

#include "n02_theme.h"
#include "common/nSettings.h"
#include "common/kaillera_lang.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <dwmapi.h>      /* DwmSetWindowAttribute */
#include <uxtheme.h>     /* SetWindowTheme */

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
        const COLORREF DarkBtnBgHot    = RGB(60, 60, 60);       /* #3C3C3C - hover */
        const COLORREF DarkBtnBgPushed = RGB(28, 28, 28);       /* #1C1C1C - pushed */
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
 * DWM Dark Title Bar
 * ============================================================================ */

/* DWMWA_USE_IMMERSIVE_DARK_MODE = 20 on Windows 10 20H1+ (build 19041+).
 * Older Windows versions return an error code from DwmSetWindowAttribute
 * for this id, which is harmless — the title bar simply stays light. */
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static void ApplyDarkTitleBar(HWND hwnd, bool dark) {
        if (hwnd == NULL)
                return;
        BOOL value = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                              &value, sizeof(value));
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

/* Returns true if the button is a "push-like" button (BS_PUSHBUTTON or
 * BS_DEFPUSHBUTTON) that should be converted to owner-draw in dark mode.
 * Checkboxes, radio buttons, group boxes and BS_PUSHLIKE-checkboxes are
 * left alone — they paint correctly through WM_CTLCOLORBTN. */
static bool IsPushButtonStyle(HWND hwnd) {
        if (!IsButtonControl(hwnd))
                return false;
        LONG style = GetWindowLong(hwnd, GWL_STYLE) & 0xF;
        /* BS_PUSHBUTTON=0, BS_DEFPUSHBUTTON=1 — both are "push" buttons.
         * BS_GROUPBOX=3, BS_AUTOCHECKBOX=4, BS_CHECKBOX=2,
         * BS_AUTORADIOBUTTON=9, BS_RADIOBUTTON=4 — skip these.
         * BS_OWNERDRAW=11 — already converted. */
        return (style == BS_PUSHBUTTON || style == BS_DEFPUSHBUTTON);
}

/* Returns true if the button is currently in BS_OWNERDRAW mode (i.e. we
 * converted it earlier when entering dark mode). Used to revert back to
 * BS_PUSHBUTTON when returning to light mode. */
static bool IsOwnerDrawButton(HWND hwnd) {
        if (!IsButtonControl(hwnd))
                return false;
        LONG style = GetWindowLong(hwnd, GWL_STYLE) & 0xF;
        return (style == BS_OWNERDRAW);
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
        /* Send WM_THEME_CHANGED to all top-level windows on the current thread.
         * Each dialog proc handles it via Theme_HandleThemeChanged() which
         * re-applies the dark title bar + control styles + repaint. */
        EnumThreadWindows(GetCurrentThreadId(), [](HWND hwnd, LPARAM lParam) -> BOOL {
                SendMessageTimeout(hwnd, WM_THEME_CHANGED, 0, 0,
                                   SMTO_ABORTIFHUNG, 100, NULL);
                /* Re-paint children too so any owner-draw buttons refresh. */
                EnumChildWindows(hwnd, [](HWND child, LPARAM lParam2) -> BOOL {
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

        /* Header control dark theme — handled by SetWindowTheme below
         * (called from Theme_ApplyToDialogChildren for SysHeader32). */
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
        if (hDlg == NULL)
                return;

        /* Always ensure brushes exist when in dark mode. */
        if (g_dark_mode) {
                EnsureBrushes();
        }

        EnumChildWindows(hDlg, [](HWND child, LPARAM lParam) -> BOOL {
                if (g_dark_mode) {
                        /* --- Dark mode: apply dark styling --- */

                        if (IsListViewControl(child)) {
                                Theme_ApplyToListView(child);
                        } else if (IsRichEditControl(child)) {
                                Theme_ApplyToRichEdit(child);
                        }

                        /* Disable visual styles for these controls so that
                         * WM_CTLCOLOR* actually paints them. Without this,
                         * UxTheme draws the chrome (light borders, light
                         * dropdown arrows) and ignores our colors. */
                        if (IsEditControl(child) ||
                            IsListBoxControl(child) ||
                            IsComboBoxControl(child) ||
                            IsScrollBarControl(child) ||
                            IsTabControl(child)) {
                                SetWindowTheme(child, L"", L"");
                        }

                        /* Convert push buttons to owner-draw so we can
                         * paint them dark via WM_DRAWITEM. Push buttons
                         * ignore the brush returned from WM_CTLCOLORBTN. */
                        if (IsPushButtonStyle(child)) {
                                LONG curStyle = GetWindowLong(child, GWL_STYLE);
                                SetWindowLong(child, GWL_STYLE,
                                        (curStyle & ~0xF) | BS_OWNERDRAW);
                                InvalidateRect(child, NULL, TRUE);
                        }
                } else {
                        /* --- Light mode: revert any dark-mode tweaks --- */

                        /* Restore the default visual style. Per MSDN, calling
                         * SetWindowTheme with both NULL pointers reverts to
                         * the class default theme. */
                        if (IsEditControl(child) ||
                            IsListBoxControl(child) ||
                            IsComboBoxControl(child) ||
                            IsScrollBarControl(child) ||
                            IsTabControl(child)) {
                                SetWindowTheme(child, NULL, NULL);
                        }

                        /* Convert BS_OWNERDRAW buttons back to BS_PUSHBUTTON
                         * so they paint themselves in the system style. */
                        if (IsOwnerDrawButton(child)) {
                                LONG curStyle = GetWindowLong(child, GWL_STYLE);
                                SetWindowLong(child, GWL_STYLE,
                                        (curStyle & ~0xF) | BS_PUSHBUTTON);
                                InvalidateRect(child, NULL, TRUE);
                        }
                }
                return TRUE;
        }, 0);
}

void Theme_OnInitDialog(HWND hDlg) {
        if (hDlg == NULL)
                return;

        /* The DWM dark title bar attribute is harmless in light mode
         * (it just leaves the title bar light), so we always call it.
         * This also makes Theme_OnInitDialog safe to call unconditionally. */
        ApplyDarkTitleBar(hDlg, g_dark_mode);

        if (!g_dark_mode)
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

        /* Button controls (push buttons, checkboxes, radio buttons, group boxes)
         * NOTE: push buttons were converted to BS_OWNERDRAW in
         * Theme_ApplyToDialogChildren, so they no longer send WM_CTLCOLORBTN —
         * they send WM_DRAWITEM instead and are painted by Theme_DrawButton.
         * The remaining buttons reaching here are checkboxes, radios, and
         * group boxes, which DO honor WM_CTLCOLORBTN. */
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
                }
                /* BS_OWNERDRAW and other styles: fall through to default below */
        }

        /* Tab controls — note that NM_CUSTOMDRAW (in Theme_HandleTabNotify)
         * does the heavy lifting for the tab items themselves; this only
         * colors the tab control's background. */
        if (IsTabControl(childHwnd)) {
                SetTextColor(hdc, ThemeColors::DarkTabText);
                SetBkColor(hdc, ThemeColors::DarkTabBg);
                return s_brushDarkTabBg;
        }

        /* Static text controls.
         *
         * Per DARK_THEME_GUIDE.md problem 6: do NOT call SetBkMode(hdc, TRANSPARENT)
         * here. Returning a brush while asking for TRANSPARENT background is
         * contradictory — Windows uses the brush on some render paths and
         * the text ends up drawn on top of the previous frame (or merged
         * with whatever was behind it). Setting both SetBkColor() and
         * returning a matching solid brush gives correct, opaque repaints. */
        if (IsStaticControl(childHwnd)) {
                SetTextColor(hdc, ThemeColors::DarkStaticText);
                SetBkColor(hdc, ThemeColors::DarkBg);
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

/* ============================================================================
 * WM_THEME_CHANGED Handler
 * ============================================================================ */

LRESULT Theme_HandleThemeChanged(HWND hDlg) {
        if (hDlg == NULL)
                return 0;

        /* Re-apply the title bar attribute for the new mode. */
        ApplyDarkTitleBar(hDlg, g_dark_mode);

        /* Re-run child styling: in dark mode this converts push buttons to
         * BS_OWNERDRAW and disables visual styles on edit/combo/etc.
         * In light mode it reverts those changes. */
        Theme_ApplyToDialogChildren(hDlg);

        /* Force a full repaint of the dialog and all its children. */
        InvalidateRect(hDlg, NULL, TRUE);
        EnumChildWindows(hDlg, [](HWND child, LPARAM lParam) -> BOOL {
                InvalidateRect(child, NULL, TRUE);
                return TRUE;
        }, 0);

        return 0;
}

/* ============================================================================
 * Owner-Draw Push Button Renderer (WM_DRAWITEM)
 * ============================================================================ */

bool Theme_DrawButton(LPDRAWITEMSTRUCT dis) {
        if (dis == NULL)
                return false;

        /* Only handle dark-mode push buttons. */
        if (!g_dark_mode)
                return false;
        if (dis->CtlType != ODT_BUTTON)
                return false;

        EnsureBrushes();

        HWND hwnd = dis->hwndItem;
        HDC  hdc  = dis->hDC;
        RECT rc   = dis->rcItem;

        /* Decide background color based on button state. */
        COLORREF bgCol   = ThemeColors::DarkBtnBg;
        COLORREF textCol = ThemeColors::DarkBtnText;

        if (dis->itemState & ODS_DISABLED) {
                bgCol   = ThemeColors::DarkBtnBg;
                textCol = ThemeColors::DarkTextDim;
        } else if (dis->itemState & ODS_SELECTED) {
                bgCol   = ThemeColors::DarkBtnBgPushed;
        } else if (dis->itemState & ODS_FOCUS) {
                /* Subtly highlighted when focused. */
                bgCol   = ThemeColors::DarkBtnBgHot;
        }

        /* Fill background. */
        HBRUSH bgBrush = CreateSolidBrush(bgCol);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        /* Draw 1px border. */
        HPEN hPen   = CreatePen(PS_SOLID, 1, ThemeColors::DarkBorder);
        HPEN hOld   = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);

        /* Read button text. */
        wchar_t wtext[256] = {0};
        GetWindowTextW(hwnd, wtext, (int)(sizeof(wtext) / sizeof(wtext[0]) - 1));

        /* Use the control's current font (matches BS_MULTILINE etc.). */
        HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
        HFONT hOldFont = NULL;
        if (hFont != NULL) {
                hOldFont = (HFONT)SelectObject(hdc, hFont);
        }

        SetTextColor(hdc, textCol);
        SetBkMode(hdc, TRANSPARENT);

        /* Detect BS_MULTILINE so multi-line button labels (e.g. BTN_SSRVWG
         * "&waiting games") wrap correctly. */
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        UINT dtFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        if (style & BS_MULTILINE) {
                dtFlags = DT_CENTER | DT_WORDBREAK;
                /* For multi-line we need a top-aligned rect with vertical
                 * centering done manually via system metrics. Simpler: use
                 * DT_CENTER only and let DrawText place the text block. */
                RECT textRc = rc;
                /* Estimate text height and center vertically. */
                int height = DrawTextW(hdc, wtext, -1, &textRc,
                                       DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
                int yOffset = ((rc.bottom - rc.top) - height) / 2;
                if (yOffset < 0) yOffset = 0;
                textRc.left   = rc.left   + 2;
                textRc.right  = rc.right  - 2;
                textRc.top    = rc.top    + yOffset;
                textRc.bottom = rc.bottom;
                DrawTextW(hdc, wtext, -1, &textRc, DT_CENTER | DT_WORDBREAK);
        } else {
                /* Inset a little so text doesn't touch the border. */
                RECT textRc = rc;
                InflateRect(&textRc, -2, -2);
                DrawTextW(hdc, wtext, -1, &textRc, dtFlags);
        }

        if (hOldFont != NULL) {
                SelectObject(hdc, hOldFont);
        }

        /* Focus rectangle (drawn inside the border). */
        if (dis->itemState & ODS_FOCUS) {
                RECT focusRc = rc;
                InflateRect(&focusRc, -3, -3);
                DrawFocusRect(hdc, &focusRc);
        }

        return true;
}

/* ============================================================================
 * Tab Control NM_CUSTOMDRAW Handler
 * ============================================================================ */

LRESULT Theme_HandleTabNotify(LPARAM lParam) {
        if (!g_dark_mode)
                return -1;

        LPNMHDR hdr = (LPNMHDR)lParam;
        if (hdr == NULL)
                return -1;

        /* Only handle SysTabControl32 notifications. */
        char cls[32] = {0};
        GetClassNameA(hdr->hwndFrom, cls, sizeof(cls));
        if (strcmp(cls, "SysTabControl32") != 0)
                return -1;

        if (hdr->code != NM_CUSTOMDRAW)
                return -1;

        LPNMCUSTOMDRAW cd = (LPNMCUSTOMDRAW)lParam;

        switch (cd->dwDrawStage) {
        case CDDS_PREPAINT:
                /* Ask for per-item notifications. */
                return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
                /* Color the tab item text and background. The system still
                 * draws the tab shape itself; we just override text colors. */
                SetTextColor(cd->hdc, ThemeColors::DarkTabText);
                SetBkColor(cd->hdc, ThemeColors::DarkTabBg);
                return CDRF_NEWFONT;

        default:
                return -1;
        }
}
