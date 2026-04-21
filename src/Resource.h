#pragma once

// Version
#define ULTRANOTE_VERSION       L"v1.2.0"

// Icons
#define IDI_APP                 101
#define IDI_TRAY                102
#define IDI_NOTELIST            103
#define IDI_PASTE               104
#define IDI_SETTINGS            105
#define IDI_NEW                 106
#define IDI_SEARCH              107
#define IDI_ABOUT               108
#define IDI_PIN                 109
#define IDI_SHOW_ALL            110
#define IDI_COPY                111
#define IDI_EXIT                112
#define IDI_DELETE              113
#define IDI_HIDE_ALL            114
#define IDI_GROUP               115
#define IDI_FOLDER              116
#define IDI_ATTACHMENT          117
#define IDI_ALARM               118

// Tray context menu
#define IDR_TRAY_MENU           200
#define ID_TRAY_NEWNOTE         201
#define ID_TRAY_SHOWNOTES       202
#define ID_TRAY_HIDENOTES       203
#define ID_TRAY_NOTELIST        204
#define ID_TRAY_PASTENOTE       205
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
#define ID_NOTE_ATTACHMENTS     218
#define ID_NOTE_SEARCH          219
#define ID_NOTE_ALARM           229

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
#define ID_NL_NOTE_ALARM        230

// Preview toggle
#define ID_NL_PREVIEW           310

// Tray menu: Settings dialog + About
#define ID_TRAY_SETTINGS        311
#define ID_TRAY_ABOUT           313

// Note list: Settings dialog + About + Search
#define ID_NL_SETTINGS          312
#define ID_NL_ABOUT             314
#define ID_NL_SEARCH            315

// Tray menu: Search
#define ID_TRAY_SEARCH          206

// Settings dialog control IDs
#define IDC_SETTINGS_TAB        500
#define IDC_SETTINGS_OK         501
#define IDC_SETTINGS_CANCEL     502
#define IDC_SETTINGS_APPLY      503

// Tab 1: Layout
#define IDC_BG_COLOR_SWATCH     510
#define IDC_BG_COLOR_BTN        511
#define IDC_TEXT_COLOR_SWATCH   512
#define IDC_TEXT_COLOR_BTN      513
#define IDC_BORDER_COLOR_SWATCH 514
#define IDC_BORDER_COLOR_BTN    515
#define IDC_FONT_DISPLAY        516
#define IDC_FONT_BTN            517
#define IDC_LAYOUT_PREVIEW      518

// Tab 2: Keyboard
#define IDC_SHORTCUT_LIST       520
#define IDC_SHORTCUT_HOTKEY     521
#define IDC_SHORTCUT_CHANGE     522
#define IDC_SHORTCUT_DEFAULT    523

// Tab 3: General
#define IDC_AUTOSAVE_SPIN       530
#define IDC_AUTOSAVE_EDIT       531
#define IDC_CONFIRM_DELETE      532
#define IDC_PREVIEW_ENABLED     533
#define IDC_PREVIEW_DELAY_SPIN  534
#define IDC_PREVIEW_DELAY_EDIT  535
#define IDC_TRAY_DBLCLICK       536
#define IDC_LANGUAGE            537
#define IDC_CLICKABLE_LINKS     538

// Global hotkey IDs (for RegisterHotKey)
#define IDH_GLOBAL_NEWNOTE      1
#define IDH_GLOBAL_NOTELIST     2

// Tab 4: Misc
#define IDC_NEWNOTE_X_EDIT      540
#define IDC_NEWNOTE_X_SPIN      541
#define IDC_NEWNOTE_Y_EDIT      542
#define IDC_NEWNOTE_Y_SPIN      543
#define IDC_CASCADE_STEP_EDIT   544
#define IDC_CASCADE_STEP_SPIN   545
#define IDC_CASCADE_RESET_EDIT  546
#define IDC_CASCADE_RESET_SPIN  547
#define IDC_DEFAULT_FOLDER      548
#define IDC_SEARCH_EDIT         549

// Tab 4: Initial text
#define IDC_INITIAL_TEXT_EDIT   550
#define IDC_INITIAL_TEXT_INSERT 551

// Initial text insert menu (variable picker)
#define ID_INITTEXT_BASE        560
#define ID_INITTEXT_MAX         589

// Tab 5: Note List settings
#define IDC_DATE_FORMAT         600
#define IDC_ZEBRA_STRIPING      601

// Tab 1: Layout (extended)
#define IDC_SEARCH_HL_SWATCH    602
#define IDC_SEARCH_HL_BTN       603

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
#define IDT_ALARM               3
#define ALARM_CHECK_INTERVAL_MS 30000
#define IDT_HEADER_DRAG         4
#define HEADER_DRAG_POLL_MS     25

// Alarm popup window control IDs (700-720 for popup; 720-770 for config dialog)
#define IDC_ALARM_DISMISS       701
#define IDC_ALARM_SNOOZE        702
#define IDC_ALARM_OPEN          703

// UpDown spinner controls attached to numeric edits in the config dialog
#define IDC_ALARM_EVERY_N_SPIN      704
#define IDC_ALARM_MONTHLY_DAY_SPIN  705
#define IDC_ALARM_MONTHLY_NTH_SPIN  706
#define IDC_ALARM_QUARTER_DAY_SPIN  707
#define IDC_ALARM_END_COUNT_SPIN    708
#define IDC_ALARM_SNOOZE_SPIN       709

// Alarm config dialog control IDs
#define IDC_ALARM_START_DATE        720
#define IDC_ALARM_START_TIME        721
#define IDC_ALARM_KIND_ONCE         722
#define IDC_ALARM_KIND_DAILY        723
#define IDC_ALARM_KIND_EVERY_N      724
#define IDC_ALARM_EVERY_N_EDIT      725
#define IDC_ALARM_KIND_WEEKLY       726
#define IDC_ALARM_WD_MON            727
#define IDC_ALARM_WD_TUE            728
#define IDC_ALARM_WD_WED            729
#define IDC_ALARM_WD_THU            730
#define IDC_ALARM_WD_FRI            731
#define IDC_ALARM_WD_SAT            732
#define IDC_ALARM_WD_SUN            733
#define IDC_ALARM_KIND_MONTHLY      734
#define IDC_ALARM_MONTHLY_DAY_RB    735
#define IDC_ALARM_MONTHLY_DAY_EDIT  736
#define IDC_ALARM_MONTHLY_NTH_RB    737
#define IDC_ALARM_MONTHLY_NTH_EDIT  738
#define IDC_ALARM_MONTHLY_NTH_COMBO 739
#define IDC_ALARM_KIND_QUARTERLY    740
#define IDC_ALARM_QUARTER_DAY_EDIT  741
#define IDC_ALARM_KIND_YEARLY       742
#define IDC_ALARM_YEARLY_DATE       743
#define IDC_ALARM_END_NEVER         744
#define IDC_ALARM_END_AFTER_N       745
#define IDC_ALARM_END_COUNT_EDIT    746
#define IDC_ALARM_END_ON_DATE       747
#define IDC_ALARM_END_DATE          748
#define IDC_ALARM_POPUP_CHK         749
#define IDC_ALARM_SOUND_CHK         750
#define IDC_ALARM_SOUNDFILE_EDIT    752
#define IDC_ALARM_SOUNDFILE_BTN     753
#define IDC_ALARM_SNOOZE_EDIT       754
#define IDC_ALARM_PAUSED_CHK        755
#define IDC_ALARM_PREVIEW           756
#define IDC_ALARM_OK                757
#define IDC_ALARM_CANCEL            758
#define IDC_ALARM_REMOVE            759

// Header context menu (column visibility toggle) — reserved range 320..329 (one per Column enum value)
#define ID_NL_COLVIS_BASE           320
#define ID_NL_COLVIS_MAX            329
