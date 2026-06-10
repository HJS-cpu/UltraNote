#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdarg>
#include <ctime>

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

// Safe placeholder substitution for LOCALIZED format strings. A wrong specifier
// in a translation (e.g. %s where %d was meant, or a huge field width) must never
// reach a printf — it would crash or overflow. These do a literal find+replace of
// the first placeholder instead. Hardcoded format literals may still use
// FormatString; .lng-sourced strings MUST go through these.
inline std::wstring FormatCount(const std::wstring& fmt, int count) {
    size_t pos = fmt.find(L"%d");
    if (pos == std::wstring::npos) return fmt;
    std::wstring result = fmt;
    result.replace(pos, 2, std::to_wstring(count));
    return result;
}

inline std::wstring FormatStr(const std::wstring& fmt, const std::wstring& value) {
    size_t pos = fmt.find(L"%s");
    if (pos == std::wstring::npos) return fmt;
    std::wstring result = fmt;
    result.replace(pos, 2, value);
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
    if (!hdcScreen) return nullptr;
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) { ReleaseDC(nullptr, hdcScreen); return nullptr; }

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
        // CreateDIBSection already zero-fills; memset is belt-and-suspenders.
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
    // AltGr (common on non-US layouts) is delivered as synthetic left-Ctrl +
    // right-Alt. It must satisfy NEITHER a Ctrl nor an Alt requirement, else
    // typing an AltGr char (e.g. AltGr+E for €) would fire a bound Ctrl+letter
    // OR Alt+letter shortcut. Clear both so AltGr chords never match a binding.
    bool rAltDown  = (GetKeyState(VK_RMENU)    & 0x8000) != 0;
    bool lCtrlDown = (GetKeyState(VK_LCONTROL) & 0x8000) != 0;
    bool rCtrlDown = (GetKeyState(VK_RCONTROL) & 0x8000) != 0;
    if (rAltDown && lCtrlDown && !rCtrlDown) { hasCtrl = false; hasAlt = false; }  // AltGr
    return (needCtrl == hasCtrl) && (needShift == hasShift) && (needAlt == hasAlt);
}

// Format a packed shortcut (LOBYTE = VK, HIBYTE = HOTKEYF_*) as display text.
// Returns "" for an unset shortcut (hotkey == 0).
inline std::wstring FormatShortcut(WORD hotkey) {
    if (hotkey == 0) return L"";
    BYTE vk   = LOBYTE(hotkey);
    BYTE mods = HIBYTE(hotkey);
    std::wstring s;
    if (mods & HOTKEYF_CONTROL) s += L"Ctrl+";
    if (mods & HOTKEYF_SHIFT)   s += L"Shift+";
    if (mods & HOTKEYF_ALT)     s += L"Alt+";
    switch (vk) {
        case VK_RETURN: s += L"Enter"; break;
        case VK_DELETE: s += L"Del";   break;
        case VK_ESCAPE: s += L"Esc";   break;
        case VK_SPACE:  s += L"Space"; break;
        case VK_TAB:    s += L"Tab";   break;
        case VK_F1: case VK_F2: case VK_F3: case VK_F4:
        case VK_F5: case VK_F6: case VK_F7: case VK_F8:
        case VK_F9: case VK_F10: case VK_F11: case VK_F12:
            s += L"F" + std::to_wstring(vk - VK_F1 + 1);
            break;
        default:
            if (vk >= 'A' && vk <= 'Z')      s += static_cast<wchar_t>(vk);
            else if (vk >= '0' && vk <= '9') s += static_cast<wchar_t>(vk);
            else {
                // Navigation/OEM keys (Home, End, Insert, arrows, ;, /, etc.) have
                // no fixed glyph — ask the OS for the localized key name instead of
                // rendering "?". Build the GetKeyNameTextW lParam from the scan code
                // (bits 16-23) and set the extended-key flag (bit 24) for keys whose
                // scan codes are shared with the numpad (nav cluster, arrows, etc.).
                UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
                bool extended =
                    vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME ||
                    vk == VK_END    || vk == VK_PRIOR  || vk == VK_NEXT ||
                    vk == VK_LEFT   || vk == VK_RIGHT  || vk == VK_UP   ||
                    vk == VK_DOWN   || vk == VK_DIVIDE || vk == VK_NUMLOCK;
                LONG lparam = static_cast<LONG>(sc << 16);
                if (extended) lparam |= (1 << 24);  // KF_EXTENDED in lParam position
                wchar_t name[64] = {};
                if (sc != 0 && GetKeyNameTextW(lparam, name, _countof(name)) > 0)
                    s += name;
                else
                    s += L"?";
            }
            break;
    }
    return s;
}

// Append "\t<shortcut>" to a menu label if the shortcut is set, otherwise return as-is.
// Tab is the standard Win32 menu separator between label and accelerator column.
inline std::wstring AppendShortcutSuffix(const std::wstring& text, WORD hotkey) {
    std::wstring sc = FormatShortcut(hotkey);
    return sc.empty() ? text : (text + L"\t" + sc);
}

// Expand strftime variables in initial text template.
// %%p is the cursor position marker (removed from output, position stored in outCursorPos).
// %% is a literal percent sign.
// Unknown / incomplete %X sequences are passed through as literals — required
// because MSVC's wcsftime() raises a debug assertion on invalid format specifiers,
// and live preview hits intermediate states like "%" or "%q" while the user types.
inline std::wstring ExpandInitialText(const std::wstring& tmpl, int& outCursorPos) {
    outCursorPos = -1;
    if (tmpl.empty()) return {};

    std::wstring work = tmpl;
    size_t cursorMarker = work.find(L"%%p");
    if (cursorMarker != std::wstring::npos) {
        work.erase(cursorMarker, 3);
    }

    std::time_t now = std::time(nullptr);
    struct tm localTime = {};
    if (localtime_s(&localTime, &now) != 0) {
        // Conversion failed (out-of-range time): keep a zeroed tm so wcsftime
        // stays well-defined rather than reading indeterminate stack contents.
        memset(&localTime, 0, sizeof(localTime));
    }

    // Specifier letters accepted by MSVC wcsftime (with or without #/E/O modifier).
    static const wchar_t* kValidSpec =
        L"aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ";

    auto isValidSpec = [&](wchar_t c) -> bool {
        for (const wchar_t* p = kValidSpec; *p; ++p) if (*p == c) return true;
        return false;
    };

    auto expandPiece = [&](const std::wstring& piece) -> std::wstring {
        std::wstring result;
        result.reserve(piece.size() * 2);
        size_t i = 0;
        while (i < piece.size()) {
            if (piece[i] != L'%') { result += piece[i++]; continue; }

            // Lone '%' at end of string — keep literal.
            if (i + 1 >= piece.size()) { result += L'%'; ++i; continue; }

            wchar_t next = piece[i + 1];

            // Escaped percent.
            if (next == L'%') { result += L'%'; i += 2; continue; }

            // Optional '#' modifier (the only one MSVC plain wcsftime accepts
            // safely; E/O are locale-only and assert in debug builds).
            size_t letterIdx = i + 1;
            if (next == L'#') {
                if (i + 2 >= piece.size()) {
                    result.append(piece, i, 2);
                    i += 2;
                    continue;
                }
                letterIdx = i + 2;
            }
            wchar_t letter = piece[letterIdx];
            if (!isValidSpec(letter)) {
                // Unknown specifier — keep the original characters literal.
                result.append(piece, i, letterIdx - i + 1);
                i = letterIdx + 1;
                continue;
            }

            // Valid specifier — call wcsftime on this single token. The token is
            // already validated, so len==0 means a legitimately-empty result
            // (e.g. %p in a 24h locale), not a failure — append it verbatim.
            std::wstring token = piece.substr(i, letterIdx - i + 1);
            wchar_t buf[256];
            size_t len = wcsftime(buf, 256, token.c_str(), &localTime);
            result.append(buf, len);
            i = letterIdx + 1;
        }
        return result;
    };

    if (cursorMarker != std::wstring::npos) {
        std::wstring expandedBefore = expandPiece(work.substr(0, cursorMarker));
        std::wstring expandedAfter  = expandPiece(work.substr(cursorMarker));
        outCursorPos = static_cast<int>(expandedBefore.size());
        return expandedBefore + expandedAfter;
    }

    return expandPiece(work);
}

// URL span found in text (character offsets)
struct UrlSpan {
    int start;           // Start index in text
    int end;             // One past the last character of the URL display range
    std::wstring url;    // URL to open (with http:// prepended for www.)
};

// Find all URLs in text. Detects http://, https://, ftp://, www. prefixes.
inline std::vector<UrlSpan> FindUrls(const std::wstring& text) {
    std::vector<UrlSpan> result;
    int len = static_cast<int>(text.size());
    int pos = 0;

    while (pos < len) {
        // Check for URL prefix at current position
        bool hasScheme = false;
        int prefixLen = 0;

        if (pos + 7 <= len && _wcsnicmp(&text[pos], L"http://", 7) == 0) {
            hasScheme = true;
            prefixLen = 7;
        } else if (pos + 8 <= len && _wcsnicmp(&text[pos], L"https://", 8) == 0) {
            hasScheme = true;
            prefixLen = 8;
        } else if (pos + 6 <= len && _wcsnicmp(&text[pos], L"ftp://", 6) == 0) {
            hasScheme = true;
            prefixLen = 6;
        } else if (pos + 4 <= len && _wcsnicmp(&text[pos], L"www.", 4) == 0) {
            hasScheme = false;
            prefixLen = 4;
        }

        if (prefixLen == 0) {
            ++pos;
            continue;
        }

        int start = pos;
        int end = pos + prefixLen;

        // Consume until whitespace or end
        while (end < len && text[end] != L' ' && text[end] != L'\t' &&
               text[end] != L'\r' && text[end] != L'\n')
            ++end;

        // Strip trailing punctuation that likely belongs to the sentence
        while (end > start + prefixLen) {
            wchar_t ch = text[end - 1];
            if (ch == L'.' || ch == L',' || ch == L')' || ch == L'>' ||
                ch == L']' || ch == L'!' || ch == L'?' || ch == L';' ||
                ch == L'\'' || ch == L'"')
                --end;
            else
                break;
        }

        // Only accept if there's content beyond the prefix
        if (end > start + prefixLen) {
            UrlSpan span;
            span.start = start;
            span.end = end;
            std::wstring raw = text.substr(start, end - start);
            span.url = hasScheme ? raw : (L"http://" + raw);
            result.push_back(std::move(span));
            pos = end;
        } else {
            pos = start + 1;
        }
    }

    return result;
}

// Get the directory containing the running EXE
inline std::wstring GetExeDirectory() {
    // Grow the buffer until the full path fits — GetModuleFileNameW returns the
    // buffer size (not the real length) on truncation. A path >= MAX_PATH would
    // otherwise silently truncate and the app would read/write its files in the
    // wrong directory (start empty, saves fail).
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD len = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
        if (len == 0) return L".";
        if (len < buf.size()) {
            buf.resize(len);
            auto pos = buf.find_last_of(L"\\/");
            if (pos != std::wstring::npos) buf.resize(pos);
            return buf;
        }
        if (buf.size() >= 32768) return L".";   // NT path limit; give up
        buf.resize(buf.size() * 2);
    }
}
