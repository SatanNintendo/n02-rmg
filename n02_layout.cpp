/*
 * n02_layout.cpp - Optional persistence of user-adjusted window layout
 * See n02_layout.h for the overview.
 */
#include "n02_layout.h"

#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#include "common/nSettings.h"

#define LAYOUT_FLAG_SECTION  "okai"
#define LAYOUT_FLAG_KEY      "SaveWindowLayout"
#define LAYOUT_DATA_SECTION  "layout"

static int s_enabled = -1;      /* -1 = not loaded yet, otherwise 0 / 1 */

bool Layout_IsEnabled() {
        if (s_enabled < 0)
                s_enabled = (nSettings::get_int_in(LAYOUT_FLAG_SECTION, LAYOUT_FLAG_KEY, 0) != 0) ? 1 : 0;
        return s_enabled != 0;
}

void Layout_SetEnabled(bool enabled) {
        s_enabled = enabled ? 1 : 0;
        nSettings::set_int_in(LAYOUT_FLAG_SECTION, LAYOUT_FLAG_KEY, enabled ? 1 : 0);
        if (!enabled) {
                /* "Off" means: the next start uses the built-in defaults. */
                nSettings::clear_section(LAYOUT_DATA_SECTION);
        }
}

int Layout_GetInt(const char* key, int def) {
        if (!Layout_IsEnabled() || key == NULL)
                return def;
        return nSettings::get_int_in(LAYOUT_DATA_SECTION, key, def);
}

void Layout_SetInt(const char* key, int value) {
        if (!Layout_IsEnabled() || key == NULL)
                return;
        nSettings::set_int_in(LAYOUT_DATA_SECTION, key, value);
}

/* ---- window geometry ------------------------------------------------------ */

static void MakeKey(char* out, size_t outSize, const char* prefix, const char* suffix) {
        _snprintf_s(out, outSize, _TRUNCATE, "%s%s", prefix ? prefix : "", suffix);
}

void Layout_SaveWindow(HWND hWnd, const char* prefix) {
        if (hWnd == NULL || prefix == NULL || !Layout_IsEnabled())
                return;

        WINDOWPLACEMENT wp;
        memset(&wp, 0, sizeof(wp));
        wp.length = sizeof(wp);
        if (!GetWindowPlacement(hWnd, &wp))
                return;

        const bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED) ||
                (wp.showCmd == SW_SHOWMINIMIZED && (wp.flags & WPF_RESTORETOMAXIMIZED) != 0);

        int x, y, w, h;
        if (!maximized && wp.showCmd != SW_SHOWMINIMIZED) {
                /* Normal state: the real window rectangle in screen coordinates. */
                RECT rc;
                if (!GetWindowRect(hWnd, &rc))
                        return;
                x = rc.left;
                y = rc.top;
                w = rc.right - rc.left;
                h = rc.bottom - rc.top;
        } else {
                /* Maximized/minimized: the "restored" rectangle is only available
                 * from the window placement, which uses workspace coordinates
                 * (relative to the work area, i.e. shifted by a top/left taskbar). */
                RECT work = { 0, 0, 0, 0 };
                SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
                x = wp.rcNormalPosition.left + work.left;
                y = wp.rcNormalPosition.top + work.top;
                w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        }

        if (w <= 100 || h <= 100)
                return;                 /* nonsense, keep what was stored before */

        char key[96];
        MakeKey(key, sizeof(key), prefix, "X");   Layout_SetInt(key, x);
        MakeKey(key, sizeof(key), prefix, "Y");   Layout_SetInt(key, y);
        MakeKey(key, sizeof(key), prefix, "W");   Layout_SetInt(key, w);
        MakeKey(key, sizeof(key), prefix, "H");   Layout_SetInt(key, h);
        MakeKey(key, sizeof(key), prefix, "Max"); Layout_SetInt(key, maximized ? 1 : 0);
}

bool Layout_RestoreWindow(HWND hWnd, const char* prefix) {
        if (hWnd == NULL || prefix == NULL || !Layout_IsEnabled())
                return false;

        char key[96];
        MakeKey(key, sizeof(key), prefix, "W");   const int w = Layout_GetInt(key, 0);
        MakeKey(key, sizeof(key), prefix, "H");   const int h = Layout_GetInt(key, 0);
        if (w <= 100 || h <= 100)
                return false;           /* nothing (valid) stored yet */

        RECT cur = { 0, 0, 0, 0 };
        GetWindowRect(hWnd, &cur);
        MakeKey(key, sizeof(key), prefix, "X");   int x = Layout_GetInt(key, cur.left);
        MakeKey(key, sizeof(key), prefix, "Y");   int y = Layout_GetInt(key, cur.top);
        MakeKey(key, sizeof(key), prefix, "Max"); const bool maximized = Layout_GetInt(key, 0) != 0;

        /* The title bar must still be reachable on some monitor (the monitor
         * setup may have changed since the layout was saved). */
        POINT probe = { x + w / 2, y + 8 };
        if (MonitorFromPoint(probe, MONITOR_DEFAULTTONULL) == NULL) {
                MONITORINFO mi;
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
                if (GetMonitorInfo(mon, &mi)) {
                        x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
                        y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
                        if (x < mi.rcWork.left) x = mi.rcWork.left;
                        if (y < mi.rcWork.top) y = mi.rcWork.top;
                }
        }

        SetWindowPos(hWnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

        if (maximized) {
                /* Posted, not sent: the dialog is still being initialized, and this
                 * way it is maximized as soon as it is up (WM_SIZE then lays out
                 * the controls exactly as for a manual maximize). */
                PostMessage(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }
        return true;
}

/* ---- ListView columns ----------------------------------------------------- */

void Layout_SaveColumns(HWND hList, const char* prefix, int count) {
        if (hList == NULL || prefix == NULL || count <= 0 || !Layout_IsEnabled())
                return;

        char key[96];
        for (int i = 0; i < count; ++i) {
                _snprintf_s(key, sizeof(key), _TRUNCATE, "%s%i", prefix, i);
                Layout_SetInt(key, ListView_GetColumnWidth(hList, i));
        }
}

void Layout_RestoreColumns(HWND hList, const char* prefix, int count, int minWidth, bool allowZero) {
        if (hList == NULL || prefix == NULL || count <= 0 || !Layout_IsEnabled())
                return;

        char key[96];
        for (int i = 0; i < count; ++i) {
                _snprintf_s(key, sizeof(key), _TRUNCATE, "%s%i", prefix, i);
                const int saved = Layout_GetInt(key, -1);
                if (saved < 0)
                        continue;
                if (saved == 0 && !allowZero)
                        continue;
                if (saved > 0 && saved < minWidth)
                        continue;
                ListView_SetColumnWidth(hList, i, saved);
        }
}
