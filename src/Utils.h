#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <cstdarg>

// RAII wrapper for GDI objects (HBRUSH, HFONT, HPEN, etc.)
class GdiObject {
public:
    GdiObject() : m_obj(nullptr) {}
    explicit GdiObject(HGDIOBJ obj) : m_obj(obj) {}
    ~GdiObject() { Reset(); }

    GdiObject(const GdiObject&) = delete;
    GdiObject& operator=(const GdiObject&) = delete;

    GdiObject(GdiObject&& other) noexcept : m_obj(other.m_obj) {
        other.m_obj = nullptr;
    }
    GdiObject& operator=(GdiObject&& other) noexcept {
        if (this != &other) {
            Reset();
            m_obj = other.m_obj;
            other.m_obj = nullptr;
        }
        return *this;
    }

    void Reset(HGDIOBJ obj = nullptr) {
        if (m_obj) DeleteObject(m_obj);
        m_obj = obj;
    }

    HGDIOBJ Get() const { return m_obj; }
    operator HGDIOBJ() const { return m_obj; }
    explicit operator bool() const { return m_obj != nullptr; }

private:
    HGDIOBJ m_obj;
};

// RAII scope guard for SelectObject / restore pattern
class GdiSelect {
public:
    GdiSelect(HDC hdc, HGDIOBJ obj)
        : m_hdc(hdc), m_old(SelectObject(hdc, obj)) {}
    ~GdiSelect() { SelectObject(m_hdc, m_old); }

    GdiSelect(const GdiSelect&) = delete;
    GdiSelect& operator=(const GdiSelect&) = delete;

private:
    HDC m_hdc;
    HGDIOBJ m_old;
};

// Load a string resource into std::wstring
inline std::wstring LoadStringFromResource(HINSTANCE hInst, UINT id) {
    const wchar_t* ptr = nullptr;
    int len = LoadStringW(hInst, id, reinterpret_cast<LPWSTR>(&ptr), 0);
    if (len > 0 && ptr)
        return std::wstring(ptr, static_cast<size_t>(len));
    return std::wstring();
}

// Format a wide string (printf-style)
inline std::wstring FormatString(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = _vscwprintf(fmt, args);
    va_end(args);
    if (len <= 0) return std::wstring();

    std::wstring result(static_cast<size_t>(len), L'\0');
    va_start(args, fmt);
    vswprintf_s(&result[0], static_cast<size_t>(len) + 1, fmt, args);
    va_end(args);
    return result;
}

// Create an HFONT from font parameters
inline HFONT CreateFontFromParams(const std::wstring& face, int sizePts,
                                  bool bold, bool italic, HDC hdc = nullptr) {
    HDC screenDC = hdc ? hdc : GetDC(nullptr);
    int height = -MulDiv(sizePts, GetDeviceCaps(screenDC, LOGPIXELSY), 72);
    if (!hdc) ReleaseDC(nullptr, screenDC);

    return CreateFontW(
        height, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        italic ? TRUE : FALSE,
        FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face.c_str()
    );
}

// Convert HICON to premultiplied-alpha HBITMAP suitable for menu use
inline HBITMAP IconToBitmap(HICON hIcon, int cx, int cy) {
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = cx;
    bmi.bmiHeader.biHeight      = -cy; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (hBmp) {
        HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);
        // Fill with zero (fully transparent)
        GDI_ERROR; // unused, just ensure pBits is zeroed via CreateDIBSection
        memset(pBits, 0, static_cast<size_t>(cx) * static_cast<size_t>(cy) * 4);
        DrawIconEx(hdcMem, 0, 0, hIcon, cx, cy, 0, nullptr, DI_NORMAL);
        SelectObject(hdcMem, oldBmp);
    }

    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hBmp;
}

// Load a shell stock icon as menu-ready HBITMAP (small icon size)
inline HBITMAP LoadShellMenuBitmap(SHSTOCKICONID id) {
    SHSTOCKICONINFO sii = {};
    sii.cbSize = sizeof(sii);
    HRESULT hr = SHGetStockIconInfo(id, SHGSI_ICON | SHGSI_SMALLICON, &sii);
    if (FAILED(hr)) return nullptr;

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    HBITMAP hBmp = IconToBitmap(sii.hIcon, cx, cy);
    DestroyIcon(sii.hIcon);
    return hBmp;
}

// Load an icon resource as menu-ready HBITMAP (small icon size)
inline HBITMAP LoadResourceMenuBitmap(UINT iconId) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    HICON hIcon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCE(iconId),
                                                 IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
    if (!hIcon) return nullptr;
    HBITMAP hBmp = IconToBitmap(hIcon, cx, cy);
    DestroyIcon(hIcon);
    return hBmp;
}

// Check if a key press matches a configured shortcut (VK + modifiers)
inline bool MatchesShortcut(WORD shortcut, WPARAM vk) {
    BYTE scVk = LOBYTE(shortcut);
    BYTE scMods = HIBYTE(shortcut);
    if (static_cast<BYTE>(vk) != scVk) return false;
    bool needCtrl  = (scMods & HOTKEYF_CONTROL) != 0;
    bool needShift = (scMods & HOTKEYF_SHIFT) != 0;
    bool needAlt   = (scMods & HOTKEYF_ALT) != 0;
    bool hasCtrl   = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool hasShift  = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool hasAlt    = (GetKeyState(VK_MENU) & 0x8000) != 0;
    return (needCtrl == hasCtrl) && (needShift == hasShift) && (needAlt == hasAlt);
}

// Get the directory containing the running EXE
inline std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0) return L".";
    std::wstring dir(path, len);
    auto pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        dir = dir.substr(0, pos);
    return dir;
}
