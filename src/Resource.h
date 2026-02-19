#pragma once

// Icons
#define IDI_APP                 101
#define IDI_TRAY                102

// Tray context menu
#define IDR_TRAY_MENU           200
#define ID_TRAY_NEWNOTE         201
#define ID_TRAY_SHOWNOTES       202
#define ID_TRAY_HIDENOTES       203
#define ID_TRAY_NOTELIST        204
#define ID_TRAY_EXIT            209

// Language selection (dynamic range: ID_LANG_BASE + index)
#define ID_LANG_BASE            230
#define ID_LANG_MAX             249

// Note context menu
#define IDR_NOTE_MENU           210
#define ID_NOTE_EDIT            211
#define ID_NOTE_DELETE          212
#define ID_NOTE_ALWAYSONTOP     213
#define ID_NOTE_NEWNOTE         214
#define ID_NOTE_COPY            215
#define ID_NOTE_HIDE            216
#define ID_NOTE_RENAME          217

// Note list menu bar
#define IDR_NOTELIST_MENU       220
#define ID_NL_FILE_CLOSE        221
#define ID_NL_NOTE_NEW          222
#define ID_NL_NOTE_EDIT         223
#define ID_NL_NOTE_DELETE       224
#define ID_NL_SHOW_ALL          225
#define ID_NL_HIDE_ALL          226
#define ID_NL_NOTE_RENAME       227
#define ID_NL_NOTE_SETFOLDER    228

// Preview toggle
#define ID_NL_PREVIEW           310

// Folder management
#define ID_NL_FOLDER_NEW        240
#define ID_NL_FOLDER_RENAME     241
#define ID_NL_FOLDER_DELETE     242
#define ID_NL_FOLDER_BASE       250
#define ID_NL_FOLDER_MAX        299

// String table
#define IDS_APP_NAME            400
#define IDS_CONFIRM_DELETE      401
#define IDS_CONFIRM_DELETE_MULTI 402
#define IDS_TRAY_TOOLTIP        403

// Custom window messages
#define WM_TRAY_CALLBACK        (WM_USER + 1)
#define WM_NOTE_CHANGED         (WM_USER + 2)
#define WM_NOTE_REQUEST_DELETE  (WM_USER + 3)

// Timer IDs
#define IDT_AUTOSAVE            1
#define AUTOSAVE_INTERVAL_MS    30000
#define IDT_PREVIEW             2
#define PREVIEW_DELAY_MS        400
