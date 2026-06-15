/*
 * kaillera_lang_dlg.cpp - Dialog localization helper for N02 Kaillera Client
 *
 * Applies localized strings from the .lng file to Win32 dialog controls
 * that were defined in the .rc resource file with hardcoded English text.
 */

#include "kaillera_lang_dlg.h"
#include "../resource.h"

/* Helper: set dialog item text if the control exists */
static void SetDlgText(HWND hDlg, int controlId, const char* text) {
    HWND ctl = GetDlgItem(hDlg, controlId);
    if (ctl != NULL) {
        SetWindowTextA(ctl, text);
    }
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
};

static BOOL CALLBACK FindStaticCallback(HWND hwnd, LPARAM lParam) {
    FindStaticCtx* ctx = (FindStaticCtx*)lParam;
    char buf[128];
    buf[0] = 0;
    GetWindowTextA(hwnd, buf, sizeof(buf));
    if (strcmp(buf, ctx->oldText) == 0) {
        SetWindowTextA(hwnd, ctx->newText);
        ctx->found = true;
        return FALSE; /* stop enumerating */
    }
    return TRUE; /* continue */
}

static void ReplaceStaticText(HWND hDlg, const char* oldText, const char* newText) {
    FindStaticCtx ctx = { oldText, newText, false };
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
        SetDlgText(hDlg, RB_MODE_P2P, LNG(MAIN_RB_P2P));
        SetDlgText(hDlg, RB_MODE_CLIENT, LNG(MAIN_RB_SERVER));
        SetDlgText(hDlg, RB_MODE_PLAYBACK, LNG(MAIN_RB_PLAYBACK));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "Nick:", LNG(MAIN_LBL_NICK));
        ReplaceStaticText(hDlg, "Local List", LNG(SSDLG_GRP_LOCAL_LIST));
        ReplaceStaticText(hDlg, "Frame Delay:", LNG(SSDLG_LBL_FRAME_DELAY));
        ReplaceStaticText(hDlg, "Mode", LNG(MAIN_GRP_MODE));
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
        SetDlgText(hDlg, IDC_LANG_LABEL, LNG(ABOUT_LBL_LANGUAGE));
        /* About dialog static text labels */
        ReplaceStaticText(hDlg, "Kaillera protocol and API:\n(c) 2001-2002 Christophe Thibault\n------------------------------------------------------", LNG(ABOUT_LBL_CREDITS));
        ReplaceStaticText(hDlg, "n02 (c) Open Kaillera\nn02.p2p (c) Open Kaillera", LNG(ABOUT_LBL_N02));
        break;

    case KAILLERA_CUSTOMIP:
        SetDlgCaption(hDlg, LNG(CAPTION_CUSTOM_IP));
        SetDlgText(hDlg, BTN_CONNECT, LNG(CUSTOMIP_BTN_CONNECT));
        /* IDC_STATIC labels */
        ReplaceStaticText(hDlg, "IP:", LNG(ITEMEDIT_LBL_IP));
        break;

    case KAILLERA_MLIST:
        SetDlgCaption(hDlg, LNG(CAPTION_MASTER_LIST));
        SetDlgText(hDlg, BTN_CONNECT, LNG(MLIST_BTN_CONNECT));
        SetDlgText(hDlg, BTN_CLOSE, LNG(MLIST_BTN_CLOSE));
        SetDlgText(hDlg, BTN_ADD, LNG(MLIST_BTN_ADD_LOCAL));
        SetDlgText(hDlg, BTN_REFRESH, LNG(MLIST_BTN_REFRESH));
        SetDlgText(hDlg, BTN_WGAMES, LNG(MLIST_BTN_WAITING_GAMES));
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
