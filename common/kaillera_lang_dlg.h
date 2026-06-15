#pragma once
/*
 * kaillera_lang_dlg.h - Dialog localization helper for N02 Kaillera Client
 *
 * Win32 dialog resources (.rc) contain hardcoded strings that can't use
 * the LNG() macro at compile time. This module provides a helper function
 * that applies localized strings to dialog controls at runtime, typically
 * in WM_INITDIALOG handlers.
 *
 * Usage: Call ApplyDialogLanguage(hDlg, dialogId) at the start of each
 *        dialog's WM_INITDIALOG handler.
 */

#include <windows.h>
#include "kaillera_lang.h"
#include "../resource.h"

/* Apply localized strings to a dialog based on its resource ID.
   This sets window text, button labels, static text, group boxes, etc.
   using the loaded language strings. */
void ApplyDialogLanguage(HWND hDlg, int dialogId);
