#include "NoteWindow.h"
#include "Application.h"
#include "FindInNoteDialog.h"
#include "AlarmConfigDialog.h"
#include "SettingsDialog.h"
#include "NoteListWindow.h"
#include "Localization.h"
#include "Utils.h"
#include "Resource.h"
#include <windowsx.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <cwctype>
#include <ctime>

static const wchar_t* NOTE_WND_CLASS = L"UltraNoteWindow";

// ============================================================================
// Registration and construction
// ============================================================================

bool NoteWindow::RegisterWindowClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_DBLCLKS | CS_DROPSHADOW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = NOTE_WND_CLASS;
    wc.cbWndExtra    = sizeof(NoteWindow*);
    return RegisterClassExW(&wc) != 0;
}

NoteWindow::NoteWindow(NoteData* data, HINSTANCE hInst)
    : m_data(data), m_hInst(hInst) {
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (data->layout.alwaysOnTop)
        exStyle |= WS_EX_TOPMOST;

    m_hwnd = CreateWindowExW(
        exStyle,
        NOTE_WND_CLASS,
        nullptr,
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        data->x, data->y, data->width, data->height,
        nullptr, nullptr, hInst, this
    );

    // Accept drag & drop files
    DragAcceptFiles(m_hwnd, TRUE);

    // Create tooltip control for attachment paths
    m_hTooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        0, 0, 0, 0,
        m_hwnd, nullptr, hInst, nullptr
    );
    if (m_hTooltip) {
        TTTOOLINFOW ti = {};
        ti.cbSize   = sizeof(ti);
        ti.uFlags   = TTF_SUBCLASS;
        ti.hwnd     = m_hwnd;
        ti.uId      = 1;
        ti.lpszText = const_cast<wchar_t*>(L"");
        SetRectEmpty(&ti.rect);
        SendMessageW(m_hTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        SendMessageW(m_hTooltip, TTM_SETMAXTIPWIDTH, 0, 500);
    }
}

NoteWindow::~NoteWindow() {
    DestroyAttachmentIcons();
    if (m_hEditBrush) {
        DeleteObject(m_hEditBrush);
        m_hEditBrush = nullptr;
    }
    if (m_hTooltip) {
        DestroyWindow(m_hTooltip);
        m_hTooltip = nullptr;
    }
    if (m_hwnd) {
        // Clear the stored pointer before destroying
        SetWindowLongPtrW(m_hwnd, 0, 0);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ============================================================================
// Window procedure
// ============================================================================

LRESULT CALLBACK NoteWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NoteWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<NoteWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, 0, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<NoteWindow*>(GetWindowLongPtrW(hwnd, 0));
    }

    if (self)
        return self->HandleMessage(msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT NoteWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            Paint(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // We handle background in WM_PAINT

        case WM_DROPFILES: {
            HandleDropFiles(reinterpret_cast<HDROP>(wParam));
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            bool ctrlHeld = (wParam & MK_CONTROL) != 0;

            // Check if click is in attachment bar (works even in edit mode)
            if (m_data->showAttachments && !m_data->attachments.empty()) {
                int attachIdx = AttachmentHitTest(x, y);
                if (attachIdx >= 0) return 0; // Consume, dblclick will open
            }

            if (m_inEditMode) break; // Let edit control handle it

            // Check if click is on a URL link
            {
                int urlIdx = UrlHitTest(x, y);
                if (urlIdx >= 0) {
                    m_pendingUrlClick = urlIdx;
                    return 0;
                }
            }

            int hit = HitTest(x, y);
            if (hit != HTCLIENT) {
                // Edge hit -> resize
                StartResize(hit, x, y);
            } else {
                // Body hit -> select + drag
                if (ctrlHeld) {
                    // Toggle selection
                    if (m_selected) {
                        Application::Get().DeselectNote(m_data->id);
                    } else {
                        Application::Get().SelectNote(m_data->id, true);
                    }
                } else {
                    // Simple click: clear multi-selection, don't select this note
                    // Selection indicator (dashed border) is only for Ctrl+Click multi-select
                    Application::Get().ClearSelection();
                    StartDrag(x, y);
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (m_dragging) {
                UpdateDrag(x, y);
            } else if (m_resizing) {
                UpdateResize(x, y);
            } else {
                // Attachment bar tooltip + hand cursor (works in edit mode too)
                if (m_data->showAttachments && m_hTooltip) {
                    int attachIdx = AttachmentHitTest(x, y);
                    if (attachIdx >= 0 && attachIdx < static_cast<int>(m_data->attachments.size())) {
                        TTTOOLINFOW ti = {};
                        ti.cbSize   = sizeof(ti);
                        ti.hwnd     = m_hwnd;
                        ti.uId      = 1;
                        ti.lpszText = const_cast<wchar_t*>(m_data->attachments[attachIdx].c_str());
                        SendMessageW(m_hTooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
                        SendMessageW(m_hTooltip, TTM_ACTIVATE, TRUE, 0);
                        SetCursor(LoadCursorW(nullptr, IDC_HAND));
                        return 0;
                    }
                    SendMessageW(m_hTooltip, TTM_ACTIVATE, FALSE, 0);
                }

                // Normal cursor handling (only outside edit mode)
                if (!m_inEditMode) {
                    // Check for URL hover (hand cursor)
                    if (UrlHitTest(x, y) >= 0) {
                        SetCursor(LoadCursorW(nullptr, IDC_HAND));
                        return 0;
                    }

                    int hit = HitTest(x, y);
                    switch (hit) {
                        case HTLEFT: case HTRIGHT:
                            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE)); break;
                        case HTTOP: case HTBOTTOM:
                            SetCursor(LoadCursorW(nullptr, IDC_SIZENS)); break;
                        case HTTOPLEFT: case HTBOTTOMRIGHT:
                            SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE)); break;
                        case HTTOPRIGHT: case HTBOTTOMLEFT:
                            SetCursor(LoadCursorW(nullptr, IDC_SIZENESW)); break;
                        default:
                            SetCursor(LoadCursorW(nullptr, IDC_ARROW)); break;
                    }
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Handle pending URL click (set in WM_LBUTTONDOWN)
            if (m_pendingUrlClick >= 0) {
                int urlIdx = m_pendingUrlClick;
                m_pendingUrlClick = -1;
                if (urlIdx < static_cast<int>(m_urlRects.size()) && UrlHitTest(x, y) == urlIdx) {
                    ShellExecuteW(m_hwnd, L"open", m_urlRects[urlIdx].url.c_str(),
                                  nullptr, nullptr, SW_SHOWNORMAL);
                }
                return 0;
            }

            if (m_dragging) {
                UpdateDrag(x, y);
                EndDrag();
            } else if (m_resizing) {
                UpdateResize(x, y);
                EndResize();
            } else if (m_data->showAttachments) {
                // Single click on attachment opens file
                int attachIdx = AttachmentHitTest(x, y);
                if (attachIdx >= 0) {
                    OpenAttachment(attachIdx);
                    return 0;
                }
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            m_pendingUrlClick = -1;  // Cancel pending URL click
            if (!m_inEditMode) {
                Application::Get().ClearSelection();
                EnterEditMode();
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            POINT pt = { x, y };
            ClientToScreen(m_hwnd, &pt);

            // Check if right-click is on an attachment
            if (m_data->showAttachments) {
                int attachIdx = AttachmentHitTest(x, y);
                if (attachIdx >= 0) {
                    ShowAttachmentContextMenu(attachIdx, pt.x, pt.y);
                    return 0;
                }
            }

            Application::Get().ClearSelection();
            ShowContextMenu(pt.x, pt.y);
            return 0;
        }

        case WM_KEYDOWN: {
            if (m_inEditMode) break; // Let edit control handle

            // Hardcoded shortcuts (not configurable)
            if (wParam == VK_RETURN) {
                EnterEditMode();
                return 0;
            }
            if (wParam == VK_F2) {
                HandleMenuCommand(ID_NOTE_RENAME);
                return 0;
            }

            // Configurable shortcuts from settings
            {
                auto settings = SettingsDialog::LoadFromStorage();

                if (MatchesShortcut(settings.shortcuts[SC_DELETE], wParam)) {
                    HWND appWnd = FindWindowW(L"UltraNoteApp", L"UltraNote");
                    if (appWnd)
                        PostMessageW(appWnd, WM_NOTE_REQUEST_DELETE,
                                     static_cast<WPARAM>(m_data->id), 0);
                    return 0;
                }

                if (MatchesShortcut(settings.shortcuts[SC_ALWAYS_ON_TOP], wParam)) {
                    m_data->layout.alwaysOnTop = !m_data->layout.alwaysOnTop;
                    SetWindowPos(m_hwnd, m_data->layout.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    NotifyChanged();
                    return 0;
                }
            }
            break;
        }

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && !m_inEditMode) {
                // We set cursor ourselves in WM_MOUSEMOVE
                return TRUE;
            }
            break;

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdcEdit, m_data->layout.backgroundColor);
            SetTextColor(hdcEdit, m_data->layout.textColor);

            if (m_hEditBrush) DeleteObject(m_hEditBrush);
            m_hEditBrush = CreateSolidBrush(m_data->layout.backgroundColor);
            return reinterpret_cast<LRESULT>(m_hEditBrush);
        }

        case WM_SIZE: {
            // Resize edit control if in edit mode
            if (m_hEditCtrl) {
                RECT rc;
                GetClientRect(m_hwnd, &rc);
                int abHeight = GetAttachmentBarHeight();
                MoveWindow(m_hEditCtrl,
                           TEXT_PADDING, TEXT_PADDING,
                           rc.right - 2 * TEXT_PADDING,
                           rc.bottom - 2 * TEXT_PADDING - abHeight,
                           TRUE);
            }
            // Repaint entire window (attachment bar moves with bottom edge)
            InvalidateRect(m_hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_DESTROY:
            return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

// ============================================================================
// Painting
// ============================================================================

void NoteWindow::Paint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    // Double-buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    PaintBackground(memDC, rc);

    if (!m_inEditMode) {
        PaintText(memDC, rc);
    }

    PaintAttachmentBar(memDC, rc);
    PaintBorder(memDC, rc);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void NoteWindow::PaintBackground(HDC hdc, const RECT& rc) {
    HBRUSH hBrush = CreateSolidBrush(m_data->layout.backgroundColor);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);
}

void NoteWindow::PaintBorder(HDC hdc, const RECT& rc) {
    // Always draw solid border in the configured border color
    HPEN pen = CreatePen(PS_SOLID, 1, m_data->layout.borderColor);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    if (m_selected) {
        // Selection indicator: dotted inner rectangle with contrasting color
        COLORREF selColor = RGB(0, 0, 0);
        // Use white dots if border is dark
        int brightness = GetRValue(m_data->layout.borderColor) +
                         GetGValue(m_data->layout.borderColor) +
                         GetBValue(m_data->layout.borderColor);
        if (brightness < 384) selColor = RGB(255, 255, 255);

        LOGBRUSH lb = {};
        lb.lbStyle = BS_SOLID;
        lb.lbColor = selColor;
        HPEN dotPen = ExtCreatePen(PS_COSMETIC | PS_DOT, 1, &lb, 0, nullptr);
        HGDIOBJ oldPen2 = SelectObject(hdc, dotPen);
        HGDIOBJ oldBrush2 = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2);
        SelectObject(hdc, oldBrush2);
        SelectObject(hdc, oldPen2);
        DeleteObject(dotPen);
    }
}

void NoteWindow::PaintText(HDC hdc, const RECT& rc) {
    if (m_data->text.empty()) {
        m_urlRects.clear();
        return;
    }

    HFONT hFont = CreateFontFromParams(m_data->layout.fontFace,
                                        m_data->layout.fontSizePts,
                                        m_data->layout.fontBold,
                                        m_data->layout.fontItalic, hdc);
    GdiSelect fontSel(hdc, hFont);

    SetBkMode(hdc, TRANSPARENT);

    int abHeight = GetAttachmentBarHeight();
    RECT textRc = {
        rc.left + TEXT_PADDING,
        rc.top + TEXT_PADDING,
        rc.right - TEXT_PADDING,
        rc.bottom - TEXT_PADDING - abHeight
    };

    bool linksEnabled = Application::Get().AreClickableLinksEnabled();
    bool hasUrls = linksEnabled && !FindUrls(m_data->text).empty();
    bool hasHighlight = !Application::Get().GetSearchHighlight().empty();

    if (hasUrls || hasHighlight) {
        PaintTextWithLinks(hdc, textRc);
    } else {
        // Fast path: DrawTextW with DT_EDITCONTROL matches EDIT control rendering
        // exactly, avoiding sub-pixel drift between edit and view modes.
        m_urlRects.clear();
        SetTextColor(hdc, m_data->layout.textColor);
        DrawTextW(hdc, m_data->text.c_str(), static_cast<int>(m_data->text.size()),
                  &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
    }

    DeleteObject(hFont);
}

void NoteWindow::PaintTextWithLinks(HDC hdc, const RECT& textRc) {
    m_urlRects.clear();

    const std::wstring& text = m_data->text;
    std::vector<UrlSpan> urls = FindUrls(text);

    // Lowercase copy of text for case-insensitive highlight matching
    const std::wstring& hlTerm = Application::Get().GetSearchHighlight();
    std::wstring textLower, hlLower;
    if (!hlTerm.empty()) {
        textLower.resize(text.size());
        for (size_t i = 0; i < text.size(); ++i) textLower[i] = towlower(text[i]);
        hlLower.resize(hlTerm.size());
        for (size_t i = 0; i < hlTerm.size(); ++i) hlLower[i] = towlower(hlTerm[i]);
    }

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    int maxWidth = textRc.right - textRc.left;

    if (maxWidth <= 0 || lineHeight <= 0) return;

    // Create underlined font for URL segments
    LOGFONTW lf;
    HFONT hCurFont = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));
    GetObjectW(hCurFont, sizeof(lf), &lf);
    lf.lfUnderline = TRUE;
    HFONT hUrlFont = CreateFontIndirectW(&lf);

    int textLen = static_cast<int>(text.size());
    int curY = textRc.top;
    int pos = 0;

    // Helper: find which URL span contains character index 'idx'
    auto findUrl = [&](int idx) -> const UrlSpan* {
        for (auto& u : urls) {
            if (idx >= u.start && idx < u.end)
                return &u;
        }
        return nullptr;
    };

    // Helper: find start of next URL after character index 'from', up to 'limit'
    auto nextUrlStart = [&](int from, int limit) -> int {
        int best = limit;
        for (auto& u : urls) {
            if (u.start > from && u.start < best)
                best = u.start;
        }
        return best;
    };

    // Set clip region to text area
    HRGN hClip = CreateRectRgn(textRc.left, textRc.top, textRc.right, textRc.bottom);
    SelectClipRgn(hdc, hClip);

    // Use TA_UPDATECP so consecutive TextOut calls continue from GDI's internal
    // current position, avoiding sub-pixel drift from manual curX accumulation.
    UINT oldAlign = SetTextAlign(hdc, TA_UPDATECP | TA_LEFT | TA_TOP);

    while (pos < textLen && curY + lineHeight <= textRc.bottom) {
        // Find end of current paragraph (up to \r\n or \n)
        int paraEnd = pos;
        while (paraEnd < textLen && text[paraEnd] != L'\r' && text[paraEnd] != L'\n')
            ++paraEnd;

        // Word-wrap this paragraph into visual lines
        int lineStart = pos;
        if (lineStart == paraEnd) {
            // Empty paragraph — advance one line for the blank line
            curY += lineHeight;
        }
        while (lineStart < paraEnd && curY + lineHeight <= textRc.bottom) {
            int remaining = paraEnd - lineStart;

            // Determine how many characters fit in maxWidth
            int fitCount = 0;
            SIZE sz;
            GetTextExtentExPointW(hdc, &text[lineStart], remaining,
                                  maxWidth, &fitCount, nullptr, &sz);

            int visualLineEnd;
            if (fitCount >= remaining) {
                visualLineEnd = paraEnd;
            } else {
                // Word-break: scan backward from fitCount to find last space
                visualLineEnd = lineStart + fitCount;
                int breakPos = visualLineEnd;
                while (breakPos > lineStart && text[breakPos] != L' ' &&
                       text[breakPos] != L'\t')
                    --breakPos;
                if (breakPos > lineStart) {
                    visualLineEnd = breakPos;
                } else {
                    // No space found — break mid-word at fitCount
                    visualLineEnd = lineStart + fitCount;
                    if (visualLineEnd == lineStart) visualLineEnd++;
                }
            }

            // Draw highlight rectangles behind matching search term (before text)
            if (!hlLower.empty() && hlLower.size() <= static_cast<size_t>(visualLineEnd - lineStart)) {
                int searchFrom = lineStart;
                while (searchFrom + static_cast<int>(hlLower.size()) <= visualLineEnd) {
                    size_t found = textLower.find(hlLower, searchFrom);
                    if (found == std::wstring::npos ||
                        static_cast<int>(found) + static_cast<int>(hlLower.size()) > visualLineEnd)
                        break;
                    int mStart = static_cast<int>(found);
                    int mEnd = mStart + static_cast<int>(hlLower.size());

                    SIZE szPre = {}, szMatch = {};
                    if (mStart > lineStart)
                        GetTextExtentPoint32W(hdc, &text[lineStart],
                                              mStart - lineStart, &szPre);
                    GetTextExtentPoint32W(hdc, &text[mStart], mEnd - mStart, &szMatch);

                    RECT hlRc = {
                        textRc.left + szPre.cx, curY,
                        textRc.left + szPre.cx + szMatch.cx, curY + lineHeight
                    };
                    HBRUSH hHlBrush = CreateSolidBrush(Application::Get().GetSearchHighlightColor());
                    FillRect(hdc, &hlRc, hHlBrush);
                    DeleteObject(hHlBrush);

                    searchFrom = mEnd;
                }
            }

            // Position GDI current point at start of visual line
            MoveToEx(hdc, textRc.left, curY, nullptr);
            int segStart = lineStart;

            while (segStart < visualLineEnd) {
                const UrlSpan* activeUrl = findUrl(segStart);
                int segEnd = activeUrl
                    ? (std::min)(activeUrl->end, visualLineEnd)
                    : nextUrlStart(segStart, visualLineEnd);
                int segLen = segEnd - segStart;

                POINT cpBefore;
                GetCurrentPositionEx(hdc, &cpBefore);

                if (activeUrl) {
                    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, hUrlFont));
                    SetTextColor(hdc, RGB(0, 0, 238));
                    TextOutW(hdc, 0, 0, &text[segStart], segLen);
                    SelectObject(hdc, oldFont);

                    POINT cpAfter;
                    GetCurrentPositionEx(hdc, &cpAfter);
                    RECT urlRect = { cpBefore.x, curY, cpAfter.x, curY + lineHeight };
                    m_urlRects.push_back({ urlRect, activeUrl->url });
                } else {
                    SetTextColor(hdc, m_data->layout.textColor);
                    TextOutW(hdc, 0, 0, &text[segStart], segLen);
                }

                segStart = segEnd;
            }

            curY += lineHeight;

            // Advance past the visual line, skip leading spaces for next line
            lineStart = visualLineEnd;
            if (lineStart < paraEnd && (text[lineStart] == L' ' || text[lineStart] == L'\t'))
                ++lineStart;
        }

        // Skip paragraph break (\r\n or \n or \r)
        pos = paraEnd;
        if (pos < textLen && text[pos] == L'\r') ++pos;
        if (pos < textLen && text[pos] == L'\n') ++pos;
    }

    SetTextAlign(hdc, oldAlign);

    // Restore clip region
    SelectClipRgn(hdc, nullptr);
    DeleteObject(hClip);
    DeleteObject(hUrlFont);
}

// ============================================================================
// Hit testing
// ============================================================================

int NoteWindow::HitTest(int x, int y) const {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    bool nearLeft   = x < RESIZE_BORDER;
    bool nearRight  = x >= rc.right - RESIZE_BORDER;
    bool nearTop    = y < RESIZE_BORDER;
    bool nearBottom = y >= rc.bottom - RESIZE_BORDER;

    if (nearTop && nearLeft)     return HTTOPLEFT;
    if (nearTop && nearRight)    return HTTOPRIGHT;
    if (nearBottom && nearLeft)  return HTBOTTOMLEFT;
    if (nearBottom && nearRight) return HTBOTTOMRIGHT;
    if (nearLeft)                return HTLEFT;
    if (nearRight)               return HTRIGHT;
    if (nearTop)                 return HTTOP;
    if (nearBottom)              return HTBOTTOM;

    return HTCLIENT;
}

// ============================================================================
// Drag (move)
// ============================================================================

void NoteWindow::StartDrag(int x, int y) {
    m_dragging = true;
    // Use screen coordinates for stable drag tracking
    POINT screenPt = { x, y };
    ClientToScreen(m_hwnd, &screenPt);
    m_dragStartCursor = screenPt;
    m_dragStartPos = { m_data->x, m_data->y };
    SetCapture(m_hwnd);
}

void NoteWindow::UpdateDrag(int x, int y) {
    // Convert to screen coordinates
    POINT screenPt = { x, y };
    ClientToScreen(m_hwnd, &screenPt);

    int newX = m_dragStartPos.x + (screenPt.x - m_dragStartCursor.x);
    int newY = m_dragStartPos.y + (screenPt.y - m_dragStartCursor.y);

    int dx = newX - m_data->x;
    int dy = newY - m_data->y;

    if (dx == 0 && dy == 0) return;

    // Move this window
    m_data->x = newX;
    m_data->y = newY;
    SetWindowPos(m_hwnd, nullptr, newX, newY, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Move other selected notes by the same delta
    Application::Get().MoveSelectedNotes(dx, dy, m_data->id);
}

void NoteWindow::EndDrag() {
    m_dragging = false;
    ReleaseCapture();
    SyncDataFromWindow();
    NotifyChanged();
}

// ============================================================================
// Resize
// ============================================================================

void NoteWindow::StartResize(int hitZone, int x, int y) {
    m_resizing = true;
    m_resizeHitZone = hitZone;
    // Use screen coordinates for stable resize tracking
    POINT screenPt = { x, y };
    ClientToScreen(m_hwnd, &screenPt);
    m_resizeStartCursor = screenPt;
    GetWindowRect(m_hwnd, &m_resizeStartRect);
    SetCapture(m_hwnd);
}

void NoteWindow::UpdateResize(int x, int y) {
    POINT screenPt = { x, y };
    ClientToScreen(m_hwnd, &screenPt);

    int dx = screenPt.x - m_resizeStartCursor.x;
    int dy = screenPt.y - m_resizeStartCursor.y;

    RECT r = m_resizeStartRect;

    switch (m_resizeHitZone) {
        case HTLEFT:        r.left += dx; break;
        case HTRIGHT:       r.right += dx; break;
        case HTTOP:         r.top += dy; break;
        case HTBOTTOM:      r.bottom += dy; break;
        case HTTOPLEFT:     r.left += dx; r.top += dy; break;
        case HTTOPRIGHT:    r.right += dx; r.top += dy; break;
        case HTBOTTOMLEFT:  r.left += dx; r.bottom += dy; break;
        case HTBOTTOMRIGHT: r.right += dx; r.bottom += dy; break;
    }

    // Enforce minimum size
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if (w < MIN_WIDTH) {
        if (m_resizeHitZone == HTLEFT || m_resizeHitZone == HTTOPLEFT || m_resizeHitZone == HTBOTTOMLEFT)
            r.left = r.right - MIN_WIDTH;
        else
            r.right = r.left + MIN_WIDTH;
    }
    if (h < MIN_HEIGHT) {
        if (m_resizeHitZone == HTTOP || m_resizeHitZone == HTTOPLEFT || m_resizeHitZone == HTTOPRIGHT)
            r.top = r.bottom - MIN_HEIGHT;
        else
            r.bottom = r.top + MIN_HEIGHT;
    }

    SetWindowPos(m_hwnd, nullptr, r.left, r.top,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void NoteWindow::EndResize() {
    m_resizing = false;
    ReleaseCapture();
    SyncDataFromWindow();
    NotifyChanged();
}

// ============================================================================
// Edit mode
// ============================================================================

void NoteWindow::EnterEditMode() {
    if (m_inEditMode) return;
    m_inEditMode = true;

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    int abHeight = GetAttachmentBarHeight();
    m_hEditCtrl = CreateWindowExW(
        0, L"EDIT", m_data->text.c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_WANTRETURN |
        ES_AUTOVSCROLL | ES_NOHIDESEL,
        TEXT_PADDING, TEXT_PADDING,
        rc.right - 2 * TEXT_PADDING,
        rc.bottom - 2 * TEXT_PADDING - abHeight,
        m_hwnd, nullptr, m_hInst, nullptr
    );

    // Set font using window DC for consistent DPI-aware metrics (must match PaintText)
    HDC hdc = GetDC(m_hwnd);
    HFONT hFont = CreateFontFromParams(m_data->layout.fontFace,
                                        m_data->layout.fontSizePts,
                                        m_data->layout.fontBold,
                                        m_data->layout.fontItalic, hdc);
    ReleaseDC(m_hwnd, hdc);
    SendMessageW(m_hEditCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    // Remove internal edit margins so text doesn't shift vs. owner-draw rendering
    SendMessageW(m_hEditCtrl, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
    // Force format rect to full client rect so word-wrap width matches DrawText
    // (EDIT control otherwise reserves implicit caret space at the right edge,
    //  causing wrap to happen ~2px earlier than in PaintText/DT_EDITCONTROL).
    {
        RECT fmt;
        GetClientRect(m_hEditCtrl, &fmt);
        SendMessageW(m_hEditCtrl, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&fmt));
    }

    // Subclass for CTRL+ENTER and ESC
    SetWindowSubclass(m_hEditCtrl, EditSubclassProc, EDIT_SUBCLASS_ID,
                      reinterpret_cast<DWORD_PTR>(this));

    // Position cursor: active search highlight wins, else %%p marker, else end.
    const std::wstring& hlTerm = Application::Get().GetSearchHighlight();
    int hlMatchPos = -1;
    if (!hlTerm.empty()) {
        std::wstring textLower(m_data->text.size(), L'\0');
        std::wstring hlLower(hlTerm.size(), L'\0');
        for (size_t i = 0; i < m_data->text.size(); ++i) textLower[i] = towlower(m_data->text[i]);
        for (size_t i = 0; i < hlTerm.size(); ++i) hlLower[i] = towlower(hlTerm[i]);
        size_t found = textLower.find(hlLower);
        if (found != std::wstring::npos) hlMatchPos = static_cast<int>(found);
    }

    if (hlMatchPos >= 0) {
        SendMessageW(m_hEditCtrl, EM_SETSEL,
                     static_cast<WPARAM>(hlMatchPos),
                     static_cast<LPARAM>(hlMatchPos + hlTerm.size()));
        SendMessageW(m_hEditCtrl, EM_SCROLLCARET, 0, 0);
    } else if (m_data->cursorPos >= 0) {
        SendMessageW(m_hEditCtrl, EM_SETSEL,
                     static_cast<WPARAM>(m_data->cursorPos),
                     static_cast<LPARAM>(m_data->cursorPos));
        m_data->cursorPos = -1;  // One-time positioning
    } else {
        // Place cursor at end instead of selecting all text
        int textLen = GetWindowTextLengthW(m_hEditCtrl);
        SendMessageW(m_hEditCtrl, EM_SETSEL, textLen, textLen);
        SendMessageW(m_hEditCtrl, EM_SCROLLCARET, 0, 0);
    }
    SetFocus(m_hEditCtrl);

    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void NoteWindow::ExitEditMode(bool save) {
    if (!m_inEditMode || !m_hEditCtrl) return;

    if (save) {
        int len = GetWindowTextLengthW(m_hEditCtrl);
        std::wstring text(static_cast<size_t>(len), L'\0');
        if (len > 0)
            GetWindowTextW(m_hEditCtrl, &text[0], len + 1);
        m_data->text = std::move(text);
        m_data->modifiedAt = static_cast<int64_t>(std::time(nullptr));
        NotifyChanged();
    }

    // Get the font to delete it
    HFONT hFont = reinterpret_cast<HFONT>(SendMessageW(m_hEditCtrl, WM_GETFONT, 0, 0));

    RemoveWindowSubclass(m_hEditCtrl, EditSubclassProc, EDIT_SUBCLASS_ID);
    DestroyWindow(m_hEditCtrl);
    m_hEditCtrl = nullptr;

    if (hFont) DeleteObject(hFont);

    m_inEditMode = false;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

LRESULT CALLBACK NoteWindow::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                LPARAM lParam, UINT_PTR /*subclassId*/,
                                                DWORD_PTR refData) {
    auto* self = reinterpret_cast<NoteWindow*>(refData);

    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
                self->ExitEditMode(true);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                self->ExitEditMode(false);
                return 0;
            }
            break;

        case WM_CONTEXTMENU: {
            // Show the edit-mode context menu (Paste / Select All / Cut / Copy /
            // Delete / Date-Time / Insert file path) instead of the default
            // EDIT control menu or the outer note context menu.
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            // Keyboard-driven WM_CONTEXTMENU sends (-1, -1). Fall back to the
            // caret position so the menu appears near the text cursor.
            if (pt.x == -1 && pt.y == -1) {
                POINT caret = {};
                if (GetCaretPos(&caret)) {
                    ClientToScreen(hwnd, &caret);
                    pt = caret;
                } else {
                    RECT rc;
                    GetWindowRect(hwnd, &rc);
                    pt.x = rc.left + 8;
                    pt.y = rc.top + 8;
                }
            }
            self->ShowEditContextMenu(pt.x, pt.y);
            return 0;
        }

        case WM_KILLFOCUS: {
            // Auto-save when focus leaves the edit control — but not when it
            // goes to our own in-note find dialog, or the edit mode would be
            // torn down between each "Find Next" click and the search cursor
            // would reset to the top on every click. Also suppressed while a
            // common dialog from the edit-context-menu (Insert path…) is open.
            if (self->m_editModalOpen) break;
            HWND newFocus = reinterpret_cast<HWND>(wParam);
            HWND findHwnd = self->m_findDialog ? self->m_findDialog->GetHwnd()
                                               : nullptr;
            bool focusInFindDialog = findHwnd && newFocus &&
                (newFocus == findHwnd || IsChild(findHwnd, newFocus));
            if (newFocus != hwnd && !IsChild(self->m_hwnd, newFocus) &&
                newFocus != self->m_hwnd && !focusInFindDialog) {
                self->ExitEditMode(true);
                return 0;
            }
            break;
        }
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Context menu
// ============================================================================

void NoteWindow::ShowContextMenu(int screenX, int screenY) {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    auto& app = Application::Get();

    auto addItem = [&](UINT id, const wchar_t* text, SHSTOCKICONID iconId, UINT flags = 0) {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE | MIIM_STATE;
        mii.fType      = MFT_STRING;
        mii.fState     = (flags & MF_GRAYED) ? MFS_GRAYED : MFS_ENABLED;
        if (flags & MF_CHECKED) mii.fState |= MFS_CHECKED;
        mii.wID        = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);
        mii.hbmpItem   = app.GetMenuBitmap(iconId);
        InsertMenuItemW(hPopup, GetMenuItemCount(hPopup), TRUE, &mii);
    };
    auto addItemRes = [&](UINT id, const wchar_t* text, UINT iconResId, UINT flags = 0) {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE | MIIM_STATE;
        mii.fType      = MFT_STRING;
        mii.fState     = (flags & MF_GRAYED) ? MFS_GRAYED : MFS_ENABLED;
        if (flags & MF_CHECKED) mii.fState |= MFS_CHECKED;
        mii.wID        = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);
        mii.hbmpItem   = app.GetResourceBitmap(iconResId);
        InsertMenuItemW(hPopup, GetMenuItemCount(hPopup), TRUE, &mii);
    };

    addItem(ID_NOTE_EDIT,   Ls(L"note.edit").c_str(),   SIID_RENAME);
    addItem(ID_NOTE_RENAME, Ls(L"note.rename").c_str(), SIID_DOCASSOC);
    addItemRes(ID_NOTE_COPY, Ls(L"note.copy").c_str(), IDI_COPY);
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    // "Set Folder" submenu
    HMENU hFolderSub = CreatePopupMenu();
    if (hFolderSub) {
        UINT noFolderFlags = MF_STRING;
        if (m_data->folder.empty()) noFolderFlags |= MF_CHECKED;
        AppendMenuW(hFolderSub, noFolderFlags, ID_NL_FOLDER_BASE, Ls(L"note.no_folder").c_str());
        auto& folders = app.GetFolders();
        if (!folders.empty())
            AppendMenuW(hFolderSub, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < folders.size() && i + 1 < (ID_NL_FOLDER_MAX - ID_NL_FOLDER_BASE); ++i) {
            UINT flags = MF_STRING;
            if (m_data->folder == folders[i]) flags |= MF_CHECKED;
            AppendMenuW(hFolderSub, flags,
                        ID_NL_FOLDER_BASE + static_cast<UINT>(i + 1),
                        folders[i].c_str());
        }
        {
            MENUITEMINFOW mii = {};
            mii.cbSize     = sizeof(mii);
            mii.fMask      = MIIM_STRING | MIIM_SUBMENU | MIIM_BITMAP;
            mii.hSubMenu   = hFolderSub;
            std::wstring folderLabel = Ls(L"notelist.set_folder");
            mii.dwTypeData = const_cast<wchar_t*>(folderLabel.c_str());
            mii.hbmpItem   = app.GetResourceBitmap(IDI_FOLDER);
            InsertMenuItemW(hPopup, GetMenuItemCount(hPopup), TRUE, &mii);
        }
    }

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    addItemRes(ID_NOTE_HIDE, Ls(L"note.hide").c_str(), IDI_HIDE_ALL);
    addItemRes(ID_NOTE_DELETE, Ls(L"note.delete").c_str(), IDI_DELETE);
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    addItemRes(ID_NOTE_ALWAYSONTOP, Ls(L"note.always_on_top").c_str(), IDI_PIN,
               m_data->layout.alwaysOnTop ? MF_CHECKED : 0);
    addItemRes(ID_NOTE_ATTACHMENTS, Ls(L"note.attachments").c_str(), IDI_ATTACHMENT,
               m_data->showAttachments ? MF_CHECKED : 0);
    addItemRes(ID_NOTE_ALARM, Ls(L"note.alarm").c_str(), IDI_ALARM,
               m_data->alarm.has_value() ? MF_CHECKED : 0);
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    addItemRes(ID_NOTE_SEARCH, Ls(L"note.search").c_str(), IDI_SEARCH);
    addItemRes(ID_NOTE_NEWNOTE, Ls(L"note.new_note").c_str(), IDI_NEW);

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hPopup,
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(hPopup);

    if (cmd)
        HandleMenuCommand(cmd);
}

void NoteWindow::HandleMenuCommand(int cmd) {
    switch (cmd) {
        case ID_NOTE_EDIT:
            EnterEditMode();
            break;

        case ID_NOTE_RENAME: {
            // Share the same input dialog as the note-list Rename flow so both
            // entry points look and behave identically.
            std::wstring value = m_data->title;
            if (NoteListWindow::InputDialog(m_hwnd, Ls(L"note.enter_title"),
                                             L"UltraNote", value)) {
                Application::Get().RenameNote(m_data->id, value);
            }
            break;
        }

        case ID_NOTE_DELETE: {
            HWND appWnd = FindWindowW(L"UltraNoteApp", L"UltraNote");
            if (appWnd)
                PostMessageW(appWnd, WM_NOTE_REQUEST_DELETE,
                             static_cast<WPARAM>(m_data->id), 0);
            break;
        }

        case ID_NOTE_ALWAYSONTOP:
            m_data->layout.alwaysOnTop = !m_data->layout.alwaysOnTop;
            SetWindowPos(m_hwnd,
                         m_data->layout.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            NotifyChanged();
            break;

        case ID_NOTE_ATTACHMENTS:
            m_data->showAttachments = !m_data->showAttachments;
            DestroyAttachmentIcons();
            InvalidateRect(m_hwnd, nullptr, TRUE);
            // Re-layout edit control if active
            if (m_hEditCtrl) {
                RECT rc;
                GetClientRect(m_hwnd, &rc);
                int abHeight = GetAttachmentBarHeight();
                MoveWindow(m_hEditCtrl, TEXT_PADDING, TEXT_PADDING,
                           rc.right - 2 * TEXT_PADDING,
                           rc.bottom - 2 * TEXT_PADDING - abHeight, TRUE);
            }
            NotifyChanged();
            break;

        case ID_NOTE_NEWNOTE:
            Application::Get().CreateNewNote();
            break;

        case ID_NOTE_SEARCH:
            OpenFindDialog();
            break;

        case ID_NOTE_ALARM:
            OpenAlarmDialog();
            break;

        case ID_NOTE_HIDE:
            m_data->isHidden = true;
            Show(false);
            NotifyChanged();
            Application::Get().RefreshNoteList();
            break;

        case ID_NOTE_COPY:
            if (OpenClipboard(m_hwnd)) {
                EmptyClipboard();
                size_t size = (m_data->text.size() + 1) * sizeof(wchar_t);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hMem) {
                    wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem));
                    wcscpy_s(dst, m_data->text.size() + 1, m_data->text.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            break;

        default:
            // Folder assignment from submenu
            if (cmd >= ID_NL_FOLDER_BASE && cmd <= ID_NL_FOLDER_MAX) {
                UINT folderIdx = static_cast<UINT>(cmd) - ID_NL_FOLDER_BASE;
                std::wstring targetFolder;
                if (folderIdx > 0) {
                    auto& folders = Application::Get().GetFolders();
                    if (folderIdx - 1 < folders.size())
                        targetFolder = folders[folderIdx - 1];
                }
                Application::Get().SetNoteFolder(m_data->id, targetFolder);
            }
            break;
    }
}

// ============================================================================
// Edit-mode context menu
// ============================================================================

bool NoteWindow::HasEditSelection() const {
    if (!m_hEditCtrl) return false;
    DWORD start = 0, end = 0;
    SendMessageW(m_hEditCtrl, EM_GETSEL,
                 reinterpret_cast<WPARAM>(&start),
                 reinterpret_cast<LPARAM>(&end));
    return end > start;
}

void NoteWindow::InsertAtCursor(const std::wstring& text) {
    if (!m_hEditCtrl) return;
    // Use EM_REPLACESEL with undo (wParam=TRUE). Replaces current selection
    // if any, otherwise inserts at the caret.
    SendMessageW(m_hEditCtrl, EM_REPLACESEL, TRUE,
                 reinterpret_cast<LPARAM>(text.c_str()));
}

void NoteWindow::InsertFilePathAtCursor() {
    if (!m_hEditCtrl) return;

    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = m_hwnd;
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                      OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

    // Guard the edit control from being torn down while the common dialog
    // is modal. See WM_KILLFOCUS in EditSubclassProc.
    m_editModalOpen = true;
    BOOL ok = GetOpenFileNameW(&ofn);
    m_editModalOpen = false;

    if (ok) {
        SetFocus(m_hEditCtrl);
        InsertAtCursor(file);
    }
}

void NoteWindow::ShowEditContextMenu(int screenX, int screenY) {
    if (!m_hEditCtrl) return;  // Only valid while edit mode is active

    bool hasSel      = HasEditSelection();
    bool hasText     = GetWindowTextLengthW(m_hEditCtrl) > 0;
    bool clipHasText = IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    auto& app = Application::Get();

    auto addItem = [&](UINT id, const wchar_t* text, HBITMAP bmp, UINT flags = 0) {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_FTYPE | MIIM_STATE | MIIM_BITMAP;
        mii.fType      = MFT_STRING;
        mii.fState     = (flags & MF_GRAYED) ? MFS_GRAYED : MFS_ENABLED;
        mii.wID        = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);
        mii.hbmpItem   = bmp;
        InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
    };

    // Always-available commands
    addItem(ID_EDIT_PASTE, Ls(L"edit.paste").c_str(),
            app.GetResourceBitmap(IDI_PASTE),
            clipHasText ? 0 : MF_GRAYED);
    addItem(ID_EDIT_SELECT_ALL, Ls(L"edit.select_all").c_str(),
            app.GetResourceBitmap(IDI_SELECT_ALL),
            hasText ? 0 : MF_GRAYED);

    // Selection-dependent commands
    if (hasSel) {
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        addItem(ID_EDIT_CUT,    Ls(L"edit.cut").c_str(),
                app.GetResourceBitmap(IDI_CUT));
        addItem(ID_EDIT_COPY,   Ls(L"edit.copy").c_str(),
                app.GetResourceBitmap(IDI_COPY));
        addItem(ID_EDIT_DELETE, Ls(L"edit.delete").c_str(),
                app.GetResourceBitmap(IDI_DELETE));
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Date/Time submenu — 12 ATnotes-style entries with live preview.
    // Index within s_dt doubles as the offset from ID_EDIT_DATETIME_BASE.
    struct DtEntry {
        const wchar_t* code;    // "%c", "%#c", …
        const wchar_t* format;  // strftime format passed to wcsftime
        const wchar_t* locKey;  // Localization key for descriptive label
        bool           sepBefore;
    };
    static const DtEntry s_dt[] = {
        { L"%c",  L"%c",  L"settings.var_datetime_short", false },
        { L"%#c", L"%#c", L"settings.var_datetime_long",  false },
        { L"%x",  L"%x",  L"settings.var_date_short",     false },
        { L"%#x", L"%#x", L"settings.var_date_long",      false },
        { L"%X",  L"%X",  L"settings.var_time",           false },
        { L"%d",  L"%d",  L"settings.var_day",            true  },
        { L"%m",  L"%m",  L"settings.var_month",          false },
        { L"%y",  L"%y",  L"settings.var_year_short",     false },
        { L"%Y",  L"%Y",  L"settings.var_year_long",      false },
        { L"%H",  L"%H",  L"settings.var_hour24",         true  },
        { L"%M",  L"%M",  L"settings.var_minute",         false },
        { L"%S",  L"%S",  L"settings.var_second",         false },
    };

    HMENU hDtMenu = CreatePopupMenu();
    if (hDtMenu) {
        std::time_t now = std::time(nullptr);
        struct tm localTime;
        localtime_s(&localTime, &now);

        for (int i = 0; i < _countof(s_dt); ++i) {
            if (s_dt[i].sepBefore)
                AppendMenuW(hDtMenu, MF_SEPARATOR, 0, nullptr);

            wchar_t buf[128];
            size_t len = wcsftime(buf, 128, s_dt[i].format, &localTime);
            std::wstring preview = (len > 0) ? std::wstring(buf, len) : L"";

            std::wstring text = s_dt[i].code;
            text += L"  ";
            text += Ls(s_dt[i].locKey);
            text += L"\t";
            text += preview;

            AppendMenuW(hDtMenu, MF_STRING,
                        ID_EDIT_DATETIME_BASE + static_cast<UINT>(i),
                        text.c_str());
        }

        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_STRING | MIIM_SUBMENU | MIIM_BITMAP | MIIM_FTYPE;
        mii.fType      = MFT_STRING;
        mii.hSubMenu   = hDtMenu;
        std::wstring dtLabel = Ls(L"edit.datetime");
        mii.dwTypeData = const_cast<wchar_t*>(dtLabel.c_str());
        mii.hbmpItem   = app.GetResourceBitmap(IDI_DATETIME);
        InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
    }

    addItem(ID_EDIT_INSERT_PATH, Ls(L"edit.insert_path").c_str(),
            app.GetResourceBitmap(IDI_INSERT_PATH));

    int cmd = TrackPopupMenu(hMenu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 0) return;

    switch (cmd) {
        case ID_EDIT_PASTE:
            SendMessageW(m_hEditCtrl, WM_PASTE, 0, 0);
            break;
        case ID_EDIT_SELECT_ALL:
            SendMessageW(m_hEditCtrl, EM_SETSEL, 0, -1);
            break;
        case ID_EDIT_CUT:
            SendMessageW(m_hEditCtrl, WM_CUT, 0, 0);
            break;
        case ID_EDIT_COPY:
            SendMessageW(m_hEditCtrl, WM_COPY, 0, 0);
            break;
        case ID_EDIT_DELETE:
            SendMessageW(m_hEditCtrl, EM_REPLACESEL, TRUE,
                         reinterpret_cast<LPARAM>(L""));
            break;
        case ID_EDIT_INSERT_PATH:
            InsertFilePathAtCursor();
            break;
        default:
            if (cmd >= ID_EDIT_DATETIME_BASE && cmd <= ID_EDIT_DATETIME_MAX) {
                int idx = cmd - ID_EDIT_DATETIME_BASE;
                if (idx >= 0 && idx < static_cast<int>(_countof(s_dt))) {
                    std::time_t now = std::time(nullptr);
                    struct tm localTime;
                    localtime_s(&localTime, &now);
                    wchar_t buf[128];
                    size_t len = wcsftime(buf, 128, s_dt[idx].format, &localTime);
                    if (len > 0) InsertAtCursor(std::wstring(buf, len));
                }
            }
            break;
    }
}

// ============================================================================
// Attachment bar
// ============================================================================

int NoteWindow::GetAttachmentBarHeight() const {
    if (!m_data->showAttachments || m_data->attachments.empty())
        return 0;
    return static_cast<int>(m_data->attachments.size()) * ATTACH_ROW_HEIGHT + 1;  // +1 for separator
}

RECT NoteWindow::GetAttachmentBarRect() const {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int h = GetAttachmentBarHeight();
    // Leave RESIZE_BORDER free at bottom and right for resize grip
    RECT barRc = {
        RESIZE_BORDER,
        rc.bottom - h - RESIZE_BORDER,
        rc.right - RESIZE_BORDER,
        rc.bottom - RESIZE_BORDER
    };
    return barRc;
}

int NoteWindow::AttachmentHitTest(int x, int y) const {
    if (!m_data->showAttachments || m_data->attachments.empty())
        return -1;

    RECT barRc = GetAttachmentBarRect();
    if (x < barRc.left || x >= barRc.right || y < barRc.top || y >= barRc.bottom)
        return -1;

    // Each attachment is one row; find which row was hit
    int rowY = barRc.top + 1;  // skip separator line
    for (int i = 0; i < static_cast<int>(m_data->attachments.size()); ++i) {
        if (y >= rowY && y < rowY + ATTACH_ROW_HEIGHT)
            return i;
        rowY += ATTACH_ROW_HEIGHT;
    }
    return -1;
}

int NoteWindow::UrlHitTest(int x, int y) const {
    POINT pt = { x, y };
    for (int i = 0; i < static_cast<int>(m_urlRects.size()); ++i) {
        if (PtInRect(&m_urlRects[i].rect, pt))
            return i;
    }
    return -1;
}

void NoteWindow::PaintAttachmentBar(HDC hdc, const RECT& /*rc*/) {
    if (!m_data->showAttachments || m_data->attachments.empty())
        return;

    RECT barRc = GetAttachmentBarRect();

    // Draw separator line at top of attachment area
    HPEN sepPen = CreatePen(PS_SOLID, 1, m_data->layout.borderColor);
    HGDIOBJ oldPen = SelectObject(hdc, sepPen);
    MoveToEx(hdc, barRc.left, barRc.top, nullptr);
    LineTo(hdc, barRc.right, barRc.top);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    // Ensure icon cache matches attachments
    if (m_attachIcons.size() != m_data->attachments.size()) {
        DestroyAttachmentIcons();
        for (auto& path : m_data->attachments) {
            SHFILEINFOW sfi = {};
            HICON hIcon = nullptr;
            if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi),
                               SHGFI_ICON | SHGFI_SMALLICON)) {
                hIcon = sfi.hIcon;
            } else {
                sfi = {};
                if (SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                                   SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    hIcon = sfi.hIcon;
                }
            }
            m_attachIcons.push_back(hIcon);
        }
    }

    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFontObj = SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, m_data->layout.textColor);

    int rowY = barRc.top + 1;  // below separator line
    for (size_t i = 0; i < m_data->attachments.size(); ++i) {
        int iconX = barRc.left + ATTACH_ITEM_PAD;
        int iconY = rowY + (ATTACH_ROW_HEIGHT - ATTACH_ICON_SIZE) / 2;

        // Draw icon
        if (i < m_attachIcons.size() && m_attachIcons[i]) {
            DrawIconEx(hdc, iconX, iconY, m_attachIcons[i],
                       ATTACH_ICON_SIZE, ATTACH_ICON_SIZE, 0, nullptr, DI_NORMAL);
        }

        // Draw filename (ellipsis if too wide)
        int textX = iconX + ATTACH_ICON_SIZE + ATTACH_ITEM_PAD;
        const wchar_t* filename = PathFindFileNameW(m_data->attachments[i].c_str());
        RECT textRc = { textX, rowY, barRc.right - ATTACH_ITEM_PAD, rowY + ATTACH_ROW_HEIGHT };
        DrawTextW(hdc, filename, static_cast<int>(wcslen(filename)),
                  &textRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

        rowY += ATTACH_ROW_HEIGHT;
    }

    SelectObject(hdc, oldFontObj);
}

void NoteWindow::HandleDropFiles(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        if (static_cast<int>(m_data->attachments.size()) >= MAX_ATTACHMENTS) {
            MessageBoxW(m_hwnd, Ls(L"note.attach_full").c_str(),
                        L"UltraNote", MB_OK | MB_ICONINFORMATION);
            break;
        }
        UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
        std::wstring path(static_cast<size_t>(len), L'\0');
        DragQueryFileW(hDrop, i, &path[0], len + 1);
        m_data->attachments.push_back(std::move(path));
    }
    DragFinish(hDrop);

    // Auto-show attachment bar if it was hidden
    if (!m_data->attachments.empty() && !m_data->showAttachments) {
        m_data->showAttachments = true;
    }

    DestroyAttachmentIcons();
    InvalidateRect(m_hwnd, nullptr, TRUE);

    // Re-layout edit control if active
    if (m_hEditCtrl) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int abHeight = GetAttachmentBarHeight();
        MoveWindow(m_hEditCtrl, TEXT_PADDING, TEXT_PADDING,
                   rc.right - 2 * TEXT_PADDING,
                   rc.bottom - 2 * TEXT_PADDING - abHeight, TRUE);
    }

    NotifyChanged();
}

void NoteWindow::OpenAttachment(int index) {
    if (index < 0 || index >= static_cast<int>(m_data->attachments.size()))
        return;
    ShellExecuteW(m_hwnd, L"open", m_data->attachments[index].c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
}

void NoteWindow::RemoveAttachment(int index) {
    if (index < 0 || index >= static_cast<int>(m_data->attachments.size()))
        return;
    m_data->attachments.erase(m_data->attachments.begin() + index);
    DestroyAttachmentIcons();
    InvalidateRect(m_hwnd, nullptr, TRUE);

    // Re-layout edit control if active
    if (m_hEditCtrl) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int abHeight = GetAttachmentBarHeight();
        MoveWindow(m_hEditCtrl, TEXT_PADDING, TEXT_PADDING,
                   rc.right - 2 * TEXT_PADDING,
                   rc.bottom - 2 * TEXT_PADDING - abHeight, TRUE);
    }

    NotifyChanged();
}

void NoteWindow::ShowAttachmentContextMenu(int index, int screenX, int screenY) {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    auto& app = Application::Get();

    // Show full path as disabled header
    AppendMenuW(hPopup, MF_STRING | MF_GRAYED, 0, m_data->attachments[index].c_str());
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    // Remove entry with delete icon
    MENUITEMINFOW mii = {};
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP;
    mii.wID        = 1;
    std::wstring removeText = Ls(L"note.attach_remove");
    mii.dwTypeData = const_cast<wchar_t*>(removeText.c_str());
    mii.hbmpItem   = app.GetResourceBitmap(IDI_DELETE);
    InsertMenuItemW(hPopup, GetMenuItemCount(hPopup), TRUE, &mii);

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hPopup, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                             screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(hPopup);

    if (cmd == 1) {
        RemoveAttachment(index);
    }
}

void NoteWindow::DestroyAttachmentIcons() {
    for (HICON icon : m_attachIcons) {
        if (icon) DestroyIcon(icon);
    }
    m_attachIcons.clear();
}

// ============================================================================
// Helpers
// ============================================================================

void NoteWindow::Show(bool show) {
    ShowWindow(m_hwnd, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void NoteWindow::BringToFront() {
    SetWindowPos(m_hwnd,
                 m_data->layout.alwaysOnTop ? HWND_TOPMOST : HWND_TOP,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(m_hwnd);
}

void NoteWindow::SetSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void NoteWindow::OffsetPosition(int dx, int dy) {
    m_data->x += dx;
    m_data->y += dy;
    SetWindowPos(m_hwnd, nullptr, m_data->x, m_data->y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void NoteWindow::SyncDataFromWindow() {
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    m_data->x = rc.left;
    m_data->y = rc.top;
    m_data->width = rc.right - rc.left;
    m_data->height = rc.bottom - rc.top;
}

void NoteWindow::NotifyChanged() {
    HWND appWnd = FindWindowW(L"UltraNoteApp", L"UltraNote");
    if (appWnd)
        PostMessageW(appWnd, WM_NOTE_CHANGED, 0, 0);
}

// ============================================================================
// In-note search
// ============================================================================

void NoteWindow::OpenFindDialog() {
    if (!m_findDialog) {
        m_findDialog = std::make_unique<FindInNoteDialog>(m_hInst, this);
        if (!m_findDialog->Create()) {
            m_findDialog.reset();
            return;
        }
    }
    // Reset search cursor so a freshly opened dialog starts from the top
    m_findSearchPos = 0;
    m_findLastMatchStart = std::wstring::npos;
    m_findLastMatchEnd   = std::wstring::npos;
    m_findDialog->ShowAndFocus();
}

void NoteWindow::OpenAlarmDialog() {
    // Reuse existing dialog if its HWND is still valid; otherwise recreate.
    if (m_alarmDialog && m_alarmDialog->GetHwnd()) {
        SetForegroundWindow(m_alarmDialog->GetHwnd());
        return;
    }
    m_alarmDialog.reset();
    m_alarmDialog = std::make_unique<AlarmConfigDialog>(m_hInst, m_hwnd, m_data->id);
    if (!m_alarmDialog->Create()) {
        m_alarmDialog.reset();
    }
}

bool NoteWindow::FindNextInNote(const std::wstring& term, bool caseSensitive, bool wholeWord) {
    if (term.empty()) return false;

    // Lazily enter edit mode so we can highlight and scroll to matches.
    bool wasInEditMode = m_inEditMode;
    if (!m_inEditMode || !m_hEditCtrl) {
        EnterEditMode();
        m_findSearchPos = 0;
        m_findLastMatchStart = std::wstring::npos;
        m_findLastMatchEnd   = std::wstring::npos;
    }
    if (!m_hEditCtrl) return false;

    int len = GetWindowTextLengthW(m_hEditCtrl);
    if (len <= 0) { MessageBeep(MB_ICONASTERISK); return false; }
    std::wstring text(len, L'\0');
    GetWindowTextW(m_hEditCtrl, text.data(), len + 1);

    // If the user moved the caret manually between searches (selection no
    // longer matches our last found range), restart from the caret position.
    if (wasInEditMode) {
        DWORD selStart = 0, selEnd = 0;
        SendMessageW(m_hEditCtrl, EM_GETSEL,
                     reinterpret_cast<WPARAM>(&selStart),
                     reinterpret_cast<LPARAM>(&selEnd));
        if (static_cast<size_t>(selStart) != m_findLastMatchStart ||
            static_cast<size_t>(selEnd)   != m_findLastMatchEnd) {
            m_findSearchPos = selEnd;
        }
    }

    auto toLower = [](std::wstring s) {
        for (auto& c : s) c = towlower(c);
        return s;
    };

    std::wstring hay = caseSensitive ? text : toLower(text);
    std::wstring nd  = caseSensitive ? term : toLower(term);

    if (m_findSearchPos > hay.size()) m_findSearchPos = hay.size();

    auto isWordChar = [](wchar_t c) { return iswalnum(c) != 0 || c == L'_'; };
    auto isWholeWordHit = [&](size_t p) {
        if (p > 0 && isWordChar(hay[p - 1])) return false;
        size_t end = p + nd.size();
        if (end < hay.size() && isWordChar(hay[end])) return false;
        return true;
    };
    auto findFrom = [&](size_t from) -> size_t {
        size_t p = from;
        while (p <= hay.size()) {
            p = hay.find(nd, p);
            if (p == std::wstring::npos) return std::wstring::npos;
            if (!wholeWord || isWholeWordHit(p)) return p;
            ++p;
        }
        return std::wstring::npos;
    };

    size_t pos = findFrom(m_findSearchPos);
    bool wrapped = false;
    if (pos == std::wstring::npos && m_findSearchPos > 0) {
        pos = findFrom(0);
        wrapped = true;
    }

    if (pos == std::wstring::npos) {
        MessageBeep(MB_ICONASTERISK);
        return false;
    }

    // Scroll viewport to the target line BEFORE setting the selection, so the
    // control doesn't first repaint at the top and then jump back down.
    LRESULT targetLine = SendMessageW(m_hEditCtrl, EM_LINEFROMCHAR,
                                      static_cast<WPARAM>(pos), 0);
    LRESULT firstVisible = SendMessageW(m_hEditCtrl, EM_GETFIRSTVISIBLELINE, 0, 0);
    int desiredTop = static_cast<int>(targetLine) - 2;
    if (desiredTop < 0) desiredTop = 0;
    int delta = desiredTop - static_cast<int>(firstVisible);
    if (delta != 0) {
        SendMessageW(m_hEditCtrl, EM_LINESCROLL, 0, static_cast<LPARAM>(delta));
    }
    SendMessageW(m_hEditCtrl, EM_SETSEL,
                 static_cast<WPARAM>(pos),
                 static_cast<LPARAM>(pos + term.size()));

    m_findLastMatchStart = pos;
    m_findLastMatchEnd   = pos + term.size();
    m_findSearchPos      = m_findLastMatchEnd;

    if (wrapped) MessageBeep(MB_OK);
    return true;
}
