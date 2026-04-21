#pragma once

#include <windows.h>

// Two tiny topmost layered windows that draw a pair of red arrow markers
// (down-pointing above, up-pointing below) at a column-header insertion gap
// while the user drags a column header in the note list.
class HeaderDragOverlay {
public:
    explicit HeaderDragOverlay(HINSTANCE hInst);
    ~HeaderDragOverlay();

    HeaderDragOverlay(const HeaderDragOverlay&) = delete;
    HeaderDragOverlay& operator=(const HeaderDragOverlay&) = delete;

    // Position/show both markers. All coordinates are in screen space.
    //   xScreen        vertical insertion line (tip x of both arrows)
    //   yTopScreen     top edge of the header  (tip y of the upper arrow)
    //   yBottomScreen  bottom edge of header   (tip y of the lower arrow)
    void Show(int xScreen, int yTopScreen, int yBottomScreen);
    void Hide();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void EnsureClassRegistered();
    HWND CreateLayeredWindow(int w, int h);
    void PaintArrow(HWND hwnd, bool pointingDown);
    void PaintStripe(HWND hwnd, int height);

    HINSTANCE m_hInst         = nullptr;
    HWND      m_hTop          = nullptr;  // points down, sits above the header
    HWND      m_hBottom       = nullptr;  // points up,   sits below the header
    HWND      m_hStripe       = nullptr;  // vertical red stripe spanning header height
    int       m_stripeHeight  = 0;        // cached height the stripe bitmap was painted for

    static constexpr int kArrowW  = 11;
    static constexpr int kArrowH  = 6;
    static constexpr int kStripeW = 3;
};
