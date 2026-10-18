/*
 * kaillera_lang_dlg.cpp - Dialog localization helper for N02 Kaillera Client
 *
 * Applies localized strings from the .lng file to Win32 dialog controls
 * that were defined in the .rc resource file with hardcoded English text.
 */

#include "kaillera_lang_dlg.h"
#include "../resource.h"
#include "../n02_theme.h"
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

/* Helper: set dialog item text if the control exists. Returns true if found. */
static bool SetDlgText(HWND hDlg, int controlId, const char* text) {
    HWND ctl = GetDlgItem(hDlg, controlId);
    if (ctl != NULL) {
        SetWindowTextA(ctl, text);
        return true;
    }
    return false;
}

/* Helper: set dialog caption */
static void SetDlgCaption(HWND hDlg, const char* text) {
    SetWindowTextA(hDlg, text);
}

/* Helper: find a static text control by its current text content
   and change it to a new text. Used for IDC_STATIC labels which
   all share the same control ID (-1). */
struct FindStaticCtx {
    const char* oldText;
    const char* newText;
    bool found;
    int tagId;   /* if != 0: synthetic control ID given to the matched label */
};

/* ---------------------------------------------------------------------------
 * Stable identification of IDC_STATIC labels
 *
 * All IDC_STATIC labels share the control ID -1, so they are located by their
 * *current* text, i.e. the English text from the .rc file. That works only
 * once: after the first translation the control no longer contains the English
 * text, so a later live language switch (e.g. Russian -> English) could not
 * find the label any more and left it in the previous language.
 *
 * To fix this, the first time a label is found by its English text it gets a
 * synthetic control ID (STATIC_LABEL_ID_BASE + index in s_staticLabelKeys).
 * Later calls find it directly with GetDlgItem(). The IDs are far above every
 * ID in resource.h, and nothing refers to the IDC_STATIC (-1) of these labels.
 * --------------------------------------------------------------------------- */
#define STATIC_LABEL_ID_BASE 0x7E00

static const char* const s_staticLabelKeys[] = {
    "Nick:",
    "Mode",
    "Local List",
    "Frame Delay:",
    "Name:",
    "IP:",
    "Join msg:",
    "Max players:",
    "Max ping:",
    "On user join:",
    "Window flash:",
    "Beep:",
    "Kaillera protocol and API:\n(c) 2001-2002 Christophe Thibault\n------------------------------------------------------",
    "n02 (c) Open Kaillera\nn02.p2p (c) Open Kaillera"
};
static const int s_staticLabelKeyCount = (int)(sizeof(s_staticLabelKeys) / sizeof(s_staticLabelKeys[0]));

static int FindStaticLabelIndex(const char* oldText) {
    for (int i = 0; i < s_staticLabelKeyCount; i++) {
        if (strcmp(s_staticLabelKeys[i], oldText) == 0)
            return i;
    }
    return -1;
}

static BOOL CALLBACK FindStaticCallback(HWND hwnd, LPARAM lParam) {
    FindStaticCtx* ctx = (FindStaticCtx*)lParam;
    char buf[128];
    buf[0] = 0;
    GetWindowTextA(hwnd, buf, sizeof(buf));
    if (strcmp(buf, ctx->oldText) == 0) {
        SetWindowTextA(hwnd, ctx->newText);
        if (ctx->tagId != 0)
            SetWindowLongPtr(hwnd, GWLP_ID, (LONG_PTR)ctx->tagId);
        ctx->found = true;
        return FALSE; /* stop enumerating */
    }
    return TRUE; /* continue */
}

static void ReplaceStaticText(HWND hDlg, const char* oldText, const char* newText) {
    int tagId = 0;
    int idx = FindStaticLabelIndex(oldText);
    if (idx >= 0) {
        tagId = STATIC_LABEL_ID_BASE + idx;
        /* Already tagged by an earlier call (language was applied before):
           find it by its ID, its text is no longer the English one. */
        HWND tagged = GetDlgItem(hDlg, tagId);
        if (tagged != NULL) {
            SetWindowTextA(tagged, newText);
            return;
        }
    }
    FindStaticCtx ctx = { oldText, newText, false, tagId };
    EnumChildWindows(hDlg, FindStaticCallback, (LPARAM)&ctx);
}

/* Helper: find and replace all matching static text controls
   (some dialogs have duplicate labels like "IP:" in different places) */
static BOOL CALLBACK ReplaceAllStaticCallback(HWND hwnd, LPARAM lParam) {
    FindStaticCtx* ctx = (FindStaticCtx*)lParam;
    char buf[128];
    buf[0] = 0;
    GetWindowTextA(hwnd, buf, sizeof(buf));
    if (strcmp(buf, ctx->oldText) == 0) {
        SetWindowTextA(hwnd, ctx->newText);
    }
    return TRUE; /* continue to find all matches */
}

static void ReplaceAllStaticText(HWND hDlg, const char* oldText, const char* newText) {
    FindStaticCtx ctx = { oldText, newText, false };
    EnumChildWindows(hDlg, ReplaceAllStaticCallback, (LPARAM)&ctx);
}

/* ---------------------------------------------------------------------------
 * Options bar (language + dark theme) of the main windows
 * --------------------------------------------------------------------------- */
#define OPTBAR_MAX_LANGS 32

/* File names of the languages in the combo. The item data of a combo item is
   the index into this table (no heap memory, nothing to free). */
static char s_optLangFiles[OPTBAR_MAX_LANGS][64];

/* Build the combo text "<native name> / <english name>", e.g. "Русский / Russian".
   LANG_NAME in the .lng file already has exactly this form, so it is used as is
   (the old code appended the file name once more: "Русский / Russian / Russian").
   If a file has only a single name, the file name is appended; "X / X" is
   collapsed to "X". */
static void OptBarFormatLangText(const char* fileName, char* out, size_t outSize) {
    const char* disp = LangGetDisplayName(fileName);
    if (disp == NULL || disp[0] == 0)
        disp = fileName;

    char text[128];
    strncpy_s(text, sizeof(text), disp, _TRUNCATE);

    char* sep = strstr(text, " / ");
    if (sep != NULL) {
        /* already "native / english": collapse identical halves */
        *sep = 0;
        const char* second = sep + 3;
        if (strcmp(text, second) == 0)
            strncpy_s(out, outSize, text, _TRUNCATE);
        else
            _snprintf_s(out, outSize, _TRUNCATE, "%s / %s", text, second);
    } else if (_stricmp(text, fileName) != 0) {
        _snprintf_s(out, outSize, _TRUNCATE, "%s / %s", text, fileName);
    } else {
        strncpy_s(out, outSize, text, _TRUNCATE);
    }
}

struct OptBarEnumCtx {
    HWND combo;
    const char* current;
    int selIdx;
    int count;
};

static void OptBarEnumCallback(const char* name, void* ud) {
    OptBarEnumCtx* c = (OptBarEnumCtx*)ud;
    if (name == NULL || c->count >= OPTBAR_MAX_LANGS)
        return;
    char text[160];
    OptBarFormatLangText(name, text, sizeof(text));
    int idx = (int)SendMessageA(c->combo, CB_ADDSTRING, 0, (LPARAM)text);
    if (idx == CB_ERR)
        return;
    strncpy_s(s_optLangFiles[c->count], sizeof(s_optLangFiles[c->count]), name, _TRUNCATE);
    SendMessageA(c->combo, CB_SETITEMDATA, (WPARAM)idx, (LPARAM)c->count);
    if (_stricmp(name, c->current) == 0)
        c->selIdx = idx;
    c->count++;
}

void OptionsBar_Refresh(HWND hDlg) {
    SetDlgText(hDlg, IDC_LANG_LABEL, LNG(ABOUT_LBL_LANGUAGE));
    SetDlgText(hDlg, IDC_DARK_THEME, LNG(THEME_DARK));

    HWND chk = GetDlgItem(hDlg, IDC_DARK_THEME);
    if (chk != NULL)
        SendMessage(chk, BM_SETCHECK, g_dark_mode ? BST_CHECKED : BST_UNCHECKED, 0);

    HWND combo = GetDlgItem(hDlg, IDC_LANG_COMBO);
    if (combo == NULL)
        return;

    const char* cur = LangGetFile();
    if (cur == NULL)
        cur = "";

    if (SendMessage(combo, CB_GETCOUNT, 0, 0) == 0) {
        OptBarEnumCtx ctx = { combo, cur, 0, 0 };
        LangEnumerate(OptBarEnumCallback, &ctx);
        SendMessage(combo, CB_SETCURSEL, (WPARAM)ctx.selIdx, 0);
    } else {
        /* The list of languages does not depend on the current language, so
           only the selection has to follow (no rebuild while the combo is
           still handling its own CBN_SELCHANGE). */
        int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
        for (int i = 0; i < count; i++) {
            LRESULT data = SendMessage(combo, CB_GETITEMDATA, (WPARAM)i, 0);
            if (data != CB_ERR && data >= 0 && data < OPTBAR_MAX_LANGS &&
                _stricmp(s_optLangFiles[data], cur) == 0) {
                SendMessage(combo, CB_SETCURSEL, (WPARAM)i, 0);
                break;
            }
        }
    }
}

bool OptionsBar_OnCommand(HWND hDlg, WPARAM wParam) {
    switch (LOWORD(wParam)) {
    case IDC_LANG_COMBO:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            HWND combo = GetDlgItem(hDlg, IDC_LANG_COMBO);
            int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                LRESULT data = SendMessage(combo, CB_GETITEMDATA, (WPARAM)sel, 0);
                if (data != CB_ERR && data >= 0 && data < OPTBAR_MAX_LANGS &&
                    s_optLangFiles[data][0] != 0) {
                    char file[64];
                    strncpy_s(file, sizeof(file), s_optLangFiles[data], _TRUNCATE);
                    if (LangSetLanguage(file)) {
                        /* every window (incl. this one) gets WM_LANG_CHANGED */
                        LangNotifyChanged();
                    }
                }
            }
        }
        return true;
    case IDC_DARK_THEME:
        if (HIWORD(wParam) == BN_CLICKED) {
            /* broadcasts WM_THEME_CHANGED to the open dialogs */
            Theme_Toggle();
            HWND chk = GetDlgItem(hDlg, IDC_DARK_THEME);
            if (chk != NULL)
                SendMessage(chk, BM_SETCHECK, g_dark_mode ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        return true;
    }
    return false;
}

void ApplyDialogLanguage(HWND hDlg, int dialogId) {
    switch (dialogId) {

    case MAIN_DIALOG:
        SetDlgCaption(hDlg, LNG(CAPTION_MAIN));
        SetDlgText(hDlg, IDC_ADD, LNG(BTN_ADD));
        SetDlgText(hDlg, IDC_EDIT, LNG(BTN_EDIT));
        SetDlgText(hDlg, IDC_DELETE, LNG(BTN_DELETE));
        SetDlgText(hDlg, IDC_HOST, LNG(MAIN_BTN_HOST));
        SetDlgText(hDlg, IDC_CONNECT, LNG(MAIN_BTN_CONNECT));
        SetDlgText(hDlg, IDC_P2P_PASTE_CONNECT, LNG(MAIN_BTN_PASTE_GO));
        SetDlgText(hDlg, IDC_PIPL, LNG(MAIN_LBL_IP_CODE));
        SetDlgText(hDlg, IDC_GAMEL, LNG(MAIN_LBL_GAME));
        SetDlgText(hDlg, IDC_HOSTPORT_LBL, LNG(MAIN_LBL_HOST_PORT));
        SetDlgText(hDlg, IDC_STOREDL, LNG(MAIN_LBL_STORED));
        SetDlgText(hDlg, BTN_SSRVWG, LNG(MAIN_BTN_WAITING_GAMES));
        SetDlgText(hDlg, RB_MODE_P2P, LNG(MAIN_RB_P2P));
        SetDlgText(hDlg, RB_MODE_CLIENT, LNG(MAIN_RB_SERVER));
        SetDlgText(hDlg, RB_MODE_PLAYBACK, LNG(MAIN_RB_PLAYBACK));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Nick:", LNG(MAIN_LBL_NICK));
        ReplaceStaticText(hDlg, "Mode", LNG(MAIN_GRP_MODE));
        OptionsBar_Refresh(hDlg);
        break;

    case KAILLERA_SSDLG:
        SetDlgCaption(hDlg, LNG(CAPTION_SERVER_SELECT));
        SetDlgText(hDlg, BTN_CONNECT, LNG(BTN_CONNECT));
        SetDlgText(hDlg, BTN_CUSTOM, LNG(SSDLG_BTN_CUSTOM_IP));
        SetDlgText(hDlg, BTN_WGAMES, LNG(SSDLG_BTN_WAITING_GAMES));
        SetDlgText(hDlg, BTN_MLIST, LNG(SSDLG_BTN_LIVE_SERVER_LIST));
        SetDlgText(hDlg, BTN_ADD, LNG(BTN_ADD));
        SetDlgText(hDlg, BTN_EDIT, LNG(BTN_EDIT));
        SetDlgText(hDlg, BTN_DELETE, LNG(BTN_DELETE));
        SetDlgText(hDlg, BTN_PING, LNG(BTN_PING));
        SetDlgText(hDlg, BTN_TRACE, LNG(BTN_TRACE));
        SetDlgText(hDlg, BTN_ABOUT, LNG(BTN_ABOUT));
        SetDlgText(hDlg, IDC_BUTTON3, LNG(BTN_OPTIONS)); /* hidden + disabled in the .rc */
        SetDlgText(hDlg, RB_MODE_P2P, LNG(MAIN_RB_P2P));
        SetDlgText(hDlg, RB_MODE_CLIENT, LNG(MAIN_RB_SERVER));
        SetDlgText(hDlg, RB_MODE_PLAYBACK, LNG(MAIN_RB_PLAYBACK));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Nick:", LNG(MAIN_LBL_NICK));
        ReplaceStaticText(hDlg, "Local List", LNG(SSDLG_GRP_LOCAL_LIST));
        ReplaceStaticText(hDlg, "Frame Delay:", LNG(SSDLG_LBL_FRAME_DELAY));
        ReplaceStaticText(hDlg, "Mode", LNG(MAIN_GRP_MODE));
        /* Update ListView column headers for the local server list */
        {
            HWND hLv = GetDlgItem(hDlg, LV_ULIST);
            if (hLv != NULL) {
                HWND hHdr = ListView_GetHeader(hLv);
                int colCount = (hHdr != NULL) ? Header_GetItemCount(hHdr) : 0;
                if (colCount >= 3) {
                    /* KLSListLv: Server Name, IP, Ping (3 columns) */
                    LVCOLUMNA lvc;
                    lvc.mask = LVCF_TEXT;
                    lvc.iSubItem = 0;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_SERVER_NAME));
                    ListView_SetColumn(hLv, 0, &lvc);
                    lvc.iSubItem = 1;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_IP));
                    ListView_SetColumn(hLv, 1, &lvc);
                    lvc.iSubItem = 2;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_PING));
                    ListView_SetColumn(hLv, 2, &lvc);
                }
            }
        }
        OptionsBar_Refresh(hDlg);
        break;

    case CONNECTION_DIALOG:
        SetDlgCaption(hDlg, LNG(CAPTION_CONNECTION_WINDOW));
        SetDlgText(hDlg, IDC_CHAT, LNG(CONN_BTN_CHAT));
        SetDlgText(hDlg, IDC_READY, LNG(CONN_CHK_READY));
        SetDlgText(hDlg, CHK_REC, LNG(CONN_CHK_RECORD));
        SetDlgText(hDlg, IDC_P2P_ADVANCED, LNG(CONN_BTN_ADVANCED));
        SetDlgText(hDlg, IDC_P2P_ADV_COPYIP, LNG(CONN_BTN_COPY_IP));
        SetDlgText(hDlg, IDC_RETR, LNG(CONN_BTN_RETR));
        SetDlgText(hDlg, IDC_PING, LNG(CONN_BTN_PING));
        SetDlgText(hDlg, IDC_DROPGAME, LNG(CONN_BTN_DROP_GAME));
        SetDlgText(hDlg, CHK_ENLIST, LNG(CONN_CHK_ENLIST));
        SetDlgText(hDlg, IDC_HOSTT, LNG(CONN_GRP_HOST));
        SetDlgText(hDlg, IDC_P2P_FDLY_LBL, LNG(CONN_LBL_FRAME_DELAY));
        SetDlgText(hDlg, IDC_P2P_CODE_LBL, LNG(CONN_LBL_CONNECT_CODE));
        SetDlgText(hDlg, IDC_P2P_CODE, LNG(CONN_LBL_WAITING));
        SetDlgText(hDlg, IDC_P2P_CODE_COPY, LNG(CONN_BTN_COPY_CODE));
        SetDlgText(hDlg, IDC_EJECT, LNG(CONN_BTN_EJECT_PEER));
        SetDlgText(hDlg, IDC_DONTALLOW, LNG(CONN_BTN_DONT_ALLOW_LIST));
        SetDlgText(hDlg, IDC_STATS, LNG(CONN_BTN_STATS));
        SetDlgText(hDlg, IDC_SSERV_WHATSMYIP, LNG(CONN_BTN_COPY_IP));
        break;

    case P2P_ITEM_EDIT:
        SetDlgCaption(hDlg, LNG(CAPTION_ITEM_EDIT));
        SetDlgText(hDlg, IDOK, LNG(BTN_OK));
        SetDlgText(hDlg, IDCANCEL, LNG(BTN_CANCEL));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Name:", LNG(ITEMEDIT_LBL_NAME));
        ReplaceStaticText(hDlg, "IP:", LNG(ITEMEDIT_LBL_IP));
        break;

    case KAILLERA_ABOUT:
        SetDlgCaption(hDlg, LNG(CAPTION_ABOUT));
        SetDlgText(hDlg, BTN_CLOSE, LNG(BTN_CLOSE));
        SetDlgText(hDlg, BTN_OKAI, LNG(ABOUT_BTN_WEBSITE));
        SetDlgText(hDlg, BTN_USEAGE, LNG(ABOUT_BTN_USAGE_POLICY));
        SetDlgText(hDlg, BTN_LICENSE, LNG(ABOUT_BTN_LICENSE));
        /* (the language selection moved to the main windows) */
        /* About dialog static text labels */
        ReplaceStaticText(hDlg, "Kaillera protocol and API:\n(c) 2001-2002 Christophe Thibault\n------------------------------------------------------", LNG(ABOUT_LBL_CREDITS));
        ReplaceStaticText(hDlg, "n02 (c) Open Kaillera\nn02.p2p (c) Open Kaillera", LNG(ABOUT_LBL_N02));
        break;

    case KAILLERA_CUSTOMIP:
        SetDlgCaption(hDlg, LNG(CAPTION_CUSTOM_IP));
        SetDlgText(hDlg, BTN_CONNECT, LNG(CUSTOMIP_BTN_CONNECT));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "IP:", LNG(CUSTOMIP_LBL_IP));
        break;

    case KAILLERA_MLIST:
        SetDlgCaption(hDlg, LNG(CAPTION_MASTER_LIST));
        SetDlgText(hDlg, BTN_CONNECT, LNG(MLIST_BTN_CONNECT));
        SetDlgText(hDlg, BTN_CLOSE, LNG(MLIST_BTN_CLOSE));
        SetDlgText(hDlg, BTN_ADD, LNG(MLIST_BTN_ADD_LOCAL));
        SetDlgText(hDlg, BTN_REFRESH, LNG(MLIST_BTN_REFRESH));
        SetDlgText(hDlg, BTN_WGAMES, LNG(MLIST_BTN_WAITING_GAMES));
        /* Update ListView column headers for the master server list.
           The KAILLERA_MLIST dialog is shared by multiple dialog procs
           with different column layouts. We detect which layout is active
           by checking the column count. */
        {
            HWND hLv = GetDlgItem(hDlg, LV_SLIST);
            if (hLv != NULL) {
                HWND hHdr = ListView_GetHeader(hLv);
                int colCount = (hHdr != NULL) ? Header_GetItemCount(hHdr) : 0;
                if (colCount > 0) {
                    auto setColText = [&](int col, const char* text) {
                        LVCOLUMNA lvc;
                        lvc.mask = LVCF_TEXT;
                        lvc.iSubItem = col;
                        lvc.pszText = const_cast<LPSTR>(text);
                        ListView_SetColumn(hLv, col, &lvc);
                    };

                    if (colCount == 7) {
                        /* MasterSLDialogProc: Server Name, Location, Ping, Users, Game, Ver, IP */
                        setColText(0, LNG(COL_SERVER_NAME));
                        setColText(1, LNG(COL_LOCATION));
                        setColText(2, LNG(COL_PING));
                        setColText(3, LNG(COL_USERS));
                        setColText(4, LNG(COL_GAME));
                        setColText(5, LNG(COL_VER));
                        setColText(6, LNG(COL_IP));
                    } else if (colCount == 8) {
                        /* MasterWGLDialogProc: Server Name, Emulator, User, Ping, Waiting, Server, Location, IP */
                        setColText(0, LNG(COL_SERVER_NAME));
                        setColText(1, LNG(COL_EMULATOR));
                        setColText(2, LNG(COL_USER));
                        setColText(3, LNG(COL_PING));
                        setColText(4, LNG(COL_WAITING));
                        setColText(5, LNG(COL_SERVER));
                        setColText(6, LNG(COL_LOCATION));
                        setColText(7, LNG(COL_IP));
                    } else if (colCount == 6) {
                        /* p2p_MasterSLDialogProc: Game, Emulator, User, Ping, Host, Code */
                        setColText(0, LNG(COL_GAME));
                        setColText(1, LNG(COL_EMULATOR));
                        setColText(2, LNG(COL_USER));
                        setColText(3, LNG(COL_PING));
                        setColText(4, LNG(COL_HOST));
                        setColText(5, LNG(COL_CODE));
                    }
                }
            }
        }
        break;

    case KAILLERA_SDLG:
        SetDlgCaption(hDlg, LNG(CAPTION_SERVER_ROOM));
        SetDlgText(hDlg, IDC_CHAT, LNG(SDLG_BTN_CHAT));
        SetDlgText(hDlg, BTN_START, LNG(SDLG_BTN_START));
        SetDlgText(hDlg, BTN_DROP, LNG(SDLG_BTN_DROP));
        SetDlgText(hDlg, BTN_LEAVE, LNG(SDLG_BTN_LEAVE));
        SetDlgText(hDlg, BTN_KICK, LNG(SDLG_BTN_KICK));
        SetDlgText(hDlg, BTN_LAGSTAT, LNG(SDLG_BTN_LAGSTAT));
        SetDlgText(hDlg, CHK_REC, LNG(SDLG_CHK_RECORD));
        SetDlgText(hDlg, IDC_CREATE, LNG(SDLG_BTN_CREATE));
        SetDlgText(hDlg, BTN_GCHAT, LNG(SDLG_BTN_GAME_CHAT));
        SetDlgText(hDlg, BTN_OPTIONS, LNG(SDLG_BTN_OPTIONS));
        SetDlgText(hDlg, BTN_ADVERTISE, LNG(SDLG_BTN_ADVERTISE));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Join msg:", LNG(SDLG_LBL_JOIN_MSG));
        /* Update ListView column headers - users list (LV_ULIST) */
        {
            HWND hLv = GetDlgItem(hDlg, LV_ULIST);
            if (hLv != NULL) {
                HWND hHdr = ListView_GetHeader(hLv);
                int colCount = (hHdr != NULL) ? Header_GetItemCount(hHdr) : 0;
                if (colCount == 5) {
                    /* Users: Name, Ping, UID, Status, Connection */
                    LVCOLUMNA lvc;
                    lvc.mask = LVCF_TEXT;
                    lvc.iSubItem = 0;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_USERLIST_NICK));
                    ListView_SetColumn(hLv, 0, &lvc);
                    lvc.iSubItem = 1;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_PING));
                    ListView_SetColumn(hLv, 1, &lvc);
                    lvc.iSubItem = 2;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_UID));
                    ListView_SetColumn(hLv, 2, &lvc);
                    lvc.iSubItem = 3;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_STATUS));
                    ListView_SetColumn(hLv, 3, &lvc);
                    lvc.iSubItem = 4;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_CONNECTION));
                    ListView_SetColumn(hLv, 4, &lvc);
                }
            }
        }
        /* Update ListView column headers - games list (LV_GLIST) */
        {
            HWND hLv = GetDlgItem(hDlg, LV_GLIST);
            if (hLv != NULL) {
                HWND hHdr = ListView_GetHeader(hLv);
                int colCount = (hHdr != NULL) ? Header_GetItemCount(hHdr) : 0;
                if (colCount == 6) {
                    /* Games: Game, GameID, Emulator, User, Status, Users */
                    LVCOLUMNA lvc;
                    lvc.mask = LVCF_TEXT;
                    lvc.iSubItem = 0;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_GAME));
                    ListView_SetColumn(hLv, 0, &lvc);
                    lvc.iSubItem = 1;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_GAME_ID));
                    ListView_SetColumn(hLv, 1, &lvc);
                    lvc.iSubItem = 2;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_EMULATOR));
                    ListView_SetColumn(hLv, 2, &lvc);
                    lvc.iSubItem = 3;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_USER));
                    ListView_SetColumn(hLv, 3, &lvc);
                    lvc.iSubItem = 4;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_STATUS));
                    ListView_SetColumn(hLv, 4, &lvc);
                    lvc.iSubItem = 5;
                    lvc.pszText = const_cast<LPSTR>(LNG(COL_USERS));
                    ListView_SetColumn(hLv, 5, &lvc);
                }
            }
        }
        break;

    case KAILLERA_OPTIONS:
        SetDlgCaption(hDlg, LNG(CAPTION_ROOM_OPTIONS));
        SetDlgText(hDlg, IDOK, LNG(BTN_OK));
        SetDlgText(hDlg, IDCANCEL, LNG(BTN_CANCEL));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Max players:", LNG(OPTS_LBL_MAX_PLAYERS));
        ReplaceStaticText(hDlg, "Max ping:", LNG(OPTS_LBL_MAX_PING));
        ReplaceStaticText(hDlg, "On user join:", LNG(OPTS_LBL_ON_USER_JOIN));
        ReplaceStaticText(hDlg, "Window flash:", LNG(OPTS_LBL_WINDOW_FLASH));
        ReplaceStaticText(hDlg, "Beep:", LNG(OPTS_LBL_BEEP));
        break;

    case RECORDER_PLAYBACK:
        SetDlgCaption(hDlg, LNG(CAPTION_PLAYER));
        SetDlgText(hDlg, BTN_PLAY, LNG(BTN_PLAY));
        SetDlgText(hDlg, BTN_STOP, LNG(BTN_STOP));
        SetDlgText(hDlg, IDCREFRESH, LNG(BTN_REFRESH));
        SetDlgText(hDlg, BTN_DELETE, LNG(BTN_DELETE_REC));
        SetDlgText(hDlg, BTN_OPENFOLDER, LNG(BTN_OPEN_FOLDER));
        SetDlgText(hDlg, RB_MODE_P2P, LNG(PLAYER_RB_P2P));
        SetDlgText(hDlg, RB_MODE_CLIENT, LNG(PLAYER_RB_SERVER));
        SetDlgText(hDlg, RB_MODE_PLAYBACK, LNG(PLAYER_RB_PLAYBACK));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Mode", LNG(PLAYER_GRP_MODE));
        OptionsBar_Refresh(hDlg);
        break;

    case N02_ERRORDLG:
        SetDlgCaption(hDlg, LNG(CAPTION_EXCEPTION));
        SetDlgText(hDlg, BTN_REPORT, LNG(ERROR_BTN_REPORT));
        SetDlgText(hDlg, BTN_CLOSE, LNG(BTN_CLOSE));
        break;

    case N02_STATSDLG:
        SetDlgCaption(hDlg, LNG(CAPTION_STATS));
        SetDlgText(hDlg, BTN_RESET, LNG(BTN_RESET));
        SetDlgText(hDlg, BTN_COPYLOG, LNG(STATS_BTN_COPY));
        SetDlgText(hDlg, BTN_CLOSE, LNG(BTN_CLOSE));
        break;

    default:
        break;
    }
}
