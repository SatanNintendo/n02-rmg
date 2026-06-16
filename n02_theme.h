#pragma once
/*
 * n02_theme.h - Dark/Light theme support for N02 Kaillera Client
 *
 * Provides a centralized dark theme system that can be toggled at runtime.
 * Light theme is the default and uses system colors (no visual change).
 * Dark theme overrides dialog backgrounds, text colors, and control styles.
 *
 * Usage in dialog procedures:
 *   1. Call Theme_OnInitDialog(hDlg) in WM_INITDIALOG
 *   2. Call Theme_OnDestroy(hDlg) in WM_DESTROY / before EndDialog
 *   3. Handle WM_CTLCOLOR* via Theme_HandleCtlColor(hDlg, hdc, childHwnd)
 *   4. Handle WM_ERASEBKGND via Theme_HandleEraseBkgnd(hDlg, hdc)
 *   5. Handle WM_THEME_CHANGED (= WM_USER + 0x200) via Theme_HandleThemeChanged(hDlg)
 *   6. Handle WM_DRAWITEM via Theme_DrawButton((LPDRAWITEMSTRUCT)lParam)
 *      (only needed for dialogs containing push buttons)
 *   7. For tab controls: in WM_NOTIFY call Theme_HandleTabNotify(lParam);
 *      if it returns a non-negative value, return that value.
 *
 * Thread safety:
 *   - g_dark_mode is a simple bool read/written from the GUI thread only.
 *   - Brushes are created lazily and never deleted until Theme_Shutdown().
 */

#include <windows.h>
#include <commctrl.h>

/* WM_CTLCOLORCOMBOBOX (0x0133) is the same value as WM_CTLCOLOREDIT.
 * Do NOT use it in switch/case statements — combo boxes are handled
 * by WM_CTLCOLOREDIT (edit portion) and WM_CTLCOLORLISTBOX (drop-down). */

/* Custom WM_USER message broadcast by Theme_NotifyChanged() so that every
 * open dialog re-applies the current theme (dark title bar, control styles,
 * owner-draw conversion, etc.). Dialog procs should handle it by calling
 * Theme_HandleThemeChanged(hDlg). */
#define WM_THEME_CHANGED (WM_USER + 0x200)

/* Global dark mode flag. Default: false (light theme) */
extern bool g_dark_mode;

/* Initialize the theme system. Call once at startup (after nSettings::Initialize). */
void Theme_Init();

/* Save the current theme preference to n02.ini. Call during shutdown. */
void Theme_Save();

/* Clean up theme resources (brushes). Call once at shutdown. */
void Theme_Shutdown();

/* Apply theme styling to a dialog on WM_INITDIALOG.
 * Sets up dark mode colors for RichEdit, ListView, and other controls.
 * Must be called AFTER all controls have been created.
 * Safe to call in light mode (no-op when g_dark_mode is false). */
void Theme_OnInitDialog(HWND hDlg);

/* Clean up per-dialog theme data. Call in WM_DESTROY or before EndDialog. */
void Theme_OnDestroy(HWND hDlg);

/* Handle WM_CTLCOLOR* messages for dark theme.
 * Returns the brush to use, or NULL if light theme (default handling). */
HBRUSH Theme_HandleCtlColor(HWND hDlg, HDC hdc, HWND childHwnd);

/* Handle WM_ERASEBKGND for dark theme.
 * Returns true if the background was painted (dark mode), false otherwise. */
bool Theme_HandleEraseBkgnd(HWND hDlg, HDC hdc);

/* Toggle dark mode on/off. Saves preference and notifies all open dialogs. */
void Theme_Toggle();

/* Check if dark mode is active. */
bool Theme_IsDark();

/* Apply dark theme to a ListView control (background, text, header). */
void Theme_ApplyToListView(HWND hListView);

/* Apply dark theme to a RichEdit control. */
void Theme_ApplyToRichEdit(HWND hRichEdit);

/* Apply theme styling (owner-draw conversion + SetWindowTheme) to all child
 * controls of a dialog recursively. In dark mode push buttons are switched
 * to BS_OWNERDRAW so the parent dialog can paint them via WM_DRAWITEM.
 * In light mode previously-modified controls are reverted to defaults.
 * Safe to call in either mode. */
void Theme_ApplyToDialogChildren(HWND hDlg);

/* Notify all top-level windows that theme has changed by broadcasting
 * WM_THEME_CHANGED. Each dialog should re-apply colors via
 * Theme_HandleThemeChanged(). */
void Theme_NotifyChanged();

/* Convenience handler for the WM_THEME_CHANGED (WM_USER + 0x200) message.
 * Re-applies the dark title bar, re-runs Theme_ApplyToDialogChildren so
 * control styles (owner-draw / SetWindowTheme) match the new mode, and
 * forces a full repaint of the dialog. Returns 0 (the value the dialog
 * proc should return). */
LRESULT Theme_HandleThemeChanged(HWND hDlg);

/* Owner-draw push button renderer for dark mode.
 * Call from each dialog proc's WM_DRAWITEM handler:
 *     case WM_DRAWITEM:
 *         if (Theme_DrawButton((LPDRAWITEMSTRUCT)lParam))
 *             return TRUE;
 *         break;
 * Returns true if the item was a dark-mode push button and was drawn.
 * Returns false otherwise (light mode, not a button, or wrong CtlType)
 * so the dialog proc can fall through to default handling. */
bool Theme_DrawButton(LPDRAWITEMSTRUCT dis);

/* Tab-control NM_CUSTOMDRAW handler for dark mode.
 * Call from each dialog proc's WM_NOTIFY handler that contains a
 * SysTabControl32:
 *     case WM_NOTIFY: {
 *         LRESULT tabResult = Theme_HandleTabNotify(lParam);
 *         if (tabResult >= 0) return tabResult;
 *         ... existing WM_NOTIFY logic ...
 *     }
 * Returns:
 *   - CDRF_NOTIFYITEMDRAW / CDRF_NEWFONT (>= 0) when it handled the
 *     notification for a SysTabControl32 in dark mode — the dialog proc
 *     should return this value.
 *   - -1 when it did not handle the notification (light mode, not a tab
 *     control, or uninteresting draw stage) — the dialog proc should
 *     continue its own WM_NOTIFY processing. */
LRESULT Theme_HandleTabNotify(LPARAM lParam);
