#include "HeaderDragOverlay.h"

namespace {
    const wchar_t kClassName[] = L"UltraNoteHeaderDragOverlay";
    bool g_classRegistered = false;

    // Premultiplied BGRA, opaque red
    constexpr BYTE kB = 0, kG = 0, kR = 210, kA = 255;
}

HeaderDragOverlay::HeaderDragOverlay(HINSTANCE hInst) : m_hInst(hInst) {
    EnsureClassRegistered();
    m_hTop    = CreateLayeredWindow(kArrowW,  kArrowH);
    m_hBottom = CreateLayeredWindow(kArrowW,  kArrowH);
    m_hStripe = CreateLayeredWindow(kStripeW, 1);        // height will be grown on first Show
    if (m_hTop)    PaintArrow(m_hTop,    /*pointingDown=*/true);
    if (m_hBottom) PaintArrow(m_hBottom, /*pointingDown=*/false);
}

HeaderDragOverlay::~HeaderDragOverlay() {
    if (m_hTop)    DestroyWindow(m_hTop);
    if (m_hBottom) DestroyWindow(m_hBottom);
    if (m_hStripe) DestroyWindow(m_hStripe);
}

void HeaderDragOverlay::EnsureClassRegistered() {
    if (g_classRegistered) return;
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = m_hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);
    g_classRegistered = true;
}

LRESULT CALLBACK HeaderDragOverlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND HeaderDragOverlay::CreateLayeredWindow(int w, int h) {
    return CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, nullptr,
        WS_POPUP, 0, 0, w, h,
        nullptr, nullptr, m_hInst, nullptr
    );
}

void HeaderDragOverlay::PaintArrow(HWND hwnd, bool pointingDown) {
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = kArrowW;
    bmi.bmiHeader.biHeight      = -kArrowH; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hOldBmp = static_cast<HBITMAP>(SelectObject(hdcMem, hBmp));

    BYTE* px = static_cast<BYTE*>(pBits);
    ZeroMemory(px, static_cast<size_t>(kArrowW) * kArrowH * 4);

    for (int y = 0; y < kArrowH; ++y) {
        int inset = pointingDown ? y : (kArrowH - 1 - y);
        int xStart = inset;
        int xEnd   = kArrowW - 1 - inset;
        if (xStart > xEnd) continue;
        BYTE* row = px + y * kArrowW * 4;
        for (int x = xStart; x <= xEnd; ++x) {
            row[x * 4 + 0] = kB;
            row[x * 4 + 1] = kG;
            row[x * 4 + 2] = kR;
            row[x * 4 + 3] = kA;
        }
    }

    SIZE sz = { kArrowW, kArrowH };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, hdcScreen, nullptr, &sz, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void HeaderDragOverlay::PaintStripe(HWND hwnd, int height) {
    if (height <= 0) return;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = kStripeW;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hOldBmp = static_cast<HBITMAP>(SelectObject(hdcMem, hBmp));

    BYTE* px = static_cast<BYTE*>(pBits);
    // Full rectangle, opaque red — fully covers Windows' default 1px insertion line.
    for (int y = 0; y < height; ++y) {
        BYTE* row = px + y * kStripeW * 4;
        for (int x = 0; x < kStripeW; ++x) {
            row[x * 4 + 0] = kB;
            row[x * 4 + 1] = kG;
            row[x * 4 + 2] = kR;
            row[x * 4 + 3] = kA;
        }
    }

    SIZE sz = { kStripeW, height };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, hdcScreen, nullptr, &sz, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void HeaderDragOverlay::Show(int xScreen, int yTopScreen, int yBottomScreen) {
    if (!m_hTop || !m_hBottom || !m_hStripe) return;

    int stripeH = yBottomScreen - yTopScreen;
    if (stripeH < 1) stripeH = 1;
    if (stripeH != m_stripeHeight) {
        PaintStripe(m_hStripe, stripeH);
        m_stripeHeight = stripeH;
    }

    // Tip centers: (xScreen, yTopScreen) for upper, (xScreen, yBottomScreen) for lower
    int topX = xScreen - kArrowW / 2;
    int topY = yTopScreen - kArrowH;
    int botX = xScreen - kArrowW / 2;
    int botY = yBottomScreen;
    int stpX = xScreen - kStripeW / 2;
    int stpY = yTopScreen;

    SetWindowPos(m_hStripe, HWND_TOPMOST, stpX, stpY, kStripeW, stripeH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(m_hTop,    HWND_TOPMOST, topX, topY, kArrowW, kArrowH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(m_hBottom, HWND_TOPMOST, botX, botY, kArrowW, kArrowH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void HeaderDragOverlay::Hide() {
    if (m_hTop)    ShowWindow(m_hTop,    SW_HIDE);
    if (m_hBottom) ShowWindow(m_hBottom, SW_HIDE);
    if (m_hStripe) ShowWindow(m_hStripe, SW_HIDE);
}
