#include "FindInNoteDialog.h"
#include "NoteWindow.h"
#include "Localization.h"
#include <windowsx.h>
#include <string>

namespace {
constexpr wchar_t kClassName[] = L"UltraNoteFindInNote";

constexpr int ID_EDIT      = 1001;
constexpr int ID_CASE      = 1002;
constexpr int ID_FIND      = 1003;
constexpr int ID_CLOSE     = 1004;

constexpr int W_MARGIN   = 10;
constexpr int W_LABEL_H  = 18;
constexpr int W_EDIT_H   = 22;
constexpr int W_CHECK_H  = 20;
constexpr int W_BTN_W    = 90;
constexpr int W_BTN_H    = 26;
constexpr int W_GAP      = 6;
constexpr int W_CLIENT_W = 340;
} // namespace

bool FindInNoteDialog::s_classRegistered = false;

bool FindInNoteDialog::EnsureClassRegistered(HINSTANCE hInst) {
    if (s_classRegistered) return true;
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    wc.cbWndExtra    = sizeof(void*);
    if (!RegisterClassExW(&wc)) return false;
    s_classRegistered = true;
    return true;
}

FindInNoteDialog::FindInNoteDialog(HINSTANCE hInst, NoteWindow* owner)
    : m_hInst(hInst), m_owner(owner) {}

FindInNoteDialog::~FindInNoteDialog() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool FindInNoteDialog::Create() {
    if (!EnsureClassRegistered(m_hInst)) return false;

    // Compute total client size from layout
    int clientH = W_MARGIN + W_LABEL_H + 2 + W_EDIT_H + W_GAP + W_CHECK_H
                  + W_GAP + W_BTN_H + W_MARGIN;
    int clientW = W_MARGIN + W_CLIENT_W + W_MARGIN;

    DWORD style   = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOOLWINDOW;

    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    // Position: next to the owner note, or centered on it
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    HWND ownerHwnd = m_owner ? m_owner->GetHwnd() : nullptr;
    if (ownerHwnd) {
        RECT ownerRc;
        GetWindowRect(ownerHwnd, &ownerRc);
        x = ownerRc.right + 8;
        y = ownerRc.top;
        // Clamp to work area
        HMONITOR hMon = MonitorFromWindow(ownerHwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(hMon, &mi)) {
            int winW = rc.right - rc.left;
            int winH = rc.bottom - rc.top;
            if (x + winW > mi.rcWork.right)  x = ownerRc.left - winW - 8;
            if (x < mi.rcWork.left)          x = mi.rcWork.left;
            if (y + winH > mi.rcWork.bottom) y = mi.rcWork.bottom - winH;
            if (y < mi.rcWork.top)           y = mi.rcWork.top;
        }
    }

    m_hwnd = CreateWindowExW(
        exStyle, kClassName, Ls(L"note.find_title").c_str(),
        style,
        x, y, rc.right - rc.left, rc.bottom - rc.top,
        ownerHwnd, nullptr, m_hInst, this);
    return m_hwnd != nullptr;
}

void FindInNoteDialog::ShowAndFocus() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(m_hwnd);
    if (m_hEdit) {
        SetFocus(m_hEdit);
        SendMessageW(m_hEdit, EM_SETSEL, 0, -1);
    }
}

void FindInNoteDialog::CreateControls() {
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    m_hLabel = CreateWindowExW(0, L"STATIC",
        Ls(L"note.find_label").c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, m_hwnd, nullptr, m_hInst, nullptr);

    m_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT)), m_hInst, nullptr);

    m_hCase = CreateWindowExW(0, L"BUTTON",
        Ls(L"note.find_case").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CASE)), m_hInst, nullptr);

    m_hFindBtn = CreateWindowExW(0, L"BUTTON",
        Ls(L"note.find_next").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIND)), m_hInst, nullptr);

    m_hCloseBtn = CreateWindowExW(0, L"BUTTON",
        Ls(L"note.find_close").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CLOSE)), m_hInst, nullptr);

    for (HWND h : { m_hLabel, m_hEdit, m_hCase, m_hFindBtn, m_hCloseBtn }) {
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }

    // Subclass edit to catch VK_RETURN and VK_ESCAPE
    SetWindowSubclass(m_hEdit, EditSubclassProc, EDIT_SUBCLASS_ID,
                      reinterpret_cast<DWORD_PTR>(this));

    LayoutControls();
}

void FindInNoteDialog::LayoutControls() {
    int y = W_MARGIN;
    SetWindowPos(m_hLabel, nullptr, W_MARGIN, y, W_CLIENT_W, W_LABEL_H,
                 SWP_NOZORDER);
    y += W_LABEL_H + 2;
    SetWindowPos(m_hEdit, nullptr, W_MARGIN, y, W_CLIENT_W, W_EDIT_H,
                 SWP_NOZORDER);
    y += W_EDIT_H + W_GAP;
    SetWindowPos(m_hCase, nullptr, W_MARGIN, y, W_CLIENT_W, W_CHECK_H,
                 SWP_NOZORDER);
    y += W_CHECK_H + W_GAP;

    int totalW = W_MARGIN + W_CLIENT_W + W_MARGIN;
    int rightEdge = totalW - W_MARGIN;
    SetWindowPos(m_hCloseBtn, nullptr, rightEdge - W_BTN_W, y, W_BTN_W, W_BTN_H,
                 SWP_NOZORDER);
    SetWindowPos(m_hFindBtn, nullptr, rightEdge - 2 * W_BTN_W - W_GAP, y,
                 W_BTN_W, W_BTN_H, SWP_NOZORDER);
}

void FindInNoteDialog::OnFindNext() {
    int len = GetWindowTextLengthW(m_hEdit);
    if (len <= 0) { MessageBeep(MB_ICONASTERISK); return; }
    std::wstring term(len, L'\0');
    GetWindowTextW(m_hEdit, term.data(), len + 1);

    bool caseSensitive = (SendMessageW(m_hCase, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (m_owner) m_owner->FindNextInNote(term, caseSensitive);
}

LRESULT CALLBACK FindInNoteDialog::WndProc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam) {
    FindInNoteDialog* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<FindInNoteDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, 0, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<FindInNoteDialog*>(GetWindowLongPtrW(hwnd, 0));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT FindInNoteDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == ID_FIND && code == BN_CLICKED)  { OnFindNext();   return 0; }
        if (id == ID_CLOSE && code == BN_CLICKED) { ShowWindow(m_hwnd, SW_HIDE); return 0; }
        break;
    }

    case WM_CLOSE:
        ShowWindow(m_hwnd, SW_HIDE);
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE && m_hEdit) {
            SetFocus(m_hEdit);
        }
        return 0;

    case WM_DESTROY:
        m_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK FindInNoteDialog::EditSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR /*subclassId*/, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<FindInNoteDialog*>(refData);
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (self) self->OnFindNext();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (self && self->m_hwnd) ShowWindow(self->m_hwnd, SW_HIDE);
            return 0;
        }
    } else if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        // Suppress beep from EDIT default handler
        return 0;
    } else if (msg == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
