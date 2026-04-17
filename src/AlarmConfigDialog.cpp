#include "AlarmConfigDialog.h"
#include "Application.h"
#include "AlarmScheduler.h"
#include "Localization.h"
#include "Resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <cwchar>

bool AlarmConfigDialog::s_classRegistered = false;

static constexpr wchar_t kClassName[] = L"UltraNoteAlarmConfig";

// Client layout constants (pixels)
static constexpr int kClientW      = 440;
static constexpr int kClientH      = 615;
static constexpr int kMargin       = 12;
static constexpr int kRowH         = 22;
static constexpr int kRowGap       = 6;
static constexpr int kSectionGap   = 12;
static constexpr int kSubIndent    = 20;
static constexpr int kSubSubIndent = kSubIndent + 16;  // 36

// Two shared X columns for inline numeric edits — one per indent level — so
// that all number boxes line up vertically within each level.
static constexpr int kEditColLvl1  = kMargin + kSubIndent + 120;    // x = 152
static constexpr int kEditColLvl2  = kMargin + kSubSubIndent + 62;  // x = 110

// Uniform dimensions for numeric edits with attached UpDown spinner.
// 44 px total: ~29 px for the number (ES_RIGHT), ~15 px for the spinner arrows.
static constexpr int kNumEditW     = 44;
static constexpr int kNumEditH     = kRowH - 2;
static constexpr int kNumEditYOff  = 1;
// Edit (+spinner) uses the same Y-offset as adjacent radios/labels so the
// box centers align vertically — essential for visual symmetry with the
// radio-button circles (kRowH=22 standalone radio center sits at y+11;
// kNumEditH=20 edit at y+1 also centers at y+11).
static constexpr int kEditYOff     = kNumEditYOff;
// Gap between numeric edit (right edge) and following post-label.
static constexpr int kPostLabelGap = 4;

// Display order: Mon..Sun (European). Maps to weekdayMask bit positions:
// bit 0 = Sunday, bit 1 = Monday, ..., bit 6 = Saturday (SYSTEMTIME::wDayOfWeek).
static constexpr int kDisplayToMaskBit[7] = { 1, 2, 3, 4, 5, 6, 0 };
// and the short-name keys in display order:
static const wchar_t* const kWdKeys[7] = {
    L"alarm.wd.mon", L"alarm.wd.tue", L"alarm.wd.wed", L"alarm.wd.thu",
    L"alarm.wd.fri", L"alarm.wd.sat", L"alarm.wd.sun"
};

AlarmConfigDialog::AlarmConfigDialog(HINSTANCE hInst, HWND owner, uint64_t noteId)
    : m_hInst(hInst), m_hOwner(owner), m_noteId(noteId) {
}

AlarmConfigDialog::~AlarmConfigDialog() {
    if (m_hFont) DeleteObject(m_hFont);
}

bool AlarmConfigDialog::EnsureClassRegistered(HINSTANCE hInst) {
    if (s_classRegistered) return true;
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCE(IDI_ALARM));
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) return false;
    s_classRegistered = true;
    return true;
}

bool AlarmConfigDialog::Create() {
    if (!EnsureClassRegistered(m_hInst)) return false;

    RECT r = { 0, 0, kClientW, kClientH };
    DWORD style   = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    AdjustWindowRectEx(&r, style, FALSE, exStyle);

    // Center over owner (or screen)
    int totalW = r.right - r.left;
    int totalH = r.bottom - r.top;
    int x = 100, y = 100;
    if (m_hOwner) {
        RECT or2;
        GetWindowRect(m_hOwner, &or2);
        x = or2.left + ((or2.right - or2.left) - totalW) / 2;
        y = or2.top  + ((or2.bottom - or2.top) - totalH) / 2;
    }
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (x < wa.left) x = wa.left;
    if (y < wa.top)  y = wa.top;
    if (x + totalW > wa.right)  x = wa.right - totalW;
    if (y + totalH > wa.bottom) y = wa.bottom - totalH;

    std::wstring title = Ls(L"alarm.cfg.title");
    m_hwnd = CreateWindowExW(exStyle, kClassName, title.c_str(),
                             style, x, y, totalW, totalH,
                             m_hOwner, nullptr, m_hInst, this);
    if (!m_hwnd) return false;

    if (m_hOwner) EnableWindow(m_hOwner, FALSE);
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    return true;
}

LRESULT CALLBACK AlarmConfigDialog::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AlarmConfigDialog* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<AlarmConfigDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<AlarmConfigDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT AlarmConfigDialog::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            CreateControls();
            LoadFromNote();
            UpdateControlStates();
            UpdatePreview();
            return 0;

        case WM_COMMAND: {
            UINT cmd = LOWORD(wp);
            UINT code = HIWORD(wp);

            if (cmd == IDC_ALARM_OK)     { OnOk();     return 0; }
            if (cmd == IDC_ALARM_CANCEL) { DestroyWindow(m_hwnd); return 0; }
            if (cmd == IDC_ALARM_REMOVE) { OnRemove(); return 0; }
            if (cmd == IDC_ALARM_SOUNDFILE_BTN) { BrowseSoundFile(); return 0; }

            // Any radio/checkbox change may affect enabled state and preview
            if (code == BN_CLICKED) {
                UpdateControlStates();
                UpdatePreview();
                return 0;
            }
            if (code == EN_CHANGE || code == CBN_SELCHANGE) {
                UpdatePreview();
                return 0;
            }
            break;
        }

        case WM_NOTIFY: {
            auto nm = reinterpret_cast<NMHDR*>(lp);
            if (nm->code == DTN_DATETIMECHANGE) {
                UpdatePreview();
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(m_hwnd);
            return 0;

        case WM_DESTROY:
            if (m_hOwner) EnableWindow(m_hOwner, TRUE);
            return 0;

        case WM_NCDESTROY: {
            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
            m_hwnd = nullptr;
            return 0;
        }
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

static HWND MakeStatic(HWND parent, HINSTANCE hInst, HFONT font,
                       const wchar_t* text, int x, int y, int w, int h,
                       DWORD extraStyle = 0) {
    HWND h_ = CreateWindowExW(0, L"STATIC", text,
                              WS_CHILD | WS_VISIBLE | SS_LEFT | extraStyle,
                              x, y, w, h, parent, nullptr, hInst, nullptr);
    SendMessageW(h_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return h_;
}

static HWND MakeRadio(HWND parent, HINSTANCE hInst, HFONT font, int id,
                      const wchar_t* text, int x, int y, int w, int h,
                      bool groupStart) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
    if (groupStart) style |= WS_GROUP;
    HWND h_ = CreateWindowExW(0, L"BUTTON", text, style,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              hInst, nullptr);
    SendMessageW(h_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return h_;
}

static HWND MakeCheck(HWND parent, HINSTANCE hInst, HFONT font, int id,
                      const wchar_t* text, int x, int y, int w, int h) {
    HWND h_ = CreateWindowExW(0, L"BUTTON", text,
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              hInst, nullptr);
    SendMessageW(h_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return h_;
}

static HWND MakeEdit(HWND parent, HINSTANCE hInst, HFONT font, int id,
                     int x, int y, int w, int h, bool numeric) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER;
    if (numeric) style |= ES_NUMBER | ES_CENTER;
    else         style |= ES_LEFT;
    HWND h_ = CreateWindowExW(0, L"EDIT", L"", style,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              hInst, nullptr);
    SendMessageW(h_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return h_;
}

// Creates a numeric EDIT with an attached UpDown spinner (docked into the
// right side of the edit via UDS_ALIGNRIGHT). Returns the EDIT handle.
static HWND MakeNumEditWithSpin(HWND parent, HINSTANCE hInst, HFONT font,
                                int editId, int spinId, int x, int y, int w, int h,
                                int minVal, int maxVal) {
    HWND hEdit = MakeEdit(parent, hInst, font, editId, x, y, w, h, true);
    HWND hSpin = CreateWindowExW(0, UPDOWN_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT |
        UDS_ARROWKEYS | UDS_NOTHOUSANDS,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(spinId)),
        hInst, nullptr);
    SendMessageW(hSpin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(hEdit), 0);
    SendMessageW(hSpin, UDM_SETRANGE32, minVal, maxVal);
    return hEdit;
}

static void AddSeparator(HWND parent, HINSTANCE hInst, int x, int y, int w) {
    CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        x, y, w, 1, parent, nullptr, hInst, nullptr);
}

static HWND MakeButton(HWND parent, HINSTANCE hInst, HFONT font, int id,
                       const wchar_t* text, int x, int y, int w, int h,
                       bool defaultBtn = false) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                  (defaultBtn ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON);
    HWND h_ = CreateWindowExW(0, L"BUTTON", text, style,
                              x, y, w, h, parent,
                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              hInst, nullptr);
    SendMessageW(h_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return h_;
}

void AlarmConfigDialog::CreateControls() {
    NONCLIENTMETRICS ncm = { sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    m_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

    const int contentW = kClientW - 2 * kMargin;
    const int lvl1X = kMargin + kSubIndent;     // 32
    const int lvl2X = kMargin + kSubSubIndent;  // 48
    int y = kMargin;

    // ─── Section: Start time ───
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.start").c_str(),
                                  kMargin, y, contentW, kRowH));
    y += kRowH;
    m_hStartDate = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATEFORMAT,
        kMargin, y, 130, kRowH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_START_DATE)),
        m_hInst, nullptr);
    SendMessageW(m_hStartDate, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hStartTime = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_TIMEFORMAT,
        kMargin + 140, y, 90, kRowH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_START_TIME)),
        m_hInst, nullptr);
    SendMessageW(m_hStartTime, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    y += kRowH + kSectionGap;

    // ─── Section: Repetition ───
    AddSeparator(m_hwnd, m_hInst, kMargin, y - kSectionGap / 2, contentW);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.repeat").c_str(),
                                  kMargin, y, contentW, kRowH));
    y += kRowH;

    // Kind radio group 1 starts here (WS_GROUP)
    m_hRbOnce = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_ONCE,
                          Ls(L"alarm.cfg.once").c_str(),
                          lvl1X, y, contentW - kSubIndent, kRowH, true);
    y += kRowH;

    m_hRbDaily = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_DAILY,
                           Ls(L"alarm.cfg.daily").c_str(),
                           lvl1X, y, contentW - kSubIndent, kRowH, false);
    y += kRowH;

    // "Alle [n] Tage" — edit on Level-2 column (short pre-label)
    m_hRbEveryN = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_EVERY_N,
                            Ls(L"alarm.cfg.every_n_pre").c_str(),
                            lvl1X, y, kEditColLvl2 - lvl1X - 4, kRowH, false);
    m_hEditEveryN = MakeNumEditWithSpin(m_hwnd, m_hInst, m_hFont,
                                        IDC_ALARM_EVERY_N_EDIT, IDC_ALARM_EVERY_N_SPIN,
                                        kEditColLvl2, y + kEditYOff,
                                        kNumEditW, kNumEditH, 1, 365);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.every_n_post").c_str(),
                                  kEditColLvl2 + kNumEditW + kPostLabelGap,
                                  y + kNumEditYOff, 120, kNumEditH));
    y += kRowH;

    m_hRbWeekly = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_WEEKLY,
                            Ls(L"alarm.cfg.weekly").c_str(),
                            lvl1X, y, contentW - kSubIndent, kRowH, false);
    y += kRowH;

    // Weekday checkboxes (Mon..Sun), indented to Level-2
    {
        int wdW = 46;
        for (int i = 0; i < 7; ++i) {
            m_hChkWd[i] = MakeCheck(m_hwnd, m_hInst, m_hFont, IDC_ALARM_WD_MON + i,
                                    Ls(kWdKeys[i]).c_str(),
                                    lvl2X + i * wdW, y, wdW, kRowH);
        }
    }
    y += kRowH + kRowGap;

    m_hRbMonthly = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_MONTHLY,
                             Ls(L"alarm.cfg.monthly").c_str(),
                             lvl1X, y, contentW - kSubIndent, kRowH, false);
    y += kRowH;

    // Sub-option: "am [n] . des Monats" — edit on Level-2 column
    m_hRbMonthlyDay = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_MONTHLY_DAY_RB,
                                Ls(L"alarm.cfg.monthly_day_pre").c_str(),
                                lvl2X, y, kEditColLvl2 - lvl2X - 4, kRowH, true);
    m_hEditMonthlyDay = MakeNumEditWithSpin(m_hwnd, m_hInst, m_hFont,
                                            IDC_ALARM_MONTHLY_DAY_EDIT,
                                            IDC_ALARM_MONTHLY_DAY_SPIN,
                                            kEditColLvl2, y + kEditYOff,
                                            kNumEditW, kNumEditH, 1, 31);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.monthly_day_post").c_str(),
                                  kEditColLvl2 + kNumEditW + kPostLabelGap,
                                  y + kNumEditYOff, 200, kNumEditH));
    y += kRowH;

    // Sub-option: "jeden [n] [Weekday]" — edit + combobox on Level-2 column.
    // Combobox h-param (kNumEditH * 8) only sizes the dropdown list.
    m_hRbMonthlyNth = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_MONTHLY_NTH_RB,
                                Ls(L"alarm.cfg.monthly_nth_pre").c_str(),
                                lvl2X, y, kEditColLvl2 - lvl2X - 4, kRowH, false);
    m_hEditMonthlyNth = MakeNumEditWithSpin(m_hwnd, m_hInst, m_hFont,
                                            IDC_ALARM_MONTHLY_NTH_EDIT,
                                            IDC_ALARM_MONTHLY_NTH_SPIN,
                                            kEditColLvl2, y + kEditYOff,
                                            kNumEditW, kNumEditH, 1, 5);
    m_hComboMonthlyWd = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        kEditColLvl2 + kNumEditW + kPostLabelGap, y + kNumEditYOff,
        110, kNumEditH * 8,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_MONTHLY_NTH_COMBO)),
        m_hInst, nullptr);
    SendMessageW(m_hComboMonthlyWd, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    for (int i = 0; i < 7; ++i) {
        SendMessageW(m_hComboMonthlyWd, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(Ls(kWdKeys[i]).c_str()));
    }
    SendMessageW(m_hComboMonthlyWd, CB_SETCURSEL, 0, 0);
    y += kRowH + kRowGap;

    // "Quartalsweise am [n] . Tag des Quartals" — Level-1 column
    m_hRbQuarterly = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_QUARTERLY,
                               Ls(L"alarm.cfg.quarterly_pre").c_str(),
                               lvl1X, y, kEditColLvl1 - lvl1X - 4, kRowH, false);
    m_hEditQuarterDay = MakeNumEditWithSpin(m_hwnd, m_hInst, m_hFont,
                                            IDC_ALARM_QUARTER_DAY_EDIT,
                                            IDC_ALARM_QUARTER_DAY_SPIN,
                                            kEditColLvl1, y + kEditYOff,
                                            kNumEditW, kNumEditH, 1, 90);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.quarterly_post").c_str(),
                                  kEditColLvl1 + kNumEditW + kPostLabelGap,
                                  y + kNumEditYOff, 220, kNumEditH));
    y += kRowH;

    m_hRbYearly = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_KIND_YEARLY,
                            Ls(L"alarm.cfg.yearly").c_str(),
                            lvl1X, y, contentW - kSubIndent, kRowH, false);
    y += kRowH + kSectionGap;

    // ─── Section: End ───
    AddSeparator(m_hwnd, m_hInst, kMargin, y - kSectionGap / 2, contentW);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.end").c_str(),
                                  kMargin, y, contentW, kRowH));
    y += kRowH;

    m_hRbEndNever = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_END_NEVER,
                              Ls(L"alarm.cfg.end_never").c_str(),
                              lvl1X, y, contentW - kSubIndent, kRowH, true);
    y += kRowH;

    // "Nach [n] Wiederholungen" — edit on Level-2 column (shorter pre-label)
    m_hRbEndAfterN = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_END_AFTER_N,
                               Ls(L"alarm.cfg.end_after_pre").c_str(),
                               lvl1X, y, kEditColLvl2 - lvl1X - 4, kRowH, false);
    m_hEditEndCount = MakeNumEditWithSpin(m_hwnd, m_hInst, m_hFont,
                                          IDC_ALARM_END_COUNT_EDIT,
                                          IDC_ALARM_END_COUNT_SPIN,
                                          kEditColLvl2, y + kEditYOff,
                                          kNumEditW, kNumEditH, 1, 999);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.end_after_post").c_str(),
                                  kEditColLvl2 + kNumEditW + kPostLabelGap,
                                  y + kNumEditYOff, 180, kNumEditH));
    y += kRowH;

    // "Am [Datum]" — date picker on Level-2 column (aligned with Nach-spinner)
    m_hRbEndOnDate = MakeRadio(m_hwnd, m_hInst, m_hFont, IDC_ALARM_END_ON_DATE,
                               Ls(L"alarm.cfg.end_on_date").c_str(),
                               lvl1X, y, kEditColLvl2 - lvl1X - 4, kRowH, false);
    m_hEndDate = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATEFORMAT,
        kEditColLvl2, y, 130, kRowH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_END_DATE)),
        m_hInst, nullptr);
    SendMessageW(m_hEndDate, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    y += kRowH + kSectionGap;

    // ─── Section: Notification ───
    AddSeparator(m_hwnd, m_hInst, kMargin, y - kSectionGap / 2, contentW);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.notify").c_str(),
                                  kMargin, y, contentW, kRowH));
    y += kRowH;

    m_hChkPopup = MakeCheck(m_hwnd, m_hInst, m_hFont, IDC_ALARM_POPUP_CHK,
                            Ls(L"alarm.cfg.popup").c_str(),
                            lvl1X, y, 80, kRowH);
    m_hChkSound = MakeCheck(m_hwnd, m_hInst, m_hFont, IDC_ALARM_SOUND_CHK,
                            Ls(L"alarm.cfg.sound").c_str(),
                            lvl1X + 90, y, 70, kRowH);
    y += kRowH;

    // "Sound-Datei: [.................................] [...]"
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.sound_file").c_str(),
                                  lvl1X, y, 75, kRowH));
    {
        int editX   = lvl1X + 75;
        int browseW = 30;
        int browseX = kMargin + contentW - browseW;
        int editW   = browseX - 4 - editX;
        m_hEditSoundFile = MakeEdit(m_hwnd, m_hInst, m_hFont, IDC_ALARM_SOUNDFILE_EDIT,
                                    editX, y, editW, kRowH, false);
        m_hBtnBrowse = MakeButton(m_hwnd, m_hInst, m_hFont, IDC_ALARM_SOUNDFILE_BTN,
                                  L"...", browseX, y, browseW, kRowH);
    }
    y += kRowH + kRowGap;

    // "Schlummern: [n] Minuten    [ ] Pausiert" — edit on Level-1 column
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.snooze_pre").c_str(),
                                  lvl1X, y + kNumEditYOff,
                                  kEditColLvl1 - lvl1X - 4, kNumEditH));
    m_hEditSnooze = MakeNumEditWithSpin(m_hwnd, m_hInst, m_hFont,
                                        IDC_ALARM_SNOOZE_EDIT, IDC_ALARM_SNOOZE_SPIN,
                                        kEditColLvl1, y + kEditYOff - 1,
                                        kNumEditW, kNumEditH, 1, 1440);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.snooze_post").c_str(),
                                  kEditColLvl1 + kNumEditW + kPostLabelGap,
                                  y + kNumEditYOff, 80, kNumEditH));
    m_hChkPaused = MakeCheck(m_hwnd, m_hInst, m_hFont, IDC_ALARM_PAUSED_CHK,
                             Ls(L"alarm.cfg.paused").c_str(),
                             kEditColLvl1 + kNumEditW + kPostLabelGap + 90,
                             y, 100, kRowH);
    y += kRowH + kSectionGap;

    // ─── Section: Preview ───
    AddSeparator(m_hwnd, m_hInst, kMargin, y - kSectionGap / 2, contentW);
    m_labels.push_back(MakeStatic(m_hwnd, m_hInst, m_hFont,
                                  Ls(L"alarm.cfg.preview").c_str(),
                                  kMargin, y, 80, kRowH));
    m_hPreview = MakeStatic(m_hwnd, m_hInst, m_hFont, L"",
                            kMargin + 80, y, contentW - 80, kRowH);
    y += kRowH + kSectionGap;

    // Buttons
    int btnW = 90;
    int btnH = 26;
    int btnY = kClientH - kMargin - btnH;
    m_hBtnRemove = MakeButton(m_hwnd, m_hInst, m_hFont, IDC_ALARM_REMOVE,
                              Ls(L"alarm.cfg.remove").c_str(),
                              kMargin, btnY, btnW, btnH);
    m_hBtnCancel = MakeButton(m_hwnd, m_hInst, m_hFont, IDC_ALARM_CANCEL,
                              Ls(L"alarm.cfg.cancel").c_str(),
                              kClientW - kMargin - 2 * btnW - 6, btnY, btnW, btnH);
    m_hBtnOk = MakeButton(m_hwnd, m_hInst, m_hFont, IDC_ALARM_OK,
                          Ls(L"alarm.cfg.ok").c_str(),
                          kClientW - kMargin - btnW, btnY, btnW, btnH, true);
}

void AlarmConfigDialog::LoadFromNote() {
    NoteData* note = Application::Get().FindNoteData(m_noteId);
    if (!note) return;

    AlarmConfig a;
    bool hadAlarm = note->alarm.has_value();
    if (hadAlarm) {
        a = *note->alarm;
    } else {
        // New alarm defaults: now + 10 minutes, daily
        SYSTEMTIME now;
        GetLocalTime(&now);
        a.startTime = now;
        // Round to next 10 min for usability
        int minute = a.startTime.wMinute + 10;
        a.startTime.wMinute = static_cast<WORD>(minute % 60);
        if (minute >= 60) {
            a.startTime.wHour = static_cast<WORD>((a.startTime.wHour + 1) % 24);
        }
        a.startTime.wSecond = 0;
        a.endDate = now;
        a.endDate.wYear++;
        a.kind = AlarmKind::Once;
    }

    SendMessageW(m_hStartDate, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&a.startTime));
    SendMessageW(m_hStartTime, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&a.startTime));

    // Radio: kind
    int kindIds[] = {
        IDC_ALARM_KIND_ONCE, IDC_ALARM_KIND_DAILY, IDC_ALARM_KIND_EVERY_N,
        IDC_ALARM_KIND_WEEKLY, IDC_ALARM_KIND_MONTHLY, IDC_ALARM_KIND_MONTHLY,
        IDC_ALARM_KIND_QUARTERLY, IDC_ALARM_KIND_YEARLY
    };
    // AlarmKind::MonthlyDay and MonthlyNth both map to the Monthly radio button
    int kindIdx = static_cast<int>(a.kind);
    if (kindIdx < 0 || kindIdx > 7) kindIdx = 0;
    CheckRadioButton(m_hwnd, IDC_ALARM_KIND_ONCE, IDC_ALARM_KIND_YEARLY, kindIds[kindIdx]);

    // EveryN edit
    wchar_t buf[32];
    swprintf_s(buf, L"%d", (std::max)(1, a.intervalDays));
    SetWindowTextW(m_hEditEveryN, buf);

    // Weekly checkboxes
    for (int i = 0; i < 7; ++i) {
        int bit = kDisplayToMaskBit[i];
        SendMessageW(m_hChkWd[i], BM_SETCHECK,
                     (a.weekdayMask & (1u << bit)) ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    // Monthly sub-options
    CheckRadioButton(m_hwnd, IDC_ALARM_MONTHLY_DAY_RB, IDC_ALARM_MONTHLY_NTH_RB,
                     (a.kind == AlarmKind::MonthlyNth)
                         ? IDC_ALARM_MONTHLY_NTH_RB : IDC_ALARM_MONTHLY_DAY_RB);
    swprintf_s(buf, L"%d", (std::clamp)(a.monthDay, 1, 31));
    SetWindowTextW(m_hEditMonthlyDay, buf);
    swprintf_s(buf, L"%d", (std::clamp)(a.nthWeek, 1, 5));
    SetWindowTextW(m_hEditMonthlyNth, buf);
    // Combobox: nthWeekday is 0=Sun..6=Sat; display index Mon=0 .. Sun=6
    int cbIdx = 0;
    for (int i = 0; i < 7; ++i) {
        if (kDisplayToMaskBit[i] == a.nthWeekday) { cbIdx = i; break; }
    }
    SendMessageW(m_hComboMonthlyWd, CB_SETCURSEL, cbIdx, 0);

    // Quarterly
    swprintf_s(buf, L"%d", (std::clamp)(a.quarterDay, 1, 90));
    SetWindowTextW(m_hEditQuarterDay, buf);

    // End condition
    int endIds[] = { IDC_ALARM_END_NEVER, IDC_ALARM_END_AFTER_N, IDC_ALARM_END_ON_DATE };
    int endIdx = static_cast<int>(a.endKind);
    if (endIdx < 0 || endIdx > 2) endIdx = 0;
    CheckRadioButton(m_hwnd, IDC_ALARM_END_NEVER, IDC_ALARM_END_ON_DATE, endIds[endIdx]);
    swprintf_s(buf, L"%d", (std::max)(1, a.endCount));
    SetWindowTextW(m_hEditEndCount, buf);
    SYSTEMTIME endDt = a.endDate;
    if (endDt.wYear == 0) {
        GetLocalTime(&endDt);
        endDt.wYear++;
    }
    SendMessageW(m_hEndDate, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&endDt));

    // Notification
    SendMessageW(m_hChkPopup, BM_SETCHECK, a.popup ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hChkSound, BM_SETCHECK, a.sound ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_hEditSoundFile, a.soundFile.c_str());
    swprintf_s(buf, L"%d", (std::max)(1, a.snoozeMinutes));
    SetWindowTextW(m_hEditSnooze, buf);
    SendMessageW(m_hChkPaused, BM_SETCHECK, a.paused ? BST_CHECKED : BST_UNCHECKED, 0);

    // Show/hide Remove button based on whether an alarm already exists
    ShowWindow(m_hBtnRemove, hadAlarm ? SW_SHOW : SW_HIDE);
}

void AlarmConfigDialog::WriteToNote() {
    NoteData* note = Application::Get().FindNoteData(m_noteId);
    if (!note) return;

    AlarmConfig a = note->alarm.has_value() ? *note->alarm : AlarmConfig{};

    // Start time (combine date + time)
    SYSTEMTIME dt, tm;
    SendMessageW(m_hStartDate, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&dt));
    SendMessageW(m_hStartTime, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&tm));
    a.startTime.wYear   = dt.wYear;
    a.startTime.wMonth  = dt.wMonth;
    a.startTime.wDay    = dt.wDay;
    a.startTime.wHour   = tm.wHour;
    a.startTime.wMinute = tm.wMinute;
    a.startTime.wSecond = 0;
    // Populate wDayOfWeek
    FILETIME ft;
    if (SystemTimeToFileTime(&a.startTime, &ft)) {
        FileTimeToSystemTime(&ft, &a.startTime);
    }

    // Kind
    if (SendMessageW(m_hRbOnce,   BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Once;
    else if (SendMessageW(m_hRbDaily,    BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Daily;
    else if (SendMessageW(m_hRbEveryN,   BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::EveryNDays;
    else if (SendMessageW(m_hRbWeekly,   BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Weekly;
    else if (SendMessageW(m_hRbMonthly,  BM_GETCHECK, 0, 0) == BST_CHECKED) {
        a.kind = (SendMessageW(m_hRbMonthlyNth, BM_GETCHECK, 0, 0) == BST_CHECKED)
                     ? AlarmKind::MonthlyNth : AlarmKind::MonthlyDay;
    }
    else if (SendMessageW(m_hRbQuarterly, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Quarterly;
    else if (SendMessageW(m_hRbYearly,    BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Yearly;

    wchar_t buf[64];
    GetWindowTextW(m_hEditEveryN, buf, 32);
    a.intervalDays = (std::max)(1, _wtoi(buf));

    // Weekday mask
    a.weekdayMask = 0;
    for (int i = 0; i < 7; ++i) {
        if (SendMessageW(m_hChkWd[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            a.weekdayMask |= static_cast<uint8_t>(1u << kDisplayToMaskBit[i]);
        }
    }

    GetWindowTextW(m_hEditMonthlyDay, buf, 32);
    a.monthDay = (std::clamp)(_wtoi(buf), 1, 31);
    GetWindowTextW(m_hEditMonthlyNth, buf, 32);
    a.nthWeek = (std::clamp)(_wtoi(buf), 1, 5);
    LRESULT cbIdx = SendMessageW(m_hComboMonthlyWd, CB_GETCURSEL, 0, 0);
    if (cbIdx >= 0 && cbIdx < 7) a.nthWeekday = kDisplayToMaskBit[cbIdx];

    GetWindowTextW(m_hEditQuarterDay, buf, 32);
    a.quarterDay = (std::clamp)(_wtoi(buf), 1, 90);

    // End condition
    if (SendMessageW(m_hRbEndNever,   BM_GETCHECK, 0, 0) == BST_CHECKED) a.endKind = AlarmEndKind::Never;
    else if (SendMessageW(m_hRbEndAfterN,  BM_GETCHECK, 0, 0) == BST_CHECKED) a.endKind = AlarmEndKind::AfterN;
    else if (SendMessageW(m_hRbEndOnDate,  BM_GETCHECK, 0, 0) == BST_CHECKED) a.endKind = AlarmEndKind::OnDate;

    GetWindowTextW(m_hEditEndCount, buf, 32);
    a.endCount = (std::max)(1, _wtoi(buf));
    SYSTEMTIME endDt;
    SendMessageW(m_hEndDate, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&endDt));
    a.endDate = endDt;

    // Notification
    a.popup = SendMessageW(m_hChkPopup, BM_GETCHECK, 0, 0) == BST_CHECKED;
    a.sound = SendMessageW(m_hChkSound, BM_GETCHECK, 0, 0) == BST_CHECKED;
    wchar_t soundBuf[MAX_PATH];
    GetWindowTextW(m_hEditSoundFile, soundBuf, MAX_PATH);
    a.soundFile = soundBuf;
    GetWindowTextW(m_hEditSnooze, buf, 32);
    a.snoozeMinutes = (std::max)(1, _wtoi(buf));
    a.paused = SendMessageW(m_hChkPaused, BM_GETCHECK, 0, 0) == BST_CHECKED;

    // Reset runtime state on config change (user edited the alarm)
    a.firedCount = 0;
    a.snoozeUntil = 0;

    note->alarm = std::move(a);
}

void AlarmConfigDialog::UpdateControlStates() {
    bool everyN = SendMessageW(m_hRbEveryN, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool weekly = SendMessageW(m_hRbWeekly, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool monthly = SendMessageW(m_hRbMonthly, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool monthlyDay = SendMessageW(m_hRbMonthlyDay, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool monthlyNth = SendMessageW(m_hRbMonthlyNth, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool quarterly = SendMessageW(m_hRbQuarterly, BM_GETCHECK, 0, 0) == BST_CHECKED;

    EnableWindow(m_hEditEveryN, everyN);
    for (int i = 0; i < 7; ++i) EnableWindow(m_hChkWd[i], weekly);
    EnableWindow(m_hRbMonthlyDay, monthly);
    EnableWindow(m_hRbMonthlyNth, monthly);
    EnableWindow(m_hEditMonthlyDay, monthly && monthlyDay);
    EnableWindow(m_hEditMonthlyNth, monthly && monthlyNth);
    EnableWindow(m_hComboMonthlyWd, monthly && monthlyNth);
    EnableWindow(m_hEditQuarterDay, quarterly);

    bool endAfterN = SendMessageW(m_hRbEndAfterN, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool endOnDate = SendMessageW(m_hRbEndOnDate, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(m_hEditEndCount, endAfterN);
    EnableWindow(m_hEndDate, endOnDate);

    bool sound = SendMessageW(m_hChkSound, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(m_hEditSoundFile, sound);
    EnableWindow(m_hBtnBrowse, sound);
}

void AlarmConfigDialog::UpdatePreview() {
    // Build a transient AlarmConfig from current control state
    AlarmConfig a;
    SYSTEMTIME dt, tm;
    SendMessageW(m_hStartDate, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&dt));
    SendMessageW(m_hStartTime, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&tm));
    a.startTime.wYear   = dt.wYear;
    a.startTime.wMonth  = dt.wMonth;
    a.startTime.wDay    = dt.wDay;
    a.startTime.wHour   = tm.wHour;
    a.startTime.wMinute = tm.wMinute;

    if (SendMessageW(m_hRbOnce, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Once;
    else if (SendMessageW(m_hRbDaily, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Daily;
    else if (SendMessageW(m_hRbEveryN, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::EveryNDays;
    else if (SendMessageW(m_hRbWeekly, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Weekly;
    else if (SendMessageW(m_hRbMonthly, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        a.kind = (SendMessageW(m_hRbMonthlyNth, BM_GETCHECK, 0, 0) == BST_CHECKED)
                     ? AlarmKind::MonthlyNth : AlarmKind::MonthlyDay;
    }
    else if (SendMessageW(m_hRbQuarterly, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Quarterly;
    else if (SendMessageW(m_hRbYearly, BM_GETCHECK, 0, 0) == BST_CHECKED) a.kind = AlarmKind::Yearly;

    wchar_t buf[32];
    GetWindowTextW(m_hEditEveryN, buf, 32);
    a.intervalDays = (std::max)(1, _wtoi(buf));

    for (int i = 0; i < 7; ++i) {
        if (SendMessageW(m_hChkWd[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            a.weekdayMask |= static_cast<uint8_t>(1u << kDisplayToMaskBit[i]);
        }
    }
    GetWindowTextW(m_hEditMonthlyDay, buf, 32);
    a.monthDay = (std::clamp)(_wtoi(buf), 1, 31);
    GetWindowTextW(m_hEditMonthlyNth, buf, 32);
    a.nthWeek = (std::clamp)(_wtoi(buf), 1, 5);
    LRESULT cbIdx = SendMessageW(m_hComboMonthlyWd, CB_GETCURSEL, 0, 0);
    if (cbIdx >= 0 && cbIdx < 7) a.nthWeekday = kDisplayToMaskBit[cbIdx];
    GetWindowTextW(m_hEditQuarterDay, buf, 32);
    a.quarterDay = (std::clamp)(_wtoi(buf), 1, 90);

    if (SendMessageW(m_hRbEndNever, BM_GETCHECK, 0, 0) == BST_CHECKED) a.endKind = AlarmEndKind::Never;
    else if (SendMessageW(m_hRbEndAfterN, BM_GETCHECK, 0, 0) == BST_CHECKED) a.endKind = AlarmEndKind::AfterN;
    else if (SendMessageW(m_hRbEndOnDate, BM_GETCHECK, 0, 0) == BST_CHECKED) a.endKind = AlarmEndKind::OnDate;

    SYSTEMTIME endDt;
    SendMessageW(m_hEndDate, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&endDt));
    a.endDate = endDt;

    a.paused = SendMessageW(m_hChkPaused, BM_GETCHECK, 0, 0) == BST_CHECKED;

    SYSTEMTIME now;
    GetLocalTime(&now);
    auto nextFire = AlarmScheduler::ComputeNextFireTime(a, now);

    std::wstring text;
    if (!nextFire.has_value()) {
        text = Ls(L"alarm.cfg.preview_none");
    } else {
        wchar_t dateBuf[64], timeBuf[32];
        GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &*nextFire,
                       nullptr, dateBuf, 64);
        GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &*nextFire,
                       nullptr, timeBuf, 32);
        text = std::wstring(dateBuf) + L" " + timeBuf;
        text += L"   " + AlarmScheduler::DescribeInterval(a);
    }
    SetWindowTextW(m_hPreview, text.c_str());
}

void AlarmConfigDialog::BrowseSoundFile() {
    wchar_t file[MAX_PATH] = {};
    GetWindowTextW(m_hEditSoundFile, file, MAX_PATH);

    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = m_hwnd;
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"Wave files (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(m_hEditSoundFile, file);
    }
}

void AlarmConfigDialog::OnOk() {
    WriteToNote();
    Application::Get().MarkDirty();
    Application::Get().RefreshNoteList();
    DestroyWindow(m_hwnd);
}

void AlarmConfigDialog::OnRemove() {
    NoteData* note = Application::Get().FindNoteData(m_noteId);
    if (note) {
        note->alarm.reset();
        Application::Get().MarkDirty();
        Application::Get().RefreshNoteList();
    }
    DestroyWindow(m_hwnd);
}
