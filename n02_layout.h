#pragma once
/*
 * n02_layout.h - Optional persistence of user-adjusted window layout
 *
 * One global switch ("Save custom window settings", check box IDC_SAVE_LAYOUT
 * in the options bar of the main windows) controls whether the layout the user
 * set up (sizes/positions of resizable windows, ListView column widths, ...)
 * is remembered between runs.
 *
 *   - switch OFF (default): nothing is saved, nothing is restored; every window
 *     opens with its built-in defaults. Turning the switch off also wipes what
 *     was stored before, so the next start is guaranteed to use the defaults.
 *   - switch ON: windows call the Layout_Save*() functions when they close and
 *     the Layout_Restore*() functions when they open.
 *
 * Storage: n02.ini, key SaveWindowLayout in section [okai] (the flag) and
 * section [layout] (all the values). The functions use explicit sections, so
 * they never change the "current section" selected by nSettings::Initialize().
 *
 * Every Save/Restore function is a no-op while the switch is off, so callers
 * do not need to check Layout_IsEnabled() themselves (they may, e.g. to decide
 * between "restored" and "built-in default" code paths).
 */

#include <windows.h>

/* Is persistence of the window layout enabled? (cached after the first call) */
bool Layout_IsEnabled();

/* Turn persistence on/off and write the flag to n02.ini.
 * Turning it OFF also deletes every stored layout value. */
void Layout_SetEnabled(bool enabled);

/* Raw access to the stored layout values (section [layout]).
 * Layout_GetInt returns 'def' while persistence is off. Layout_SetInt does
 * nothing while persistence is off. */
int  Layout_GetInt(const char* key, int def);
void Layout_SetInt(const char* key, int value);

/* Remember / restore the position, size and maximized state of a top-level
 * window under the keys <prefix>X, <prefix>Y, <prefix>W, <prefix>H, <prefix>Max.
 * Layout_RestoreWindow() returns true if a stored layout was applied. A stored
 * position that is no longer on any monitor is replaced by a centered one. If
 * the window was maximized when it was saved, it is maximized again (after the
 * dialog has finished initializing). Call it at the END of WM_INITDIALOG. */
void Layout_SaveWindow(HWND hWnd, const char* prefix);
bool Layout_RestoreWindow(HWND hWnd, const char* prefix);

/* Remember / restore the widths of the first 'count' columns of a ListView
 * under the keys <prefix>0, <prefix>1, ...
 * Layout_RestoreColumns() leaves a column unchanged when nothing valid is
 * stored for it. Stored widths below 'minWidth' (other than 0, which is only
 * restored when 'allowZero' is true, for columns the user may hide) are
 * ignored, so a column can never get "lost" at a few pixels. */
void Layout_SaveColumns(HWND hList, const char* prefix, int count);
void Layout_RestoreColumns(HWND hList, const char* prefix, int count,
                           int minWidth = 10, bool allowZero = false);
