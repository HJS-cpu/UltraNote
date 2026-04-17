#include "AlarmPopupWindow.h"
#include "Application.h"
#include "Localization.h"
#include "Resource.h"
#include <mmsystem.h>

bool AlarmPopupWindow::s_classRegistered = false;

static constexpr wchar_t kClassName[] = L"UltraNoteAlarmPopup";

// Client-area layout constants (pixels)
static constexpr int kClientW = 360;
static constexpr int kClientH = 170;
static constexpr int kMargin  = 12;
static constexpr int kBtnW    = 100;
static constexpr int kBtnH    = 28;

AlarmPopupWindow::AlarmPopupWindow(HINSTANCE hInst, uint64_t noteId,
                                   const std::wstring& title, const std::wstring& text,
                                   bool playSound, const std::wstring& soundFile,
                                   int snoozeMinutes, int stackIndex)
    : m_hInst(hInst), m_noteId(noteId), m_title(title), m_text(text),
      m_playSound(playSound), m_soundFile(soundFile),
      m_snoozeMinutes(snoozeMinutes), m_stackIndex(stackIndex) {
}

AlarmPopupWindow::~AlarmPopupWindow() {
    StopSound();
    if (m_hFontTitle) DeleteObject(m_hFontTitle);
    if (m_hFontText)  DeleteObject(m_hFontText);
}

bool AlarmPopupWindow::EnsureClassRegistered(HINSTANCE hInst) {
    if (s_classRegistered) return true;
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCE(IDI_ALARM));
    wc.hIconSm       = LoadIconW(hInst, MAKEINTRESOURCE(IDI_ALARM));
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) return false;
    s_classRegistered = true;
    return true;
}

bool AlarmPopupWindow::Create() {
    if (!EnsureClassRegistered(m_hInst)) return false;

    // Adjust client to total size
    RECT r = { 0, 0, kClientW, kClientH };
    DWORD style   = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    AdjustWindowRectEx(&r, style, FALSE, exStyle);

    // Position: lower-right of work area, stacked upward
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int totalW = r.right - r.left;
    int totalH = r.bottom - r.top;
    int x = wa.right - totalW - 12;
    int y = wa.bottom - totalH - 12 - m_stackIndex * (totalH + 8);
    if (y < wa.top + 8) y = wa.top + 8;

    std::wstring windowTitle = Ls(L"alarm.popup.title");
    m_hwnd = CreateWindowExW(exStyle, kClassName, windowTitle.c_str(),
                             style, x, y, totalW, totalH,
                             nullptr, nullptr, m_hInst, this);
    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    FlashWindow(m_hwnd, TRUE);
    StartSound();
    return true;
}

LRESULT CALLBACK AlarmPopupWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AlarmPopupWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<AlarmPopupWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<AlarmPopupWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT AlarmPopupWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            CreateControls();
            return 0;

        case WM_COMMAND: {
            UINT cmd = LOWORD(wp);
            if (cmd == IDC_ALARM_OPEN)    { OnAction(AlarmAction::OpenNote); return 0; }
            if (cmd == IDC_ALARM_SNOOZE)  { OnAction(AlarmAction::Snooze);   return 0; }
            if (cmd == IDC_ALARM_DISMISS) { OnAction(AlarmAction::Dismiss);  return 0; }
            break;
        }

        case WM_CLOSE:
            OnAction(AlarmAction::Dismiss);
            return 0;

        case WM_NCDESTROY: {
            StopSound();
            if (!m_actionTaken) {
                Application::Get().OnAlarmPopupClosed(m_noteId, AlarmAction::Dismiss);
                m_actionTaken = true;
            }
            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
            delete this;
            return 0;
        }
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

void AlarmPopupWindow::CreateControls() {
    NONCLIENTMETRICS ncm = { sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    m_hFontText = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONT lf = ncm.lfMessageFont;
    lf.lfWeight = FW_BOLD;
    lf.lfHeight = lf.lfHeight - 2;  // slightly larger
    m_hFontTitle = CreateFontIndirectW(&lf);

    int y = kMargin;
    m_hLblTitle = CreateWindowExW(0, L"STATIC", m_title.c_str(),
                                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                  kMargin, y, kClientW - 2 * kMargin, 22,
                                  m_hwnd, nullptr, m_hInst, nullptr);
    SendMessageW(m_hLblTitle, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontTitle), TRUE);

    y += 26;
    m_hLblText = CreateWindowExW(0, L"STATIC", m_text.c_str(),
                                 WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                 kMargin, y, kClientW - 2 * kMargin, 60,
                                 m_hwnd, nullptr, m_hInst, nullptr);
    SendMessageW(m_hLblText, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontText), TRUE);

    // Buttons along the bottom
    int btnY = kClientH - kMargin - kBtnH;
    int spacing = (kClientW - 2 * kMargin - 3 * kBtnW) / 2;
    if (spacing < 4) spacing = 4;

    m_hBtnOpen = CreateWindowExW(0, L"BUTTON", Ls(L"alarm.popup.open").c_str(),
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                 kMargin, btnY, kBtnW, kBtnH,
                                 m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_OPEN)),
                                 m_hInst, nullptr);
    SendMessageW(m_hBtnOpen, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontText), TRUE);

    m_hBtnSnooze = CreateWindowExW(0, L"BUTTON", Ls(L"alarm.popup.snooze").c_str(),
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                   kMargin + kBtnW + spacing, btnY, kBtnW, kBtnH,
                                   m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_SNOOZE)),
                                   m_hInst, nullptr);
    SendMessageW(m_hBtnSnooze, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontText), TRUE);

    m_hBtnDismiss = CreateWindowExW(0, L"BUTTON", Ls(L"alarm.popup.dismiss").c_str(),
                                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                    kMargin + 2 * (kBtnW + spacing), btnY, kBtnW, kBtnH,
                                    m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ALARM_DISMISS)),
                                    m_hInst, nullptr);
    SendMessageW(m_hBtnDismiss, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontText), TRUE);
}

void AlarmPopupWindow::StartSound() {
    if (!m_playSound) return;
    if (!m_soundFile.empty()) {
        DWORD attrs = GetFileAttributesW(m_soundFile.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            PlaySoundW(m_soundFile.c_str(), nullptr,
                       SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
            m_soundActive = true;
            return;
        }
    }
    // Fallback: one-shot system beep
    MessageBeep(MB_ICONEXCLAMATION);
}

void AlarmPopupWindow::StopSound() {
    if (m_soundActive) {
        PlaySoundW(nullptr, nullptr, SND_PURGE);
        m_soundActive = false;
    }
}

void AlarmPopupWindow::OnAction(AlarmAction action) {
    if (m_actionTaken) return;
    m_actionTaken = true;
    StopSound();
    Application::Get().OnAlarmPopupClosed(m_noteId, action);
    // Close the window; deletion happens in WM_NCDESTROY
    DestroyWindow(m_hwnd);
}
