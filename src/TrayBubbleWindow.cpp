#include "TrayBubbleWindow.h"
#include <algorithm>
#include <vector>

bool TrayBubbleWindow::s_classRegistered = false;

static constexpr wchar_t kClassName[] = L"UltraNoteTrayBubble";

// Visual constants. The kPx* values are authored at 96 dpi and scaled to the
// bubble's actual monitor DPI at Show() time (see ScaleDpi / m_dpi).
namespace {
    constexpr COLORREF kBgColor     = RGB(255, 252, 206);  // Sticky-note yellow
    constexpr COLORREF kBorderColor = RGB(170, 150,  60);  // Darker amber
    constexpr COLORREF kHeaderColor = RGB( 40,  40,  40);
    constexpr COLORREF kBodyColor   = RGB( 60,  60,  60);

    constexpr int kPxPadX        = 12;   // Horizontal padding inside bubble body
    constexpr int kPxPadY        = 8;    // Vertical padding
    constexpr int kPxCornerR     = 8;    // Rounded corner radius
    constexpr int kPxTailLen     = 10;   // Height of the tail (in tail direction)
    constexpr int kPxTailHalfW   = 9;    // Half-width of the tail base
    constexpr int kPxHeaderGap   = 4;    // Extra space under header line
    constexpr int kPxMaxContentW = 320;  // Maximum text width before wrapping
    constexpr int kPxGap         = 6;    // Gap between bubble and tray icon

    // Effective DPI of a monitor via GetDpiForMonitor (shcore.dll, Win8.1+) loaded
    // through GetProcAddress so we don't add a shcore.lib link dependency. Returns
    // 96 on older systems / failure.
    UINT MonitorDpi(HMONITOR hMon) {
        using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
        static GetDpiForMonitorFn fn = []() -> GetDpiForMonitorFn {
            HMODULE h = LoadLibraryW(L"shcore.dll");   // process-lifetime, intentionally not freed
            return h ? reinterpret_cast<GetDpiForMonitorFn>(
                           GetProcAddress(h, "GetDpiForMonitor")) : nullptr;
        }();
        if (!fn) return 96;
        UINT dx = 96, dy = 96;
        if (SUCCEEDED(fn(hMon, 0 /*MDT_EFFECTIVE_DPI*/, &dx, &dy)) && dx > 0) return dx;
        return 96;
    }
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

    // Fonts are created lazily in Show() once the target monitor DPI is known
    // (a bubble may pop on a 96-dpi taskbar while the app started on a 192-dpi
    // primary monitor), so we don't build them here.
    return true;
}

// Rebuilds the bubble fonts for `dpi` using the per-DPI non-client metrics.
// SystemParametersInfoForDpi (Win10 1607+) returns the message font already
// scaled for the given DPI; on older systems we fall back to the 96-dpi metrics
// and scale the height manually so the bubble still grows on high-DPI monitors.
void TrayBubbleWindow::EnsureFontsForDpi(UINT dpi) {
    if (dpi == 0) dpi = 96;
    if (m_hFontRegular && m_hFontBold && dpi == m_dpi) return;  // already current
    m_dpi = dpi;

    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    bool scaled = false;
    // GetProcAddress keeps the link clean (user32 is always loaded) and degrades
    // gracefully on pre-1607 builds where the symbol is absent.
    using SPIFD = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
    if (HMODULE hUser = GetModuleHandleW(L"user32.dll")) {
        if (auto pSPIFD = reinterpret_cast<SPIFD>(
                GetProcAddress(hUser, "SystemParametersInfoForDpi"))) {
            scaled = pSPIFD(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi) != FALSE;
        }
    }
    if (!scaled) {
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        // 96-dpi metrics: scale the font height to the target DPI ourselves.
        ncm.lfMessageFont.lfHeight = MulDiv(ncm.lfMessageFont.lfHeight, dpi, 96);
    }

    LOGFONTW lfRegular = ncm.lfMessageFont;
    LOGFONTW lfBold    = ncm.lfMessageFont;
    lfBold.lfWeight    = FW_BOLD;

    if (m_hFontRegular) { DeleteObject(m_hFontRegular); m_hFontRegular = nullptr; }
    if (m_hFontBold)    { DeleteObject(m_hFontBold);    m_hFontBold    = nullptr; }
    m_hFontRegular = CreateFontIndirectW(&lfRegular);
    m_hFontBold    = CreateFontIndirectW(&lfBold);
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

    const int maxContentW = ScaleDpi(kPxMaxContentW);
    auto measureBlock = [&](const std::wstring& text, HFONT font, int& outH) -> int {
        if (text.empty()) { outH = 0; return 0; }
        HGDIOBJ oldF = SelectObject(hdc, font);
        RECT rc = { 0, 0, maxContentW, 0 };
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
    result.cy = headerH + (headerH && bodyH ? ScaleDpi(kPxHeaderGap) : 0) + bodyH;

    ReleaseDC(m_hwnd, hdc);
    return result;
}

// Builds the combined region (rounded rect body + triangular tail) and
// assigns it to the window. The tail protrudes `kTailLen` pixels on `side`,
// with its tip horizontally/vertically centered at `tailCenter` in client
// coords. The rectangular body occupies the remaining client area.
void TrayBubbleWindow::BuildRegion(int bubbleW, int bubbleH, TailSide side, int tailCenter) {
    const int tailLen   = ScaleDpi(kPxTailLen);
    const int tailHalfW = ScaleDpi(kPxTailHalfW);
    const int cornerR   = ScaleDpi(kPxCornerR);

    RECT bodyRc = { 0, 0, bubbleW, bubbleH };
    switch (side) {
        case TailSide::Bottom: bodyRc.bottom -= tailLen; break;
        case TailSide::Top:    bodyRc.top    += tailLen; break;
        case TailSide::Right:  bodyRc.right  -= tailLen; break;
        case TailSide::Left:   bodyRc.left   += tailLen; break;
    }

    HRGN rBody = CreateRoundRectRgn(bodyRc.left, bodyRc.top,
                                    bodyRc.right + 1, bodyRc.bottom + 1,
                                    cornerR * 2, cornerR * 2);

    POINT tri[3] = {};
    const int c = tailCenter;
    switch (side) {
        case TailSide::Bottom:
            tri[0] = { c - tailHalfW, bodyRc.bottom - 1 };
            tri[1] = { c + tailHalfW, bodyRc.bottom - 1 };
            tri[2] = { c,             bubbleH - 1       };
            break;
        case TailSide::Top:
            tri[0] = { c - tailHalfW, bodyRc.top + 1 };
            tri[1] = { c + tailHalfW, bodyRc.top + 1 };
            tri[2] = { c,             0              };
            break;
        case TailSide::Right:
            tri[0] = { bodyRc.right - 1, c - tailHalfW };
            tri[1] = { bodyRc.right - 1, c + tailHalfW };
            tri[2] = { bubbleW - 1,      c             };
            break;
        case TailSide::Left:
            tri[0] = { bodyRc.left + 1, c - tailHalfW };
            tri[1] = { bodyRc.left + 1, c + tailHalfW };
            tri[2] = { 0,               c             };
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
    const int tailLen   = ScaleDpi(kPxTailLen);
    const int tailHalfW = ScaleDpi(kPxTailHalfW);
    const int cornerR   = ScaleDpi(kPxCornerR);
    const int padX      = ScaleDpi(kPxPadX);
    const int padY      = ScaleDpi(kPxPadY);
    const int headerGap = ScaleDpi(kPxHeaderGap);

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
        case TailSide::Bottom: bodyRc.bottom -= tailLen; break;
        case TailSide::Top:    bodyRc.top    += tailLen; break;
        case TailSide::Right:  bodyRc.right  -= tailLen; break;
        case TailSide::Left:   bodyRc.left   += tailLen; break;
    }
    HPEN pen       = CreatePen(PS_SOLID, 1, kBorderColor);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, bodyRc.left, bodyRc.top, bodyRc.right, bodyRc.bottom,
              cornerR * 2, cornerR * 2);

    // Tail outline: two edges from the triangle (the third is hidden behind
    // the body stroke, which is rounded and actually leaves a tiny gap — so
    // we draw the full tail triangle too and let the body stroke overlap.)
    POINT tri[4] = {};  // 4 points so Polyline closes the triangle back
    const int c = m_tailCenter;
    switch (m_tailSide) {
        case TailSide::Bottom:
            tri[0] = { c - tailHalfW, bodyRc.bottom };
            tri[1] = { c,             rc.bottom - 1 };
            tri[2] = { c + tailHalfW, bodyRc.bottom };
            tri[3] = tri[0];
            break;
        case TailSide::Top:
            tri[0] = { c - tailHalfW, bodyRc.top };
            tri[1] = { c,             0          };
            tri[2] = { c + tailHalfW, bodyRc.top };
            tri[3] = tri[0];
            break;
        case TailSide::Right:
            tri[0] = { bodyRc.right, c - tailHalfW };
            tri[1] = { rc.right - 1, c             };
            tri[2] = { bodyRc.right, c + tailHalfW };
            tri[3] = tri[0];
            break;
        case TailSide::Left:
            tri[0] = { bodyRc.left, c - tailHalfW };
            tri[1] = { 0,           c             };
            tri[2] = { bodyRc.left, c + tailHalfW };
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
    InflateRect(&textRc, -padX, -padY);

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
        textRc.top = calc.bottom + headerGap;
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

    // Scale everything to the tray icon's monitor DPI. Must run before
    // MeasureContent()/ScaleDpi(), which read m_dpi.
    EnsureFontsForDpi(MonitorDpi(MonitorFromRect(&trayIconRect, MONITOR_DEFAULTTONEAREST)));
    const int kPadX      = ScaleDpi(kPxPadX);
    const int kPadY      = ScaleDpi(kPxPadY);
    const int kTailLen   = ScaleDpi(kPxTailLen);
    const int kGap       = ScaleDpi(kPxGap);
    const int kCornerR   = ScaleDpi(kPxCornerR);
    const int kTailHalfW = ScaleDpi(kPxTailHalfW);

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
