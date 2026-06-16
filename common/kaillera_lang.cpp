/*
 * kaillera_lang.cpp - Multilingual string system for N02 Kaillera Client
 *
 * Implements a lightweight key=value language file loader.
 * The .lng format is INI-like: KEY=VALUE, with ; comments and blank lines.
 * All data is stored in a single contiguous allocation for cache-friendliness
 * and zero-leak cleanup.
 */

#include "kaillera_lang.h"
#include "nSettings.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal data structures
 * --------------------------------------------------------------------------- */

/* A single key-value pair. Both pointers point into the shared string buffer. */
struct LangEntry {
    const char* key;
    const char* value;
};

/* The global language state. */
static struct {
    LangEntry*  entries;      /* Array of key-value pairs              */
    int         count;        /* Number of entries                     */
    int         capacity;     /* Allocated capacity of entries array   */
    char*       stringPool;   /* Single allocation holding all strings */
    size_t      stringPoolSz; /* Size of stringPool                    */
    char        langName[64]; /* Display name of loaded language       */
    char        langFile[64]; /* Filename-based identifier (e.g. Russian) */
    bool        initialized;  /* Whether LangInit() succeeded          */
} g_lang = {0};

/* ---------------------------------------------------------------------------
 * Helper: trim whitespace in-place
 * --------------------------------------------------------------------------- */
static char* TrimInPlace(char* s) {
    if (s == NULL) return s;
    /* Trim leading */
    while (*s == ' ' || *s == '\t') s++;
    if (*s == 0) return s;
    /* Trim trailing */
    char* end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = 0;
        end--;
    }
    return s;
}

/* ---------------------------------------------------------------------------
 * Helper: decode escape sequences in-place (\n -> newline, etc.)
 * --------------------------------------------------------------------------- */
static void DecodeEscapes(char* s) {
    if (s == NULL) return;
    char* dst = s;
    while (*s) {
        if (*s == '\\' && *(s + 1)) {
            switch (*(s + 1)) {
                case 'n':  *dst++ = '\n'; s += 2; break;
                case 'r':  *dst++ = '\r'; s += 2; break;
                case 't':  *dst++ = '\t'; s += 2; break;
                case '\\': *dst++ = '\\'; s += 2; break;
                default:   *dst++ = *s++;          break;
            }
        } else {
            *dst++ = *s++;
        }
    }
    *dst = 0;
}

/* ---------------------------------------------------------------------------
 * Built-in English defaults (used when .lng file is missing or incomplete)
 * This ensures the UI always shows readable text even without the .lng file.
 * --------------------------------------------------------------------------- */
static const LangEntry g_defaultEntries[] = {
    {"ABOUT_BTN_LICENSE", "License"},
    {"ABOUT_BTN_USAGE_POLICY", "Useage Policy"},
    {"ABOUT_BTN_WEBSITE", "Open Kaillera Website"},
    {"ABOUT_LBL_CREDITS", "Kaillera protocol and API:\n(c) 2001-2002 Christophe Thibault\n------------------------------------------------------"},
    {"ABOUT_LBL_LANGUAGE", "Language:"},
    {"ABOUT_LBL_N02", "n02 (c) Open Kaillera\nn02.p2p (c) Open Kaillera"},
    {"ADV_HOST_FORMAT", "%s - %d player(s)"},
    {"ADV_JOIN_FORMAT", "<%s> | %s - %d player(s)"},
    {"BTN_ABOUT", "About"},
    {"BTN_ADD", "Add"},
    {"BTN_CANCEL", "Cancel"},
    {"BTN_CLOSE", "Close"},
    {"BTN_CONNECT", "Connect"},
    {"BTN_COPY", "Copy"},
    {"BTN_DELETE", "Delete"},
    {"BTN_DELETE_REC", "Delete"},
    {"BTN_EDIT", "Edit"},
    {"BTN_OK", "OK"},
    {"BTN_OPEN_FOLDER", "Open Folder"},
    {"BTN_OPTIONS", "Options"},
    {"BTN_PING", "Ping"},
    {"BTN_PLAY", "Play"},
    {"BTN_REFRESH", "Refresh"},
    {"BTN_RESET", "Reset"},
    {"BTN_STOP", "Stop"},
    {"BTN_TRACE", "Trace"},
    {"CAPTION_ABOUT", "About"},
    {"CAPTION_CONNECTION_WINDOW", "Connection Window"},
    {"CAPTION_CUSTOM_IP", "Custom Server IP"},
    {"CAPTION_EXCEPTION", "Exception"},
    {"CAPTION_ITEM_EDIT", "N02"},
    {"CAPTION_MAIN", "N02"},
    {"CAPTION_MASTER_LIST", "N02"},
    {"CAPTION_PLAYER", "Player"},
    {"CAPTION_ROOM_OPTIONS", "Room Options"},
    {"CAPTION_SERVER_ROOM", "N02"},
    {"CAPTION_SERVER_SELECT", "N02"},
    {"CAPTION_STATS", "Stats"},
    {"CHAT_DROPPED_FORMAT", "* Dropped: %s (Player %i)"},
    {"CHAT_JOIN_FORMAT", "* Joins: %s (Ping: %i)"},
    {"CHAT_KICKED", "* You have been kicked out of the game"},
    {"CHAT_LOGIN_STATUS_FORMAT", "* %s"},
    {"CHAT_MINIMAL_GUI_NOTICE", "- Minimal gui update mode is on. GUI related items outside your game will be disabled. Please reconnect to server once you've finished playing."},
    {"CHAT_MOTD_FORMAT", "- %s"},
    {"CHAT_MSG_FORMAT", "<%s> %s"},
    {"CHAT_NETSYNC_WAIT", "waiting for others"},
    {"CHAT_PART_FORMAT", "* Parts: %s"},
    {"CHAT_PLAYER_JOIN_FORMAT", "* Joins: %s"},
    {"CHAT_PLAYER_PART_FORMAT", "* Parts: %s"},
    {"CHAT_PM_FORMAT", "%s- %s\r\n"},
    {"CHAT_PM_PREFIX", "TO: <"},
    {"CHAT_RECORDING_NOTICE", "- your game will be recorded"},
    {"CHAT_STARTING_FORMAT", "* Starting: %s (%i/%i)"},
    {"CHAT_STARTING_HINT", "- press \"Drop\" if emulator fails"},
    {"COL_CODE", "Code"},
    {"COL_CONNECTION", "Connection"},
    {"COL_DATE", "Date"},
    {"COL_DELAY", "Delay"},
    {"COL_DURATION", "Duration"},
    {"COL_EMULATOR", "Emulator"},
    {"COL_FILENAME", "Filename"},
    {"COL_GAME", "Game"},
    {"COL_GAME_ID", "GameID"},
    {"COL_HOST", "Host"},
    {"COL_IP", "IP"},
    {"COL_LOCATION", "Location"},
    {"COL_NAME", "Name"},
    {"COL_NICK", "Nick"},
    {"COL_PING", "Ping"},
    {"COL_PLAYERS", "Players"},
    {"COL_SERVER", "Server"},
    {"COL_SERVER_NAME", "Name"},
    {"COL_SIZE", "Size"},
    {"COL_STATUS", "Status"},
    {"COL_UID", "UID"},
    {"COL_USER", "User"},
    {"COL_USERS", "Users"},
    {"COL_VER", "ver"},
    {"COL_WAITING", "Waiting"},
    {"CONN_BTN_ADVANCED", "Advanced"},
    {"CONN_BTN_CHAT", "&Chat"},
    {"CONN_BTN_COPY_CODE", "Copy"},
    {"CONN_BTN_COPY_IP", "Copy IP"},
    {"CONN_BTN_DONT_ALLOW_LIST", "Dont allow list"},
    {"CONN_BTN_DROP_GAME", "&Drop Game"},
    {"CONN_BTN_EJECT_PEER", "Eject peer"},
    {"CONN_BTN_PING", "ping"},
    {"CONN_BTN_RETR", "re&tr"},
    {"CONN_BTN_STATS", "stats"},
    {"CONN_CHK_ENLIST", "Show on public game list"},
    {"CONN_CHK_READY", "Ready"},
    {"CONN_CHK_RECORD", "&Record game"},
    {"CONN_GRP_HOST", "Host:"},
    {"CONN_LBL_CONNECT_CODE", "Connect code:"},
    {"CONN_LBL_FRAME_DELAY", "Frame Delay:"},
    {"CONN_LBL_WAITING", "(waiting)"},
    {"CONN_TYPE_AVG", "Avg"},
    {"CONN_TYPE_BAD", "Bad"},
    {"CONN_TYPE_EXC", "Exc"},
    {"CONN_TYPE_GOOD", "Good"},
    {"CONN_TYPE_LAN", "LAN"},
    {"CONN_TYPE_LOW", "Low"},
    {"CTX_CREATE", "Create"},
    {"CTX_FIND_USER", "Find user"},
    {"CTX_IGNORE", "Ignore"},
    {"CTX_JOIN", "Join"},
    {"CTX_SEND_MESSAGE", "Send Message"},
    {"CTX_UNIGNORE", "Unignore"},
    {"CUSTOMIP_BTN_CONNECT", "Connect"},
    {"CUSTOMIP_LBL_IP", "IP:"},
    {"DELAY_FORMAT", "%i frames"},
    {"DELAY_FRAMES", "%i frames"},
    {"DLG_TITLE_ADD", "Add"},
    {"DLG_TITLE_EDIT", "Edit"},
    {"ERROR_BTN_REPORT", "Report as bug"},
    {"ERR_CORE_INIT_FAILED", "Core Initialization Failed"},
    {"ERR_EMU_MISMATCH_BODY", "Emulator/version mismatch and the game may desync.\nDo you want to continue?"},
    {"ERR_EMU_MISMATCH_TITLE", "Error"},
    {"ERR_JOIN_RUNNING_GAME", "Joining running game is not allowed"},
    {"ERR_NO_GAME_IN_LOBBY", "This lobby has no game to start."},
    {"ERR_ROM_NOT_FOUND", "The rom '%s' is not in your list."},
    {"EXCEPTION_ACCESS_VIOLATION", "EXCEPTION_ACCESS_VIOLATION"},
    {"EXCEPTION_ARRAY_BOUNDS_EXCEEDED", "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
    {"EXCEPTION_BREAKPOINT", "EXCEPTION_BREAKPOINT"},
    {"EXCEPTION_DATATYPE_MISALIGNMENT", "EXCEPTION_DATATYPE_MISALIGNMENT"},
    {"EXCEPTION_FLT_DENORMAL_OPERAND", "EXCEPTION_FLT_DENORMAL_OPERAND"},
    {"EXCEPTION_FLT_DIVIDE_BY_ZERO", "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
    {"EXCEPTION_FLT_INEXACT_RESULT", "EXCEPTION_FLT_INEXACT_RESULT"},
    {"EXCEPTION_FLT_INVALID_OPERATION", "EXCEPTION_FLT_INVALID_OPERATION"},
    {"EXCEPTION_FLT_OVERFLOW", "EXCEPTION_FLT_OVERFLOW"},
    {"EXCEPTION_FLT_STACK_CHECK", "EXCEPTION_FLT_STACK_CHECK"},
    {"EXCEPTION_FLT_UNDERFLOW", "EXCEPTION_FLT_UNDERFLOW"},
    {"EXCEPTION_ILLEGAL_INSTRUCTION", "EXCEPTION_ILLEGAL_INSTRUCTION"},
    {"EXCEPTION_INT_DIVIDE_BY_ZERO", "EXCEPTION_INT_DIVIDE_BY_ZERO"},
    {"EXCEPTION_INT_OVERFLOW", "EXCEPTION_INT_OVERFLOW"},
    {"EXCEPTION_INVALID_DISPOSITION", "EXCEPTION_INVALID_DISPOSITION"},
    {"EXCEPTION_IN_PAGE_ERROR", "EXCEPTION_IN_PAGE_ERROR"},
    {"EXCEPTION_NONCONTINUABLE_EXCEPTION", "EXCEPTION_NONCONTINUABLE_EXCEPTION"},
    {"EXCEPTION_PRIV_INSTRUCTION", "EXCEPTION_PRIV_INSTRUCTION"},
    {"EXCEPTION_RECORD_FORMAT", "EXCEPTION_RECORD {\r\n\tExceptionCode=%s;\r\n\tExceptionFlags=%i;\r\n\tExceptionAddress=0x%08x;\r\n\tExceptionInformation=%i;\r\n};"},
    {"EXCEPTION_SINGLE_STEP", "EXCEPTION_SINGLE_STEP"},
    {"EXCEPTION_STACK_OVERFLOW", "EXCEPTION_STACK_OVERFLOW"},
    {"FD_1", "1 frame (8ms)"},
    {"FD_2", "2 frames (24ms)"},
    {"FD_3", "3 frames (40ms)"},
    {"FD_4", "4 frames (56ms)"},
    {"FD_5", "5 frames (72ms)"},
    {"FD_6", "6 frames (88ms)"},
    {"FD_7", "7 frames (104ms)"},
    {"FD_8", "8 frames (120ms)"},
    {"FD_9", "9 frames (136ms)"},
    {"FD_AUTO", "Auto"},
    {"GAME_CREATE", "Create"},
    {"GAME_SWAP", "Swap"},
    {"GUI_THREAD_ERR_BODY", "Please reopen kaillera"},
    {"GUI_THREAD_ERR_TITLE", "GUI thread exception"},
    {"ITEMEDIT_LBL_IP", "IP:"},
    {"ITEMEDIT_LBL_NAME", "Name:"},
    {"K_30FPS_HALVED", "30fps mode: halved delay from %i to %i frames"},
    {"K_ALL_PLAYERS_READY", "All players are ready"},
    {"K_CONNECTING_TO", "Connecting to %s:%i"},
    {"K_ERR_CONNECTING", "error connecting to server"},
    {"K_IGNORING_GAMEBEGN", "Ignoring GAMEBEGN (leave requested)"},
    {"K_LOGGING_IN", "logging in"},
    {"K_LOST_CONNECTION", "Lost Connection"},
    {"K_PING_SPOOF_ENABLED", "Ping spoofing enabled: %dms delay"},
    {"K_PING_SPOOF_MSG", "Ping spoofing: %dms (target: %df)"},
    {"K_PING_SPOOF_SET", "Ping spoofing set to %dms"},
    {"K_SERVER_DELAY_INFO", "Server says: delay is %i frames (throughput=%i, conset=%i)"},
    {"K_SERVER_FULL", "Server is full"},
    {"K_SERVER_REPLIED", "server replied"},
    {"LOBBY_AWAY", "*Away (leave messages)"},
    {"LOBBY_CHAT", "*Chat (not game)"},
    {"MAIN_BTN_CONNECT", "&Connect"},
    {"MAIN_BTN_HOST", "Ho&st"},
    {"MAIN_BTN_PASTE_GO", "Paste && Go"},
    {"MAIN_BTN_WAITING_GAMES", "&waiting games"},
    {"MAIN_GRP_MODE", "Mode"},
    {"MAIN_LBL_GAME", "Game:"},
    {"MAIN_LBL_HOST_PORT", "Host port:"},
    {"MAIN_LBL_IP_CODE", "IP/Code:"},
    {"MAIN_LBL_NICK", "Nick:"},
    {"MAIN_LBL_STORED", "Stored:"},
    {"MAIN_RB_P2P", "P2P"},
    {"MAIN_RB_PLAYBACK", "Playback"},
    {"MAIN_RB_SERVER", "Server"},
    {"MLIST_BTN_ADD_LOCAL", "&Add to local list"},
    {"MLIST_BTN_CLOSE", "Clo&se"},
    {"MLIST_BTN_CONNECT", "&Connect"},
    {"MLIST_BTN_REFRESH", "&Refresh"},
    {"MLIST_BTN_WAITING_GAMES", "&Waiting Games"},
    {"MODE_1_P2P", "1. P2P"},
    {"MODE_2_SERVER", "2. Server"},
    {"MODE_3_PLAYBACK", "3. Playback"},
    {"MODE_P2P_ONLY", "P2P"},
    {"MSL_CLEANING_UP", "Cleaning up..."},
    {"MSL_DOWNLOADING", "Downloading list..."},
    {"MSL_ERROR_REFRESHING", "Error refreshing list"},
    {"MSL_GAMES_FOUND", "%i games found"},
    {"MSL_MASTER_SERVERS", "Master servers list..."},
    {"MSL_MASTER_WAITING", "Master servers waiting games list..."},
    {"MSL_NO_GAMES", "No games found"},
    {"MSL_PARSING", "Parsing list..."},
    {"MSL_PINGED_GAMES", "pinged %i games"},
    {"MSL_PINGED_SERVERS", "pinged %i servers"},
    {"MSL_PINGING", "Pinging list..."},
    {"MSL_REFRESH", "Refresh"},
    {"MSL_SERVERS_FOUND", "%i servers found"},
    {"MSL_STOP", "Stop"},
    {"MSL_WAITING_GAMES", "waiting games..."},
    {"MSL_WAITING_GAMES_FOUND", "%i waiting games found"},
    {"N02_WINDOW_TITLE", "N02 %s"},
    {"OPTS_LBL_BEEP", "Beep:"},
    {"OPTS_LBL_MAX_PING", "Max ping:"},
    {"OPTS_LBL_MAX_PLAYERS", "Max players:"},
    {"OPTS_LBL_ON_USER_JOIN", "On user join:"},
    {"OPTS_LBL_WINDOW_FLASH", "Window flash:"},
    {"OPT_FALSE", "False"},
    {"OPT_TRUE", "True"},
    {"P2P_30FPS_HALVED", "30fps mode: halved delay from %i to %i frames"},
    {"P2P_BOTH_READY", "both players are ready, starting game"},
    {"P2P_CALCULATED_DELAY", "== Calculated delay %i frame(s)"},
    {"P2P_CALCULATED_PING", "== Calculated Ping = %ims"},
    {"P2P_CANT_QUIT", "Cant quit while in game"},
    {"P2P_CHAT_FORMAT", "<%s> %s"},
    {"P2P_CHECKING_IP", "(checking ip)"},
    {"P2P_CODE_COPIED", "Copied connect code to clipboard"},
    {"P2P_CODE_RECEIVED", "Received connect code: %s"},
    {"P2P_CONNECTING_TO", "Connecting to %s:%i"},
    {"P2P_CONN_REQUEST", "Connection Request from %s (%s).. Waiting for reconfirmation..."},
    {"P2P_DATA_TIMEOUT", "Data timeout. Dropping game"},
    {"P2P_DELAY_OVERRIDE", "Calculated delay: %i frames, using override: %i frames"},
    {"P2P_DONE", "Done"},
    {"P2P_DONUT_LOADED", "Donut loaded."},
    {"P2P_DROPPING_GAME", "dropping game"},
    {"P2P_EMU_DIFF_ALERT", "Emulator/version difference alert! Game may desync!"},
    {"P2P_ERR_CONNECT", "Error connecting to specified host/port"},
    {"P2P_ERR_NO_GAME", "Pick a valid game"},
    {"P2P_EVERYONE_LOADED", "== Everyone Loaded"},
    {"P2P_FALSE", "false"},
    {"P2P_FD_1", "1 frame (0-33ms)"},
    {"P2P_FD_2", "2 frames (34-67ms)"},
    {"P2P_FD_3", "3 frames (68-99ms)"},
    {"P2P_FD_4", "4 frames (100-133ms)"},
    {"P2P_FD_5", "5 frames (134-167ms)"},
    {"P2P_FD_6", "6 frames (168-199ms)"},
    {"P2P_FD_7", "7 frames (200-233ms)"},
    {"P2P_FD_8", "8 frames (234-267ms)"},
    {"P2P_FD_9", "9 frames (268+ms)"},
    {"P2P_FD_AUTO", "Auto"},
    {"P2P_GAMESYNC_COUNTDOWN", "gamesync in less than %i second(s)"},
    {"P2P_GAME_NOT_FOUND", "ERROR: Game not found on your local list"},
    {"P2P_HALFDELAY_DISABLED", "DISABLED"},
    {"P2P_HALFDELAY_ENABLED", "ENABLED"},
    {"P2P_HALFDELAY_MSG", "Half delay mode %s (for 30fps games: Mario Kart, 1080, THPS)"},
    {"P2P_HOSTING_ON_PORT", "Hosting %s on port %i"},
    {"P2P_HOSTING_WARNING", "WARNING: Hosting requires hosting ports to be forwarded and enabled in firewalls."},
    {"P2P_IP_COPIED", "Copied IP to clipboard."},
    {"P2P_IRRELEVENT", "irrelevent"},
    {"P2P_MUFFIN_LOADED", "Muffin loaded, waiting for Donut."},
    {"P2P_NAT_BUSY", "NAT traversal: host is busy. Please wait and try again."},
    {"P2P_NAT_CONNECTING", "NAT traversal: connecting to %s:%i"},
    {"P2P_NAT_CONNECT_ERROR", "NAT traversal: error connecting to %s:%i"},
    {"P2P_NAT_ERR", "NAT traversal <- ERR %s"},
    {"P2P_NAT_FALLBACK_FAILED", "NAT traversal: fallback connect failed"},
    {"P2P_NAT_FALLBACK_FORMAT", "NAT traversal: %s. Falling back to %s:%i"},
    {"P2P_NAT_FALLBACK_NO_NAME", "NAT traversal: falling back to %s:%i"},
    {"P2P_NAT_HOST", "NAT traversal <- HOST %s:%i"},
    {"P2P_NAT_HOST_IGNORED", "NAT traversal <- HOST (ignored; not joining-by-code)"},
    {"P2P_NAT_LOOKUP", "NAT traversal: looking up host for code %s"},
    {"P2P_NAT_OK", "NAT traversal <- OK"},
    {"P2P_NAT_OUT", "NAT traversal -> %s:%i: %s"},
    {"P2P_NAT_PEER", "NAT traversal: peer %s:%i"},
    {"P2P_NAT_PEER_IGNORED", "NAT traversal <- PEER (ignored; not hosting-by-code)"},
    {"P2P_NAT_PEER_INFO", "NAT traversal <- PEER %s:%i"},
    {"P2P_NAT_PUNCH", "NAT traversal -> %s:%i: punch x10"},
    {"P2P_NAT_REGOK", "NAT traversal <- REGOK code=%s observed=%s:%s"},
    {"P2P_NAT_REGOK_IGNORED", "NAT traversal <- REGOK (ignored; not hosting-by-code)"},
    {"P2P_NAT_SERVER_RESP", "NAT traversal: server response was: %s"},
    {"P2P_NAT_TIMEOUT", "NAT traversal: timed out (try direct IP/port-forwarding or server mode)"},
    {"P2P_NAT_UNAVAILABLE", "Unable to contact NAT server, hosting by IP. You may need to manually port forward."},
    {"P2P_NAT_UNHANDLED", "NAT traversal <- (unhandled) %s"},
    {"P2P_NAT_UNKNOWN_TOKEN", "NAT traversal: ignoring PEER for unknown token"},
    {"P2P_NOT_READY", "not ready"},
    {"P2P_PEER_DROPPED", "peer dropped"},
    {"P2P_PEER_DROPPED_DIFF_EMU", "Peer dropped connection (Probably different emu version)"},
    {"P2P_PEER_DROPPED_NO_GAME", "Peer dropped connection (probbly doensot have the game)"},
    {"P2P_PEER_LEFT", "Peer left"},
    {"P2P_PEER_READY_FORMAT", "%s is %s"},
    {"P2P_PEER_RECONFIRMED", "Peer reconfirmed connection"},
    {"P2P_PEER_REPLIED", "Peer replied: %s (%s)"},
    {"P2P_PING_TIMEOUT", "Ping timeout. Peer dropped."},
    {"P2P_READY", "ready"},
    {"P2P_READY_MARKED", "You are marked as %s"},
    {"P2P_REINIT_SERVER", "reinitializing server..."},
    {"P2P_SOCKET_ERROR", "Error initializing sockets"},
    {"P2P_SOCKET_INIT_ERROR", "Error initializing socket at specified port"},
    {"P2P_SSRV", "SSRV: %s"},
    {"P2P_TRUE", "true"},
    {"P2P_UNKNOWN_CMD", "Unknown command \"%s\""},
    {"P2P_USING_VERSION_CLIENT", "Using version: %s"},
    {"P2P_USING_VERSION_HOST", "Using version: %s - Things may behave in an unexpected manner if different versions are used"},
    {"P2P_YOUR_IP", "Your IP address is: %s"},
    {"PING_FORMAT", "ping: %i ms pl: %i"},
    {"PING_NA", "N/A"},
    {"PLAYER_EMU_MISMATCH", "Application name mismatch.\nExpected \"%s\" but recieved \"%s\".\nUsing a different emulator for playback may cause things to behave in an unexpected manner.\nDo you want to continue?"},
    {"PLAYER_ERR", "Error"},
    {"PLAYER_ERR_TITLE", "Error"},
    {"PLAYER_FILE_TOO_SHORT", "File too short"},
    {"PLAYER_GRP_MODE", "Mode"},
    {"PLAYER_PLAYER_D", "Player %d"},
    {"PLAYER_RB_P2P", "P2P"},
    {"PLAYER_RB_PLAYBACK", "Playback"},
    {"PLAYER_RB_SERVER", "Server"},
    {"QUIT_MSG", "Open Kaillera - n02"},
    {"REC_ERROR_TITLE", "recording - error"},
    {"REC_FAILED", "recording %s failed error = %i"},
    {"SDLG_BTN_ADVERTISE", "Advertise"},
    {"SDLG_BTN_CHAT", "Chat"},
    {"SDLG_BTN_CREATE", "Create"},
    {"SDLG_BTN_DROP", "Drop"},
    {"SDLG_BTN_GAME_CHAT", "Chat"},
    {"SDLG_BTN_KICK", "&Kick"},
    {"SDLG_BTN_LAGSTAT", "Lagstat"},
    {"SDLG_BTN_LEAVE", "Leave"},
    {"SDLG_BTN_OPTIONS", "Options"},
    {"SDLG_BTN_START", "&Start"},
    {"SDLG_CHK_RECORD", "&Record game"},
    {"SDLG_LBL_DELAY", "delay: 3 frames"},
    {"SDLG_LBL_JOIN_MSG", "Join msg:"},
    {"SDLG_LBL_SPEED", "speed: 58fps"},
    {"SPEED_FORMAT", "%i fps/%i pps"},
    {"SSDLG_BTN_CUSTOM_IP", "Custom Server IP"},
    {"SSDLG_BTN_LIVE_SERVER_LIST", "Live Server List"},
    {"SSDLG_BTN_WAITING_GAMES", "Waiting Games"},
    {"SSDLG_GRP_LOCAL_LIST", "Local List"},
    {"SSDLG_LBL_FRAME_DELAY", "Frame Delay:"},
    {"STATS_BTN_COPY", "Copy"},
    {"STATS_IN_HEADER", "in\n--------------------\nrate: %i/s (%i B/s)\ntotal: %i (%i Bytes)\n\n"},
    {"STATS_OTHERS", "others\n--------------------\nloss: %i\nretransmits:%i/%i\nmisorder:%i\naddrdism:%i\ncoundism:%i"},
    {"STATS_OUT_HEADER", "out\n--------------------\nrate: %i/s (%i B/s)\ntotal: %i (%i Bytes)\n\n"},
    {"STATS_P2P", "\n\np2p\n--------------------\n%s"},
    {"STATS_THREAD_EXCEPTION", "StatsThread Exception"},
    {"STATUS_IDLE", "Idle"},
    {"STATUS_PLAYING", "Playing"},
    {"STATUS_WAITING", "Waiting"},
    {"TAB_CONNECT", "Conn&ect"},
    {"TAB_HOST", "Ho&st"},
    {"TIMESTAMP_AM", "AM"},
    {"TIMESTAMP_FORMAT", "[%d:%02d %s]"},
    {"TIMESTAMP_PM", "PM"},
    {"TITLE_CONNECTED", "Connected to %s (%i users & %i games)"},
    {"TITLE_CONNECTING", "Connecting to %s"},
    {"TITLE_HOSTING_P2P", "Hosting P2P"},
    {"TITLE_IN_GAME", "Game %s"},
    {"TITLE_P2P_CONNECTED", "Connected to %s (%s)"},
    {"TITLE_P2P_CONNECTION", "P2P Connection Window"},
    {"TRACE_STACK_HEADER", "TRACE STACK: \r\n"},
};
static const int g_defaultCount = sizeof(g_defaultEntries) / sizeof(g_defaultEntries[0]);

/* ---------------------------------------------------------------------------
 * Helper: comparison function for qsort / bsearch (case-sensitive)
 * --------------------------------------------------------------------------- */
static int __cdecl LangEntryCompare(const void* a, const void* b) {
    return strcmp(((const LangEntry*)a)->key, ((const LangEntry*)b)->key);
}

/* Helper: binary search in the defaults table */
static const char* LangGetDefault(const char* key) {
    int lo = 0, hi = g_defaultCount - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(g_defaultEntries[mid].key, key);
        if (cmp == 0) return g_defaultEntries[mid].value;
        if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Internal: free all allocated memory
 * --------------------------------------------------------------------------- */
static void LangCleanup() {
    if (g_lang.entries) {
        free(g_lang.entries);
        g_lang.entries = NULL;
    }
    if (g_lang.stringPool) {
        free(g_lang.stringPool);
        g_lang.stringPool = NULL;
    }
    g_lang.count = 0;
    g_lang.capacity = 0;
    g_lang.stringPoolSz = 0;
    g_lang.langName[0] = 0;
    g_lang.langFile[0] = 0;
    g_lang.initialized = false;
}

/* ---------------------------------------------------------------------------
 * Internal: load a .lng file and populate the entries array
 * --------------------------------------------------------------------------- */
static bool LangLoadFile(const char* filePath, const char* langIdent) {
    FILE* f = NULL;
    if (fopen_s(&f, filePath, "rb") != 0 || f == NULL) {
        return false;
    }

    /* Determine file size */
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 2 * 1024 * 1024) { /* Max 2MB */
        fclose(f);
        return false;
    }

    /* Read entire file into a temporary buffer */
    char* buf = (char*)malloc((size_t)fileSize + 1);
    if (buf == NULL) {
        fclose(f);
        return false;
    }

    size_t bytesRead = fread(buf, 1, (size_t)fileSize, f);
    fclose(f);
    buf[bytesRead] = 0;

    /* Skip UTF-8 BOM if present (0xEF 0xBB 0xBF) */
    if (bytesRead >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        bytesRead -= 3;
        memmove(buf, buf + 3, bytesRead + 1); /* shift data to start of buffer */
    }

    /* Convert UTF-8 to the system ANSI codepage.
     * The .lng files are saved in UTF-8, but Win32 "A" APIs expect strings
     * in the system ANSI codepage (e.g. Windows-1251 for Russian).
     * Without this conversion, non-ASCII characters appear garbled. */
    {
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        buf, (int)bytesRead, NULL, 0);
        if (wlen > 0) {
            wchar_t* wbuf = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
            if (wbuf) {
                MultiByteToWideChar(CP_UTF8, 0, buf, (int)bytesRead, wbuf, wlen);
                wbuf[wlen] = 0;

                int alen = WideCharToMultiByte(CP_ACP, 0, wbuf, wlen,
                                                NULL, 0, NULL, NULL);
                if (alen > 0) {
                    char* abuf = (char*)malloc(alen + 1);
                    if (abuf) {
                        WideCharToMultiByte(CP_ACP, 0, wbuf, wlen,
                                            abuf, alen, NULL, NULL);
                        abuf[alen] = 0;
                        free(buf);
                        buf = abuf;
                        bytesRead = (size_t)alen;
                    }
                }
                free(wbuf);
            }
        }
        /* If conversion fails (e.g. file is already ANSI), keep original buf */
    }

    /* Count lines to estimate capacity */
    int lineCount = 0;
    for (size_t i = 0; i < bytesRead; i++) {
        if (buf[i] == '\n') lineCount++;
    }
    lineCount += 2; /* safety margin */

    /* Allocate entries array */
    LangEntry* newEntries = (LangEntry*)malloc(sizeof(LangEntry) * (size_t)lineCount);
    if (newEntries == NULL) {
        free(buf);
        return false;
    }

    /* Parse line by line. We reuse the file buffer as the string pool
       by NUL-terminating each key and value in-place. */
    int entryCount = 0;
    char* line = buf;
    char* fileEnd = buf + bytesRead;

    while (line < fileEnd) {
        /* Find end of line */
        char* lineEnd = line;
        while (lineEnd < fileEnd && *lineEnd != '\r' && *lineEnd != '\n') {
            lineEnd++;
        }
        *lineEnd = 0;

        /* Skip to next line */
        char* nextLine = lineEnd + 1;
        while (nextLine < fileEnd && (*nextLine == '\r' || *nextLine == '\n')) {
            nextLine++;
        }

        /* Trim the line */
        char* trimmed = TrimInPlace(line);

        /* Skip empty lines and comments */
        if (*trimmed == 0 || *trimmed == ';') {
            line = nextLine;
            continue;
        }

        /* Find the = separator */
        char* eq = strchr(trimmed, '=');
        if (eq == NULL) {
            line = nextLine;
            continue;
        }

        /* Split into key and value */
        *eq = 0;
        char* key = TrimInPlace(trimmed);
        char* value = TrimInPlace(eq + 1);

        /* Decode escape sequences in value */
        DecodeEscapes(value);

        /* Handle LANG_NAME specially */
        if (strcmp(key, "LANG_NAME") == 0) {
            strncpy_s(g_lang.langName, value, _TRUNCATE);
            line = nextLine;
            continue;
        }

        /* Store the entry */
        if (entryCount < lineCount) {
            newEntries[entryCount].key = key;
            newEntries[entryCount].value = value;
            entryCount++;
        }

        line = nextLine;
    }

    /* Now we need to make a persistent copy of the string data.
       The temporary buffer (buf) contains all the strings, but we need
       to keep them alive. We'll copy the entire buffer and remap pointers. */
    size_t poolSize = bytesRead + 1;
    char* pool = (char*)malloc(poolSize);
    if (pool == NULL) {
        free(newEntries);
        free(buf);
        return false;
    }
    memcpy(pool, buf, bytesRead + 1);

    /* Remap pointers from buf-relative to pool-relative */
    ptrdiff_t offset = pool - buf;
    for (int i = 0; i < entryCount; i++) {
        newEntries[i].key += offset;
        newEntries[i].value += offset;
    }

    /* Sort entries by key for binary search */
    qsort(newEntries, (size_t)entryCount, sizeof(LangEntry), LangEntryCompare);

    /* Commit */
    LangCleanup(); /* Free any previous data */
    g_lang.entries = newEntries;
    g_lang.count = entryCount;
    g_lang.capacity = lineCount;
    g_lang.stringPool = pool;
    g_lang.stringPoolSz = poolSize;
    g_lang.initialized = true;

    /* Store the language identifier (filename without extension) */
    if (langIdent) {
        strncpy_s(g_lang.langFile, langIdent, _TRUNCATE);
    }

    /* Free the temporary parse buffer */
    free(buf);

    return true;
}

/* ---------------------------------------------------------------------------
 * Internal: get the KailleraLang folder path (next to the DLL)
 * --------------------------------------------------------------------------- */
extern HINSTANCE hx; /* DLL instance, from kailleraclient.cpp */

static bool GetLangFolderPath(char* outPath, size_t outSize) {
    char dllPath[MAX_PATH];
    GetModuleFileNameA(hx, dllPath, MAX_PATH);

    /* Strip filename to get directory */
    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = 0;
    } else {
        dllPath[0] = 0;
    }

    /* Append KailleraLang\\ */
    if (strlen(dllPath) + strlen("KailleraLang\\") + 1 >= outSize) {
        return false;
    }
    strcpy_s(outPath, outSize, dllPath);
    strcat_s(outPath, outSize, "KailleraLang\\");

    return true;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void LangInit() {
    /* Read the preferred language from settings */
    nSettings::Initialize("okai");
    char langFile[64];
    nSettings::get_str("Lang", langFile, "English");
    nSettings::Terminate();

    /* Build the path to the .lng file */
    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return;
    }

    /* Ensure the KailleraLang directory exists */
    CreateDirectoryA(langPath, NULL);

    size_t basePathLen = strlen(langPath);
    strcat_s(langPath, sizeof(langPath), langFile);
    strcat_s(langPath, sizeof(langPath), ".lng");

    /* Try to load the requested language */
    if (!LangLoadFile(langPath, langFile)) {
        /* Fall back to English */
        langPath[basePathLen] = 0;
        strcat_s(langPath, sizeof(langPath), "English.lng");
        if (!LangLoadFile(langPath, "English")) {
            /* Last resort: continue with empty strings; LNG() will return keys */
        }
    }
}

void LangShutdown() {
    LangCleanup();
}

const char* LangGet(const char* key) {
    if (key == NULL) return "";

    if (g_lang.initialized && g_lang.count > 0) {
        /* Binary search for the key in the loaded .lng file */
        LangEntry needle;
        needle.key = key;
        needle.value = NULL;

        LangEntry* found = (LangEntry*)bsearch(
            &needle,
            g_lang.entries,
            (size_t)g_lang.count,
            sizeof(LangEntry),
            LangEntryCompare
        );

        if (found) {
            return found->value;
        }

        /* Linear search fallback: if bsearch fails (e.g. due to sort
           inconsistency or pointer remapping issues), try a slow linear
           scan. This ensures keys present in the .lng file are always
           found even if the binary search has a subtle bug. */
        for (int i = 0; i < g_lang.count; i++) {
            if (strcmp(g_lang.entries[i].key, key) == 0) {
                return g_lang.entries[i].value;
            }
        }
    }

    /* Fallback: search built-in English defaults */
    const char* defVal = LangGetDefault(key);
    if (defVal != NULL) {
        return defVal;
    }

    /* Last resort: return the key itself */
    return key;
}

const char* LangGetName() {
    if (g_lang.langName[0] != 0) {
        return g_lang.langName;
    }
    return "English";
}

const char* LangGetFile() {
    if (g_lang.langFile[0] != 0) {
        return g_lang.langFile;
    }
    return "English";
}

int LangGetCount() {
    return g_lang.count;
}

int LangEnumerate(LangEnumCallback callback, void* userdata) {
    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return 0;
    }

    int count = 0;
    char searchPath[MAX_PATH];
    strcpy_s(searchPath, sizeof(searchPath), langPath);
    strcat_s(searchPath, sizeof(searchPath), "*.lng");

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            /* Strip .lng extension */
            char name[MAX_PATH];
            strcpy_s(name, sizeof(name), findData.cFileName);
            char* dot = strrchr(name, '.');
            if (dot && _stricmp(dot, ".lng") == 0) {
                *dot = 0;
            }
            if (callback) {
                callback(name, userdata);
            }
            count++;
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return count;
}

const char* LangGetDisplayName(const char* fileName) {
    static char displayName[128];

    if (fileName == NULL) return "";

    /* If this is the currently loaded language, use the cached name */
    if (g_lang.langFile[0] != 0 && _stricmp(fileName, g_lang.langFile) == 0) {
        if (g_lang.langName[0] != 0) {
            strncpy_s(displayName, sizeof(displayName), g_lang.langName, _TRUNCATE);
            return displayName;
        }
    }

    /* Otherwise, quickly scan the .lng file for LANG_NAME= line */
    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return fileName;
    }
    strcat_s(langPath, sizeof(langPath), fileName);
    strcat_s(langPath, sizeof(langPath), ".lng");

    HANDLE hFile = CreateFileA(langPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return fileName;
    }

    DWORD bytesRead;
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize > 1024 * 64) {
        CloseHandle(hFile);
        return fileName;
    }

    char* buf = (char*)malloc(fileSize + 1);
    if (!buf) {
        CloseHandle(hFile);
        return fileName;
    }

    if (!ReadFile(hFile, buf, fileSize, &bytesRead, NULL)) {
        free(buf);
        CloseHandle(hFile);
        return fileName;
    }
    CloseHandle(hFile);
    buf[bytesRead] = 0;

    /* Skip UTF-8 BOM if present */
    char* data = buf;
    if (bytesRead >= 3 && (unsigned char)data[0] == 0xEF &&
        (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) {
        data += 3;
    }

    /* Search for LANG_NAME= line */
    char* line = data;
    while (line && *line) {
        /* Skip whitespace */
        while (*line == ' ' || *line == '\t') line++;
        /* Skip comments and empty lines */
        if (*line == ';' || *line == '#' || *line == '\r' || *line == '\n') {
            line = strchr(line, '\n');
            if (line) line++;
            continue;
        }
        /* Check if this line starts with LANG_NAME= */
        if (strncmp(line, "LANG_NAME=", 10) == 0) {
            char* value = line + 10;
            /* Trim trailing whitespace/CR/LF */
            char* end = value;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t len = end - value;
            if (len >= sizeof(displayName)) len = sizeof(displayName) - 1;
            memcpy(displayName, value, len);
            displayName[len] = 0;
            /* Trim trailing spaces */
            while (len > 0 && (displayName[len-1] == ' ' || displayName[len-1] == '\t')) {
                displayName[--len] = 0;
            }
            /* Convert UTF-8 to ANSI for Win32 display */
            int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, displayName, (int)len, NULL, 0);
            if (wlen > 0) {
                wchar_t* wbuf = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
                if (wbuf) {
                    MultiByteToWideChar(CP_UTF8, 0, displayName, (int)len, wbuf, wlen);
                    wbuf[wlen] = 0;
                    int alen = WideCharToMultiByte(CP_ACP, 0, wbuf, wlen, NULL, 0, NULL, NULL);
                    if (alen > 0 && alen < (int)sizeof(displayName)) {
                        char* abuf = (char*)malloc(alen + 1);
                        if (abuf) {
                            WideCharToMultiByte(CP_ACP, 0, wbuf, wlen, abuf, alen, NULL, NULL);
                            abuf[alen] = 0;
                            strncpy_s(displayName, sizeof(displayName), abuf, _TRUNCATE);
                            free(abuf);
                        }
                    }
                    free(wbuf);
                }
            }
            free(buf);
            return displayName;
        }
        line = strchr(line, '\n');
        if (line) line++;
    }

    free(buf);
    /* LANG_NAME not found, return the file name as fallback */
    strncpy_s(displayName, sizeof(displayName), fileName, _TRUNCATE);
    return displayName;
}

bool LangSetLanguage(const char* langName) {
    if (langName == NULL) return false;

    char langPath[MAX_PATH];
    if (!GetLangFolderPath(langPath, sizeof(langPath))) {
        return false;
    }

    size_t basePathLen = strlen(langPath);
    strcat_s(langPath, sizeof(langPath), langName);
    strcat_s(langPath, sizeof(langPath), ".lng");

    if (LangLoadFile(langPath, langName)) {
        /* Save the preference */
        nSettings::Initialize("okai");
        nSettings::set_str("Lang", (char*)langName);
        nSettings::Terminate();
        return true;
    }

    return false;
}

/* ---------------------------------------------------------------------------
 * LangNotifyChanged - tell all windows the language has changed
 * --------------------------------------------------------------------------- */
static BOOL CALLBACK SendLangChangedEnum(HWND hwnd, LPARAM lParam) {
    (void)lParam;
    SendMessage(hwnd, WM_LANG_CHANGED, 0, 0);
    /* Also enumerate child windows (ListView headers, etc.) */
    EnumChildWindows(hwnd, SendLangChangedEnum, 0);
    return TRUE;
}

void LangNotifyChanged() {
    /* Re-initialize shared status strings */
    InitStatusStrings();

    /* Send WM_LANG_CHANGED to all windows on the current thread */
    EnumThreadWindows(GetCurrentThreadId(), SendLangChangedEnum, 0);
}
