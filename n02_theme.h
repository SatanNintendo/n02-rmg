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
 *
 * Thread safety:
 *   - g_dark_mode is a simple bool read/written from the GUI thread only.
 *   - Brushes are created lazily and never deleted until Theme_Shutdown().
 */

#include <windows.h>
#include <commctrl.h>

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
 * Must be called AFTER all controls have been created. */
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

/* Apply dark theme to all child controls of a dialog recursively. */
void Theme_ApplyToDialogChildren(HWND hDlg);

/* Notify all top-level windows that theme has changed.
 * They should re-apply colors via Theme_OnInitDialog or similar. */
void Theme_NotifyChanged();
