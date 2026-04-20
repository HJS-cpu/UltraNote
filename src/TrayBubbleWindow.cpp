#include "TrayBubbleWindow.h"
#include <algorithm>
#include <vector>

bool TrayBubbleWindow::s_classRegistered = false;

static constexpr wchar_t kClassName[] = L"UltraNoteTrayBubble";

// Visual constants
namespace {
    constexpr COLORREF kBgColor     = RGB(255, 252, 206);  // Sticky-note yellow
    constexpr COLORREF kBorderColor = RGB(170, 150,  60);  // Darker amber
    constexpr COLORREF kHeaderColor = RGB( 40,  40,  40);
    constexpr COLORREF kBodyColor   = RGB( 60,  60,  60);

    constexpr int kPadX        = 12;   // Horizontal padding inside bubble body
    constexpr int kPadY        = 8;    // Vertical padding
    constexpr int kCornerR     = 8;    // Rounded corner radius
    constexpr int kTailLen     = 10;   // Height of the tail (in tail direction)
    constexpr int kTailHalfW   = 9;    // Half-width of the tail base
    constexpr int kHeaderGap   = 4;    // Extra space under header line
    constexpr int kMaxContentW = 320;  // Maximum text width before wrapping
    constexpr int kGap         = 6;    // Gap between bubble and tray icon
}

bool TrayBubbleWindow::Create(HINSTANCE hInst) {
    m_hInst = hInst;

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_SAVEBITS;
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // We paint everything ourselves
        wc.lpszClassName = kClassName;
        if (!RegisterClassExW(&wc)) return false;
        s_classRegistered = true;
    }

    // WS_EX_NOACTIVATE: never takes focus (stays behind while notes keep theirs)
    // WS_EX_TOOLWINDOW: no taskbar entry, no Alt+Tab
    // WS_EX_TOPMOST:    always above notes
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
    m_hwnd = CreateWindowExW(exStyle, kClassName, L"",
                             WS_POPUP,
                             0, 0, 100, 50,
                             nullptr, nullptr, hInst, this);
    if (!m_hwnd) return false;

    NONCLIENTMETRICS ncm = { sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    LOGFONTW lfRegular = ncm.lfMessageFont;
    LOGFONTW lfBold    = ncm.lfMessageFont;
    lfBold.lfWeight    = FW_BOLD;
    m_hFontRegular = CreateFontIndirectW(&lfRegular);
    m_hFontBold    = CreateFontIndirectW(&lfBold);

    return true;
}

void TrayBubbleWindow::Destroy() {
    if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    if (m_hFontBold)    { DeleteObject(m_hFontBold);    m_hFontBold    = nullptr; }
    if (m_hFontRegular) { DeleteObject(m_hFontRegular); m_hFontRegular = nullptr; }
}

bool TrayBubbleWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

LRESULT CALLBACK TrayBubbleWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TrayBubbleWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<TrayBubbleWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TrayBubbleWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT TrayBubbleWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            Paint(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        // Prevent the bubble from stealing activation when clicked
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        // Click the bubble → dismiss
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            Hide();
            return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

// Measures the full text block without wrapping, capped at kMaxContentW.
SIZE TrayBubbleWindow::MeasureContent() const {
    SIZE result = { 0, 0 };
    if (!m_hwnd) return result;

    HDC hdc = GetDC(m_hwnd);
    if (!hdc) return result;

    auto measureBlock = [&](const std::wstring& text, HFONT font, int& outH) -> int {
        if (text.empty()) { outH = 0; return 0; }
        HGDIOBJ oldF = SelectObject(hdc, font);
        RECT rc = { 0, 0, kMaxContentW, 0 };
        DrawTextW(hdc, text.c_str(), -1, &rc,
                  DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(hdc, oldF);
        outH = rc.bottom - rc.top;
        return rc.right - rc.left;
    };

    int headerH = 0, bodyH = 0;
    int headerW = measureBlock(m_header, m_hFontBold,    headerH);
    int bodyW   = measureBlock(m_body,   m_hFontRegular, bodyH);

    result.cx = (std::max)(headerW, bodyW);
    result.cy = headerH + (headerH && bodyH ? kHeaderGap : 0) + bodyH;

    ReleaseDC(m_hwnd, hdc);
    return result;
}

// Builds the combined region (rounded rect body + triangular tail) and
// assigns it to the window. The tail protrudes `kTailLen` pixels on `side`,
// with its tip horizontally/vertically centered at `tailCenter` in client
// coords. The rectangular body occupies the remaining client area.
void TrayBubbleWindow::BuildRegion(int bubbleW, int bubbleH, TailSide side, int tailCenter) {
    RECT bodyRc = { 0, 0, bubbleW, bubbleH };
    switch (side) {
        case TailSide::Bottom: bodyRc.bottom -= kTailLen; break;
        case TailSide::Top:    bodyRc.top    += kTailLen; break;
        case TailSide::Right:  bodyRc.right  -= kTailLen; break;
        case TailSide::Left:   bodyRc.left   += kTailLen; break;
    }

    HRGN rBody = CreateRoundRectRgn(bodyRc.left, bodyRc.top,
                                    bodyRc.right + 1, bodyRc.bottom + 1,
                                    kCornerR * 2, kCornerR * 2);

    POINT tri[3] = {};
    const int c = tailCenter;
    switch (side) {
        case TailSide::Bottom:
            tri[0] = { c - kTailHalfW, bodyRc.bottom - 1 };
            tri[1] = { c + kTailHalfW, bodyRc.bottom - 1 };
            tri[2] = { c,              bubbleH - 1      };
            break;
        case TailSide::Top:
            tri[0] = { c - kTailHalfW, bodyRc.top + 1 };
            tri[1] = { c + kTailHalfW, bodyRc.top + 1 };
            tri[2] = { c,              0              };
            break;
        case TailSide::Right:
            tri[0] = { bodyRc.right - 1, c - kTailHalfW };
            tri[1] = { bodyRc.right - 1, c + kTailHalfW };
            tri[2] = { bubbleW - 1,      c              };
            break;
        case TailSide::Left:
            tri[0] = { bodyRc.left + 1, c - kTailHalfW };
            tri[1] = { bodyRc.left + 1, c + kTailHalfW };
            tri[2] = { 0,               c              };
            break;
    }
    HRGN rTail = CreatePolygonRgn(tri, 3, WINDING);

    HRGN rFull = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(rFull, rBody, rTail, RGN_OR);
    DeleteObject(rBody);
    DeleteObject(rTail);

    // System takes ownership of rFull — do not delete below.
    SetWindowRgn(m_hwnd, rFull, TRUE);
}

void TrayBubbleWindow::Paint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    // Fill: the window region clips the fill to the actual bubble shape.
    HBRUSH bg = CreateSolidBrush(kBgColor);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    // Stroke the body rectangle outline. The tail's visible edges come for
    // free from the region boundary (clipped fill), but we want a crisp line
    // along the body perimeter. RoundRect with NULL_BRUSH strokes only.
    RECT bodyRc = rc;
    switch (m_tailSide) {
        case TailSide::Bottom: bodyRc.bottom -= kTailLen; break;
        case TailSide::Top:    bodyRc.top    += kTailLen; break;
        case TailSide::Right:  bodyRc.right  -= kTailLen; break;
        case TailSide::Left:   bodyRc.left   += kTailLen; break;
    }
    HPEN pen       = CreatePen(PS_SOLID, 1, kBorderColor);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, bodyRc.left, bodyRc.top, bodyRc.right, bodyRc.bottom,
              kCornerR * 2, kCornerR * 2);

    // Tail outline: two edges from the triangle (the third is hidden behind
    // the body stroke, which is rounded and actually leaves a tiny gap — so
    // we draw the full tail triangle too and let the body stroke overlap.)
    POINT tri[4] = {};  // 4 points so Polyline closes the triangle back
    const int c = m_tailCenter;
    switch (m_tailSide) {
        case TailSide::Bottom:
            tri[0] = { c - kTailHalfW, bodyRc.bottom };
            tri[1] = { c,              rc.bottom - 1 };
            tri[2] = { c + kTailHalfW, bodyRc.bottom };
            tri[3] = tri[0];
            break;
        case TailSide::Top:
            tri[0] = { c - kTailHalfW, bodyRc.top };
            tri[1] = { c,              0          };
            tri[2] = { c + kTailHalfW, bodyRc.top };
            tri[3] = tri[0];
            break;
        case TailSide::Right:
            tri[0] = { bodyRc.right, c - kTailHalfW };
            tri[1] = { rc.right - 1, c              };
            tri[2] = { bodyRc.right, c + kTailHalfW };
            tri[3] = tri[0];
            break;
        case TailSide::Left:
            tri[0] = { bodyRc.left, c - kTailHalfW };
            tri[1] = { 0,           c              };
            tri[2] = { bodyRc.left, c + kTailHalfW };
            tri[3] = tri[0];
            break;
    }
    Polyline(hdc, tri, 4);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    // Text
    SetBkMode(hdc, TRANSPARENT);
    RECT textRc = bodyRc;
    InflateRect(&textRc, -kPadX, -kPadY);

    if (!m_header.empty()) {
        HGDIOBJ oldF = SelectObject(hdc, m_hFontBold);
        SetTextColor(hdc, kHeaderColor);
        RECT h = textRc;
        DrawTextW(hdc, m_header.c_str(), -1, &h,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        // Advance textRc below the header line(s)
        RECT calc = textRc;
        DrawTextW(hdc, m_header.c_str(), -1, &calc,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        textRc.top = calc.bottom + kHeaderGap;
        SelectObject(hdc, oldF);
    }
    if (!m_body.empty() && textRc.top < textRc.bottom) {
        HGDIOBJ oldF = SelectObject(hdc, m_hFontRegular);
        SetTextColor(hdc, kBodyColor);
        DrawTextW(hdc, m_body.c_str(), -1, &textRc,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(hdc, oldF);
    }
}

void TrayBubbleWindow::Show(const RECT& trayIconRect,
                            const std::wstring& header,
                            const std::wstring& body) {
    if (!m_hwnd) return;
    m_header = header;
    m_body   = body;

    SIZE content = MeasureContent();
    int bubbleW = content.cx + 2 * kPadX;
    int bubbleH = content.cy + 2 * kPadY;

    // Pick tail side from the taskbar edge (compare icon rect with work area).
    RECT wa = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int iconCX = (trayIconRect.left + trayIconRect.right)  / 2;
    int iconCY = (trayIconRect.top  + trayIconRect.bottom) / 2;

    TailSide side = TailSide::Bottom;
    if (trayIconRect.bottom >= wa.bottom)      side = TailSide::Bottom;
    else if (trayIconRect.top    <= wa.top)    side = TailSide::Top;
    else if (trayIconRect.left   <= wa.left)   side = TailSide::Left;
    else if (trayIconRect.right  >= wa.right)  side = TailSide::Right;

    // Final window size includes the tail extension on the chosen side.
    int winW = bubbleW, winH = bubbleH;
    int winX = 0, winY = 0;
    int tailCenter = 0;

    switch (side) {
        case TailSide::Bottom:
            winH += kTailLen;
            winX = iconCX - winW / 2;
            winY = trayIconRect.top - kGap - winH;
            tailCenter = (std::clamp)(iconCX - winX,
                                      kCornerR + kTailHalfW,
                                      winW - kCornerR - kTailHalfW);
            break;
        case TailSide::Top:
            winH += kTailLen;
            winX = iconCX - winW / 2;
            winY = trayIconRect.bottom + kGap;
            tailCenter = (std::clamp)(iconCX - winX,
                                      kCornerR + kTailHalfW,
                                      winW - kCornerR - kTailHalfW);
            break;
        case TailSide::Right:
            winW += kTailLen;
            winX = trayIconRect.left - kGap - winW;
            winY = iconCY - winH / 2;
            tailCenter = (std::clamp)(iconCY - winY,
                                      kCornerR + kTailHalfW,
                                      winH - kCornerR - kTailHalfW);
            break;
        case TailSide::Left:
            winW += kTailLen;
            winX = trayIconRect.right + kGap;
            winY = iconCY - winH / 2;
            tailCenter = (std::clamp)(iconCY - winY,
                                      kCornerR + kTailHalfW,
                                      winH - kCornerR - kTailHalfW);
            break;
    }

    // Clamp to work area so bubble never leaves the visible desktop.
    if (winX < wa.left)           winX = wa.left;
    if (winY < wa.top)            winY = wa.top;
    if (winX + winW > wa.right)   winX = wa.right  - winW;
    if (winY + winH > wa.bottom)  winY = wa.bottom - winH;

    // Recompute tail center after clamp (icon center in new window coords).
    switch (side) {
        case TailSide::Bottom:
        case TailSide::Top:
            tailCenter = (std::clamp)(iconCX - winX,
                                      kCornerR + kTailHalfW,
                                      winW - kCornerR - kTailHalfW);
            break;
        case TailSide::Right:
        case TailSide::Left:
            tailCenter = (std::clamp)(iconCY - winY,
                                      kCornerR + kTailHalfW,
                                      winH - kCornerR - kTailHalfW);
            break;
    }

    m_tailSide   = side;
    m_tailCenter = tailCenter;

    SetWindowPos(m_hwnd, HWND_TOPMOST, winX, winY, winW, winH,
                 SWP_NOACTIVATE | SWP_HIDEWINDOW);
    BuildRegion(winW, winH, side, tailCenter);
    InvalidateRect(m_hwnd, nullptr, TRUE);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
}

void TrayBubbleWindow::Hide() {
    if (m_hwnd && IsWindowVisible(m_hwnd)) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}
