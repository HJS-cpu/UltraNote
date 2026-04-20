#pragma once

#include <windows.h>
#include <string>

// Custom-drawn balloon popup shown while the mouse hovers the systray icon.
// Replaces the default Windows tooltip (szTip left empty) to give UltraNote
// a distinctive sticky-note look: rounded rectangle body + short tail pointing
// at the tray icon. Single instance, owned by Application.
class TrayBubbleWindow {
public:
    bool Create(HINSTANCE hInst);
    void Destroy();
    HWND GetHwnd() const { return m_hwnd; }

    // Show the bubble anchored to trayIconRect (screen coords, from
    // Shell_NotifyIconGetRect). Header is drawn in bold on the first line,
    // body is rendered below in the regular weight (may contain newlines).
    void Show(const RECT& trayIconRect,
              const std::wstring& header,
              const std::wstring& body);
    void Hide();
    bool IsVisible() const;

private:
    // Taskbar edge relative to the work area — determines which side of
    // the bubble the tail exits and how the body is aligned to the icon.
    enum class TailSide { Bottom, Top, Left, Right };

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void Paint(HDC hdc);
    SIZE MeasureContent() const;
    void BuildRegion(int bubbleW, int bubbleH, TailSide side, int tailCenter);

    HINSTANCE m_hInst        = nullptr;
    HWND      m_hwnd         = nullptr;
    HFONT     m_hFontBold    = nullptr;
    HFONT     m_hFontRegular = nullptr;

    std::wstring m_header;
    std::wstring m_body;
    TailSide     m_tailSide   = TailSide::Bottom;
    int          m_tailCenter = 0;  // pixel offset of tail tip along its side

    static bool s_classRegistered;
};
