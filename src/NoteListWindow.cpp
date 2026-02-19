#include "NoteListWindow.h"
#include "Application.h"
#include "NoteWindow.h"
#include "Localization.h"
#include "Storage.h"
#include "Utils.h"
#include "Resource.h"
#include <windowsx.h>
#include <uxtheme.h>
#include <ctime>
#include <algorithm>

static const wchar_t* NOTELIST_WND_CLASS = L"UltraNoteListWindow";

// ============================================================================
// Registration and construction
// ============================================================================

bool NoteListWindow::RegisterWindowClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = NOTELIST_WND_CLASS;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCE(IDI_APP));
    wc.hIconSm       = LoadIconW(hInst, MAKEINTRESOURCE(IDI_APP));
    wc.cbWndExtra    = sizeof(NoteListWindow*);
    return RegisterClassExW(&wc) != 0;
}

NoteListWindow::NoteListWindow(HINSTANCE hInst)
    : m_hInst(hInst) {}

NoteListWindow::~NoteListWindow() {
    if (m_hToolbarImages) {
        ImageList_Destroy(m_hToolbarImages);
        m_hToolbarImages = nullptr;
    }
    if (m_hFolderIcon) { DestroyIcon(m_hFolderIcon); m_hFolderIcon = nullptr; }
    if (m_hAllNotesIcon) { DestroyIcon(m_hAllNotesIcon); m_hAllNotesIcon = nullptr; }
    if (m_hwnd) {
        SetWindowLongPtrW(m_hwnd, 0, 0);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ============================================================================
// Create / Show / Hide
// ============================================================================

bool NoteListWindow::Create() {
    m_hwnd = CreateWindowExW(
        0,
        NOTELIST_WND_CLASS,
        Ls(L"notelist.title").c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 400,
        nullptr, nullptr, m_hInst, this
    );

    if (!m_hwnd) return false;

    // Helper: append menu item with shell stock icon
    auto& app = Application::Get();
    auto addItem = [&](HMENU hMenu, UINT id, const wchar_t* text, SHSTOCKICONID iconId) {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP;
        mii.wID        = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);
        mii.hbmpItem   = app.GetMenuBitmap(iconId);
        InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
    };

    // Build menu bar programmatically
    HMENU hMenuBar = CreateMenu();

    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    addItem(hFileMenu, ID_NL_NOTE_NEW, Ls(L"notelist.new_note").c_str(), SIID_DOCNOASSOC);
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    addItem(hFileMenu, ID_NL_FILE_CLOSE, Ls(L"notelist.close").c_str(), SIID_DELETE);
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu),
                Ls(L"notelist.file").c_str());

    // Note menu
    HMENU hNoteMenu = CreatePopupMenu();
    addItem(hNoteMenu, ID_NL_NOTE_EDIT,   Ls(L"notelist.edit").c_str(),   SIID_RENAME);
    addItem(hNoteMenu, ID_NL_NOTE_RENAME, Ls(L"notelist.rename").c_str(), SIID_DOCASSOC);
    addItem(hNoteMenu, ID_NL_NOTE_DELETE, Ls(L"notelist.delete").c_str(), SIID_DELETE);
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hNoteMenu),
                Ls(L"notelist.note").c_str());

    // Folder menu
    HMENU hFolderMenu = CreatePopupMenu();
    addItem(hFolderMenu, ID_NL_FOLDER_NEW,    Ls(L"folder.new").c_str(),    SIID_FOLDER);
    addItem(hFolderMenu, ID_NL_FOLDER_RENAME, Ls(L"folder.rename").c_str(), SIID_RENAME);
    addItem(hFolderMenu, ID_NL_FOLDER_DELETE, Ls(L"folder.delete").c_str(), SIID_DELETE);
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFolderMenu),
                Ls(L"notelist.folder_menu").c_str());

    // Options menu with Settings > Language submenu
    HMENU hOptionsMenu = CreatePopupMenu();
    HMENU hSettingsMenu = CreatePopupMenu();
    HMENU hLangMenu = CreatePopupMenu();
    auto langs = Localization::Get().GetAvailableLanguages();
    const auto& currentLang = Localization::Get().GetCurrentLanguage();
    for (size_t i = 0; i < langs.size() && i < (ID_LANG_MAX - ID_LANG_BASE + 1); ++i) {
        UINT flags = MF_STRING;
        if (langs[i].first == currentLang) flags |= MF_CHECKED;
        AppendMenuW(hLangMenu, flags, ID_LANG_BASE + static_cast<UINT>(i),
                    langs[i].second.c_str());
    }
    // Settings submenu with icon
    {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_STRING | MIIM_SUBMENU | MIIM_BITMAP;
        mii.hSubMenu   = hLangMenu;
        mii.dwTypeData = const_cast<wchar_t*>(Ls(L"menu.language").c_str());
        mii.hbmpItem   = app.GetMenuBitmap(SIID_WORLD);
        InsertMenuItemW(hSettingsMenu, 0, TRUE, &mii);
    }
    {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_STRING | MIIM_SUBMENU | MIIM_BITMAP;
        mii.hSubMenu   = hSettingsMenu;
        mii.dwTypeData = const_cast<wchar_t*>(Ls(L"menu.settings").c_str());
        mii.hbmpItem   = app.GetMenuBitmap(SIID_WORLD);
        InsertMenuItemW(hOptionsMenu, 0, TRUE, &mii);
    }
    AppendMenuW(hOptionsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hOptionsMenu, MF_STRING | (m_previewEnabled ? MF_CHECKED : 0),
                ID_NL_PREVIEW, Ls(L"notelist.preview").c_str());

    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hOptionsMenu),
                Ls(L"notelist.options").c_str());

    SetMenu(m_hwnd, hMenuBar);

    // Load small icons for folder list
    SHSTOCKICONINFO sii = {};
    sii.cbSize = sizeof(sii);
    if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SMALLICON, &sii))) {
        m_hFolderIcon = sii.hIcon;
    }
    sii = {};
    sii.cbSize = sizeof(sii);
    if (SUCCEEDED(SHGetStockIconInfo(SIID_STACK, SHGSI_ICON | SHGSI_SMALLICON, &sii))) {
        m_hAllNotesIcon = sii.hIcon;
    }

    CreateToolbar();
    CreateFolderList();
    CreateListView();
    LoadSettings();
    return true;
}

void NoteListWindow::Show() {
    Refresh();
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    if (m_previewEnabled) StartPreviewTimer();
}

void NoteListWindow::Hide() {
    StopPreviewTimer();
    HidePreviewNote();
    ShowWindow(m_hwnd, SW_HIDE);
}

bool NoteListWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

// ============================================================================
// Window procedure
// ============================================================================

LRESULT CALLBACK NoteListWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NoteListWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<NoteListWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, 0, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<NoteListWindow*>(GetWindowLongPtrW(hwnd, 0));
    }

    if (self)
        return self->HandleMessage(msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT NoteListWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            ResizeControls();
            return 0;

        case WM_COMMAND: {
            UINT code = HIWORD(wParam);
            UINT id = LOWORD(wParam);

            // Folder ListBox selection change
            if (reinterpret_cast<HWND>(lParam) == m_hFolderList && code == LBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
                if (sel == 0) {
                    // "All Notes"
                    m_selectedFolder.clear();
                } else if (sel > 0) {
                    int len = static_cast<int>(SendMessageW(m_hFolderList, LB_GETTEXTLEN, sel, 0));
                    if (len > 0) {
                        std::wstring text(static_cast<size_t>(len), L'\0');
                        SendMessageW(m_hFolderList, LB_GETTEXT, sel,
                                     reinterpret_cast<LPARAM>(&text[0]));
                        m_selectedFolder = text;
                    }
                }
                PopulateList();
                return 0;
            }

            // Language selection - forward to app window
            if (id >= ID_LANG_BASE && id <= ID_LANG_MAX) {
                HWND appWnd = FindWindowW(L"UltraNoteApp", L"UltraNote");
                if (appWnd)
                    PostMessageW(appWnd, WM_COMMAND, wParam, 0);
                return 0;
            }

            // Folder assignment submenu
            if (id >= ID_NL_FOLDER_BASE && id <= ID_NL_FOLDER_MAX) {
                size_t folderIdx = id - ID_NL_FOLDER_BASE;
                auto& folders = Application::Get().GetFolders();
                std::wstring targetFolder;
                if (folderIdx == 0) {
                    targetFolder.clear(); // No folder
                } else if (folderIdx - 1 < folders.size()) {
                    targetFolder = folders[folderIdx - 1];
                }
                // Apply to all selected notes
                int idx = -1;
                while ((idx = ListView_GetNextItem(m_hListView, idx, LVNI_SELECTED)) >= 0) {
                    LVITEMW item = {};
                    item.mask  = LVIF_PARAM;
                    item.iItem = idx;
                    ListView_GetItem(m_hListView, &item);
                    Application::Get().SetNoteFolder(
                        static_cast<uint64_t>(item.lParam), targetFolder);
                }
                return 0;
            }

            switch (id) {
                case ID_NL_FILE_CLOSE:
                    Hide();
                    return 0;
                case ID_NL_NOTE_NEW:
                    Application::Get().CreateNewNote();
                    return 0;
                case ID_NL_NOTE_EDIT:
                    EditSelectedNote();
                    return 0;
                case ID_NL_NOTE_RENAME:
                    RenameSelectedNote();
                    return 0;
                case ID_NL_NOTE_DELETE:
                    DeleteSelectedNotes();
                    return 0;
                case ID_NL_SHOW_ALL:
                    Application::Get().ShowAllNotes();
                    return 0;
                case ID_NL_HIDE_ALL:
                    Application::Get().HideAllNotes();
                    return 0;
                case ID_NL_PREVIEW: {
                    m_previewEnabled = !m_previewEnabled;
                    // Update menu checkmark
                    HMENU hMenuBar = GetMenu(m_hwnd);
                    if (hMenuBar) {
                        int menuCount = GetMenuItemCount(hMenuBar);
                        HMENU hOptionsMenu = GetSubMenu(hMenuBar, menuCount - 1);
                        if (hOptionsMenu)
                            CheckMenuItem(hOptionsMenu, ID_NL_PREVIEW,
                                          MF_BYCOMMAND | (m_previewEnabled ? MF_CHECKED : MF_UNCHECKED));
                    }
                    if (m_previewEnabled) {
                        StartPreviewTimer();
                    } else {
                        StopPreviewTimer();
                        HidePreviewNote();
                    }
                    return 0;
                }
                case ID_NL_FOLDER_NEW:
                    NewFolderDialog();
                    return 0;
                case ID_NL_FOLDER_RENAME: {
                    // Rename currently selected folder (index 0 = "All Notes", skip)
                    int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
                    if (sel > 0) {
                        int len = static_cast<int>(SendMessageW(m_hFolderList, LB_GETTEXTLEN, sel, 0));
                        if (len > 0) {
                            std::wstring text(static_cast<size_t>(len), L'\0');
                            SendMessageW(m_hFolderList, LB_GETTEXT, sel,
                                         reinterpret_cast<LPARAM>(&text[0]));
                            RenameFolderDialog(text);
                        }
                    }
                    return 0;
                }
                case ID_NL_FOLDER_DELETE: {
                    int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
                    if (sel > 0) {
                        int len = static_cast<int>(SendMessageW(m_hFolderList, LB_GETTEXTLEN, sel, 0));
                        if (len > 0) {
                            std::wstring text(static_cast<size_t>(len), L'\0');
                            SendMessageW(m_hFolderList, LB_GETTEXT, sel,
                                         reinterpret_cast<LPARAM>(&text[0]));
                            DeleteFolderConfirm(text);
                        }
                    }
                    return 0;
                }
            }
            break;
        }

        case WM_NOTIFY: {
            auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->hwndFrom == m_hListView) {
                switch (nmhdr->code) {
                    case LVN_COLUMNCLICK: {
                        auto* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
                        SortByColumn(nmlv->iSubItem);
                        return 0;
                    }
                    case NM_DBLCLK:
                        EditSelectedNote();
                        return 0;
                    case NM_RCLICK: {
                        POINT pt;
                        GetCursorPos(&pt);
                        ShowNoteContextMenu(pt.x, pt.y);
                        return 0;
                    }
                    case LVN_KEYDOWN: {
                        auto* kd = reinterpret_cast<NMLVKEYDOWN*>(lParam);
                        if (kd->wVKey == VK_F2) {
                            RenameSelectedNote();
                            return 0;
                        }
                        break;
                    }
                }
            }
            // Header custom draw (gray column headers)
            HWND hHeader = m_hListView ? ListView_GetHeader(m_hListView) : nullptr;
            if (hHeader && nmhdr->hwndFrom == hHeader && nmhdr->code == NM_CUSTOMDRAW) {
                auto* cd = reinterpret_cast<NMCUSTOMDRAW*>(lParam);
                switch (cd->dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT: {
                        // Gray background matching "Alle Notizen"
                        HBRUSH hBrush = CreateSolidBrush(RGB(230, 230, 230));
                        FillRect(cd->hdc, &cd->rc, hBrush);
                        DeleteObject(hBrush);

                        // Get header item text
                        HDITEMW hdi = {};
                        wchar_t buf[128] = {};
                        hdi.mask = HDI_TEXT;
                        hdi.pszText = buf;
                        hdi.cchTextMax = 128;
                        SendMessageW(hHeader, HDM_GETITEMW, cd->dwItemSpec,
                                     reinterpret_cast<LPARAM>(&hdi));

                        // Draw text
                        SetBkMode(cd->hdc, TRANSPARENT);
                        SetTextColor(cd->hdc, GetSysColor(COLOR_BTNTEXT));
                        RECT textRc = cd->rc;
                        textRc.left += 6;
                        DrawTextW(cd->hdc, buf, -1, &textRc,
                                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                        // Subtle right border
                        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                        HGDIOBJ oldPen = SelectObject(cd->hdc, hPen);
                        MoveToEx(cd->hdc, cd->rc.right - 1, cd->rc.top, nullptr);
                        LineTo(cd->hdc, cd->rc.right - 1, cd->rc.bottom);
                        SelectObject(cd->hdc, oldPen);
                        DeleteObject(hPen);

                        return CDRF_SKIPDEFAULT;
                    }
                    default:
                        return CDRF_DODEFAULT;
                }
            }
            if (nmhdr->hwndFrom == m_hToolbar && nmhdr->code == NM_CUSTOMDRAW) {
                auto* cd = reinterpret_cast<NMTBCUSTOMDRAW*>(lParam);
                switch (cd->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT: {
                        RECT rc;
                        GetClientRect(m_hToolbar, &rc);
                        HBRUSH hBrush = CreateSolidBrush(RGB(225, 225, 225));
                        FillRect(cd->nmcd.hdc, &rc, hBrush);
                        DeleteObject(hBrush);
                        return CDRF_NOTIFYITEMDRAW;
                    }
                    case CDDS_ITEMPREPAINT:
                        cd->clrBtnFace = RGB(225, 225, 225);
                        return TBCDRF_USECDCOLORS;
                    default:
                        return CDRF_DODEFAULT;
                }
            }
            if (nmhdr->hwndFrom == m_hToolbar && nmhdr->code == TBN_GETINFOTIPW) {
                auto* tip = reinterpret_cast<NMTBGETINFOTIPW*>(lParam);
                const wchar_t* key = nullptr;
                switch (tip->iItem) {
                    case ID_NL_NOTE_NEW:    key = L"notelist.tb_new";      break;
                    case ID_NL_NOTE_EDIT:   key = L"notelist.tb_edit";     break;
                    case ID_NL_NOTE_DELETE:  key = L"notelist.tb_delete";   break;
                    case ID_NL_NOTE_RENAME:  key = L"notelist.tb_rename";   break;
                    case ID_NL_SHOW_ALL:    key = L"notelist.tb_show_all"; break;
                    case ID_NL_HIDE_ALL:    key = L"notelist.tb_hide_all"; break;
                }
                if (key) {
                    std::wstring text = Ls(key);
                    wcsncpy_s(tip->pszText, tip->cchTextMax, text.c_str(), _TRUNCATE);
                }
                return 0;
            }
            break;
        }

        case WM_SETCURSOR: {
            if (reinterpret_cast<HWND>(wParam) == m_hwnd) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(m_hwnd, &pt);
                if (pt.x >= m_folderListWidth && pt.x < m_folderListWidth + SPLITTER_WIDTH) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                    return TRUE;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            if (x >= m_folderListWidth && x < m_folderListWidth + SPLITTER_WIDTH) {
                m_splitterDragging = true;
                m_splitterDragStart = x;
                m_splitterWidthStart = m_folderListWidth;
                SetCapture(m_hwnd);
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (m_splitterDragging) {
                int x = GET_X_LPARAM(lParam);
                int newWidth = m_splitterWidthStart + (x - m_splitterDragStart);
                if (newWidth < FOLDER_MIN_WIDTH) newWidth = FOLDER_MIN_WIDTH;
                if (newWidth > FOLDER_MAX_WIDTH) newWidth = FOLDER_MAX_WIDTH;
                if (newWidth != m_folderListWidth) {
                    m_folderListWidth = newWidth;
                    ResizeControls();
                }
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (m_splitterDragging) {
                m_splitterDragging = false;
                ReleaseCapture();
                return 0;
            }
            break;
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis->hwndItem == m_hFolderList) {
                if (dis->itemID == static_cast<UINT>(-1)) return TRUE;

                // Get item text
                int len = static_cast<int>(SendMessageW(m_hFolderList, LB_GETTEXTLEN, dis->itemID, 0));
                std::wstring text(static_cast<size_t>(len > 0 ? len : 0), L'\0');
                if (len > 0)
                    SendMessageW(m_hFolderList, LB_GETTEXT, dis->itemID,
                                 reinterpret_cast<LPARAM>(&text[0]));

                bool isSelected = (dis->itemState & ODS_SELECTED) != 0;
                bool isAllNotes = (dis->itemID == 0);

                // Background
                COLORREF bgColor;
                COLORREF txColor;
                if (isSelected) {
                    bgColor = GetSysColor(COLOR_HIGHLIGHT);
                    txColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
                } else if (isAllNotes) {
                    bgColor = RGB(230, 230, 230); // Light gray
                    txColor = GetSysColor(COLOR_WINDOWTEXT);
                } else {
                    bgColor = GetSysColor(COLOR_WINDOW);
                    txColor = GetSysColor(COLOR_WINDOWTEXT);
                }

                HBRUSH hBrush = CreateSolidBrush(bgColor);
                FillRect(dis->hDC, &dis->rcItem, hBrush);
                DeleteObject(hBrush);

                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, txColor);

                // Draw icon
                int iconSize = GetSystemMetrics(SM_CXSMICON);
                int iconY = dis->rcItem.top + (dis->rcItem.bottom - dis->rcItem.top - iconSize) / 2;
                HICON hIcon = isAllNotes ? m_hAllNotesIcon : m_hFolderIcon;
                if (hIcon) {
                    DrawIconEx(dis->hDC, dis->rcItem.left + 3, iconY,
                               hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
                }
                int textLeft = dis->rcItem.left + 3 + iconSize + 4;

                // Bold font for "All Notes"
                HFONT hFont = nullptr;
                HFONT hOldFont = nullptr;
                if (isAllNotes) {
                    LOGFONTW lf = {};
                    HFONT hDefault = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
                    GetObjectW(hDefault, sizeof(lf), &lf);
                    lf.lfWeight = FW_BOLD;
                    hFont = CreateFontIndirectW(&lf);
                    hOldFont = static_cast<HFONT>(SelectObject(dis->hDC, hFont));
                }

                RECT textRc = dis->rcItem;
                textRc.left = textLeft;
                DrawTextW(dis->hDC, text.c_str(), static_cast<int>(text.size()),
                          &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                if (hFont) {
                    SelectObject(dis->hDC, hOldFont);
                    DeleteObject(hFont);
                }

                // Focus rect
                if (dis->itemState & ODS_FOCUS) {
                    DrawFocusRect(dis->hDC, &dis->rcItem);
                }

                return TRUE;
            }
            break;
        }

        case WM_MEASUREITEM: {
            auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            int iconSize = GetSystemMetrics(SM_CYSMICON);
            mis->itemHeight = (iconSize > 16 ? iconSize : 16) + 4;
            return TRUE;
        }

        case WM_CONTEXTMENU: {
            if (reinterpret_cast<HWND>(wParam) == m_hFolderList) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ShowFolderContextMenu(pt.x, pt.y);
                return 0;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_PREVIEW && m_previewEnabled && m_hListView) {
                POINT pt;
                GetCursorPos(&pt);
                POINT clientPt = pt;
                ScreenToClient(m_hListView, &clientPt);

                RECT lvRect;
                GetClientRect(m_hListView, &lvRect);

                int hitIdx = -1;
                if (PtInRect(&lvRect, clientPt)) {
                    LVHITTESTINFO htInfo = {};
                    htInfo.pt = clientPt;
                    hitIdx = ListView_HitTest(m_hListView, &htInfo);
                }

                if (hitIdx != m_previewPendingIdx) {
                    // Cursor moved to a different row
                    m_previewPendingIdx = hitIdx;
                    m_previewHoverStart = GetTickCount();

                    // If cursor left the list, hide preview
                    if (hitIdx < 0) {
                        HidePreviewNote();
                    }
                } else if (hitIdx >= 0 && hitIdx != m_previewNoteIdx) {
                    // Still on same row, check if delay elapsed
                    DWORD elapsed = GetTickCount() - m_previewHoverStart;
                    if (elapsed >= PREVIEW_DELAY_MS) {
                        // Get note ID from ListView item
                        LVITEMW item = {};
                        item.mask  = LVIF_PARAM;
                        item.iItem = hitIdx;
                        ListView_GetItem(m_hListView, &item);
                        uint64_t noteId = static_cast<uint64_t>(item.lParam);

                        HidePreviewNote();
                        ShowPreviewNote(noteId);
                        m_previewNoteIdx = hitIdx;
                    }
                }
            }
            return 0;
        }

        case WM_CLOSE:
            SaveSettings();
            StopPreviewTimer();
            HidePreviewNote();
            Hide();
            return 0;

        case WM_DESTROY:
            return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

// ============================================================================
// Toolbar
// ============================================================================

void NoteListWindow::CreateToolbar() {
    m_hToolbar = CreateWindowExW(
        0, TOOLBARCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP,
        0, 0, 0, 0,
        m_hwnd, nullptr, m_hInst, nullptr
    );
    if (!m_hToolbar) return;

    SendMessageW(m_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(m_hToolbar, TB_SETMAXTEXTROWS, 0, 0);

    // Create image list with shell stock icons
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    m_hToolbarImages = ImageList_Create(cx, cy, ILC_COLOR32, 6, 1);

    struct { SHSTOCKICONID siid; } icons[] = {
        { SIID_DOCNOASSOC },   // 0: New
        { SIID_RENAME },       // 1: Edit
        { SIID_DELETE },       // 2: Delete
        { SIID_FOLDER },       // 3: Show All
        { SIID_FOLDERBACK },   // 4: Hide All
        { SIID_DOCASSOC },     // 5: Rename (use DOCASSOC as rename icon)
    };
    for (auto& icon : icons) {
        HBITMAP hBmp = LoadShellMenuBitmap(icon.siid);
        if (hBmp) {
            ImageList_Add(m_hToolbarImages, hBmp, nullptr);
            DeleteObject(hBmp);
        }
    }

    SendMessageW(m_hToolbar, TB_SETIMAGELIST, 0,
                 reinterpret_cast<LPARAM>(m_hToolbarImages));

    TBBUTTON buttons[] = {
        { 0, ID_NL_NOTE_NEW,    TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 1, ID_NL_NOTE_EDIT,   TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 5, ID_NL_NOTE_RENAME, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 2, ID_NL_NOTE_DELETE,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 0, 0,                 0,               BTNS_SEP,    {0}, 0, 0 },
        { 3, ID_NL_SHOW_ALL,    TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 4, ID_NL_HIDE_ALL,    TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
    };

    SendMessageW(m_hToolbar, TB_ADDBUTTONS, _countof(buttons),
                 reinterpret_cast<LPARAM>(buttons));
    SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);
}

// ============================================================================
// Folder list (left panel)
// ============================================================================

void NoteListWindow::CreateFolderList() {
    m_hFolderList = CreateWindowExW(
        0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
        LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER,
        0, 0, m_folderListWidth, 100,
        m_hwnd, nullptr, m_hInst, nullptr
    );

    PopulateFolderList();
}

void NoteListWindow::PopulateFolderList() {
    if (!m_hFolderList) return;

    SendMessageW(m_hFolderList, LB_RESETCONTENT, 0, 0);

    // "All Notes" always first
    SendMessageW(m_hFolderList, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(Ls(L"note.all_folders").c_str()));

    // Sorted folders
    auto& folders = Application::Get().GetFolders();
    for (const auto& f : folders) {
        SendMessageW(m_hFolderList, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(f.c_str()));
    }

    // Restore selection
    if (m_selectedFolder.empty()) {
        SendMessageW(m_hFolderList, LB_SETCURSEL, 0, 0); // "All Notes"
    } else {
        int count = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCOUNT, 0, 0));
        bool found = false;
        for (int i = 1; i < count; ++i) {
            int len = static_cast<int>(SendMessageW(m_hFolderList, LB_GETTEXTLEN, i, 0));
            if (len > 0) {
                std::wstring text(static_cast<size_t>(len), L'\0');
                SendMessageW(m_hFolderList, LB_GETTEXT, i,
                             reinterpret_cast<LPARAM>(&text[0]));
                if (text == m_selectedFolder) {
                    SendMessageW(m_hFolderList, LB_SETCURSEL, i, 0);
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            m_selectedFolder.clear();
            SendMessageW(m_hFolderList, LB_SETCURSEL, 0, 0);
        }
    }
}

// ============================================================================
// ListView
// ============================================================================

void NoteListWindow::CreateListView() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    int tbHeight = 0;
    if (m_hToolbar) {
        RECT tbRect;
        GetWindowRect(m_hToolbar, &tbRect);
        tbHeight = tbRect.bottom - tbRect.top;
    }

    m_hListView = CreateWindowExW(
        0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        m_folderListWidth + SPLITTER_WIDTH, tbHeight,
        rc.right - m_folderListWidth - SPLITTER_WIDTH, rc.bottom - tbHeight,
        m_hwnd, nullptr, m_hInst, nullptr
    );

    ListView_SetExtendedListViewStyle(m_hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    SetupColumns();

    // Disable theme on header so our NM_CUSTOMDRAW gray background works
    HWND hHeader = ListView_GetHeader(m_hListView);
    if (hHeader) {
        SetWindowTheme(hHeader, L"", L"");
    }
}

void NoteListWindow::SetupColumns() {
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt = LVCFMT_LEFT;

    col.cx = 150;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_title").c_str());
    ListView_InsertColumn(m_hListView, COL_TITLE, &col);

    col.cx = 200;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_text").c_str());
    ListView_InsertColumn(m_hListView, COL_TEXT, &col);

    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_folder").c_str());
    ListView_InsertColumn(m_hListView, COL_FOLDER, &col);

    col.cx = 130;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_created").c_str());
    ListView_InsertColumn(m_hListView, COL_CREATED, &col);
}

void NoteListWindow::PopulateList() {
    ListView_DeleteAllItems(m_hListView);

    auto& notes = Application::Get().GetAllNotes();

    int insertIdx = 0;
    for (size_t i = 0; i < notes.size(); ++i) {
        auto& note = notes[i];

        // Filter by selected folder
        if (!m_selectedFolder.empty()) {
            if (note.folder != m_selectedFolder) continue;
        }

        // Title: use title field, fallback to first line of text
        std::wstring titleDisplay = note.title;
        if (titleDisplay.empty()) {
            titleDisplay = note.text;
            auto nl = titleDisplay.find(L'\n');
            if (nl != std::wstring::npos) titleDisplay = titleDisplay.substr(0, nl);
            if (titleDisplay.empty()) titleDisplay = Ls(L"note.empty");
        }

        // Text: first line
        std::wstring textDisplay = note.text;
        auto nl = textDisplay.find(L'\n');
        if (nl != std::wstring::npos) textDisplay = textDisplay.substr(0, nl);
        if (textDisplay.empty()) textDisplay = Ls(L"note.empty");

        // Folder display (empty if no folder assigned)
        std::wstring folderDisplay = note.folder;

        LVITEMW item = {};
        item.mask    = LVIF_TEXT | LVIF_PARAM;
        item.iItem   = insertIdx;
        item.lParam  = static_cast<LPARAM>(note.id);
        item.pszText = const_cast<LPWSTR>(titleDisplay.c_str());

        int idx = ListView_InsertItem(m_hListView, &item);

        ListView_SetItemText(m_hListView, idx, COL_TEXT,
                             const_cast<LPWSTR>(textDisplay.c_str()));
        ListView_SetItemText(m_hListView, idx, COL_FOLDER,
                             const_cast<LPWSTR>(folderDisplay.c_str()));

        // Format timestamp
        if (note.createdAt > 0) {
            time_t t = static_cast<time_t>(note.createdAt);
            struct tm tm = {};
            localtime_s(&tm, &t);
            wchar_t buf[64];
            wcsftime(buf, 64, L"%Y-%m-%d %H:%M", &tm);
            ListView_SetItemText(m_hListView, idx, COL_CREATED, buf);
        }

        ++insertIdx;
    }
}

void NoteListWindow::Refresh() {
    if (!m_hListView) return;
    PopulateFolderList();
    PopulateList();
    if (m_sortColumn >= 0) {
        SortByColumn(m_sortColumn);
    }
}

void NoteListWindow::ResizeControls() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    int tbHeight = 0;
    if (m_hToolbar) {
        SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);
        RECT tbRect;
        GetWindowRect(m_hToolbar, &tbRect);
        tbHeight = tbRect.bottom - tbRect.top;
    }

    if (m_hFolderList) {
        MoveWindow(m_hFolderList, 0, tbHeight, m_folderListWidth,
                   rc.bottom - tbHeight, TRUE);
    }

    int lvLeft = m_folderListWidth + SPLITTER_WIDTH;
    if (m_hListView) {
        MoveWindow(m_hListView, lvLeft, tbHeight,
                   rc.right - lvLeft, rc.bottom - tbHeight, TRUE);
    }

    // Invalidate splitter area
    RECT splitterRc = { m_folderListWidth, tbHeight,
                        m_folderListWidth + SPLITTER_WIDTH, rc.bottom };
    InvalidateRect(m_hwnd, &splitterRc, TRUE);
}

// ============================================================================
// Settings persistence
// ============================================================================

void NoteListWindow::LoadSettings() {
    auto settings = Storage::LoadSettings();

    // Window position and size
    auto itX = settings.find(L"notelist.x");
    auto itY = settings.find(L"notelist.y");
    auto itW = settings.find(L"notelist.width");
    auto itH = settings.find(L"notelist.height");
    if (itW != settings.end() && itH != settings.end()) {
        int w = itW->second;
        int h = itH->second;
        if (w >= 200 && h >= 150) {
            UINT flags = SWP_NOZORDER;
            int x = 0, y = 0;
            if (itX != settings.end() && itY != settings.end()) {
                x = itX->second;
                y = itY->second;
                POINT pt = { x + w / 2, y + 20 };
                if (MonitorFromPoint(pt, MONITOR_DEFAULTTONULL) != nullptr) {
                    flags &= ~SWP_NOMOVE;
                } else {
                    flags |= SWP_NOMOVE;
                }
            } else {
                flags |= SWP_NOMOVE;
            }
            SetWindowPos(m_hwnd, nullptr, x, y, w, h, flags);
        }
    }

    // Folder list width
    auto itFW = settings.find(L"notelist.folder_width");
    if (itFW != settings.end() && itFW->second >= FOLDER_MIN_WIDTH && itFW->second <= FOLDER_MAX_WIDTH) {
        m_folderListWidth = itFW->second;
        ResizeControls();
    }

    // Preview setting
    auto itPV = settings.find(L"notelist.preview");
    if (itPV != settings.end()) {
        m_previewEnabled = (itPV->second != 0);
        if (m_previewEnabled) {
            StartPreviewTimer();
            // Update menu checkmark
            HMENU hMenuBar = GetMenu(m_hwnd);
            if (hMenuBar) {
                int menuCount = GetMenuItemCount(hMenuBar);
                HMENU hOptionsMenu = GetSubMenu(hMenuBar, menuCount - 1);
                if (hOptionsMenu)
                    CheckMenuItem(hOptionsMenu, ID_NL_PREVIEW, MF_BYCOMMAND | MF_CHECKED);
            }
        }
    }

    // Column widths (4 columns now)
    if (m_hListView) {
        const wchar_t* keys[] = {
            L"notelist.col0_width", L"notelist.col1_width",
            L"notelist.col2_width", L"notelist.col3_width"
        };
        for (int i = 0; i < COL_COUNT; ++i) {
            auto it = settings.find(keys[i]);
            if (it != settings.end() && it->second > 20)
                ListView_SetColumnWidth(m_hListView, i, it->second);
        }
    }
}

void NoteListWindow::SaveSettings() {
    auto settings = Storage::LoadSettings();

    RECT wr;
    GetWindowRect(m_hwnd, &wr);
    settings[L"notelist.x"]      = wr.left;
    settings[L"notelist.y"]      = wr.top;
    settings[L"notelist.width"]  = wr.right - wr.left;
    settings[L"notelist.height"] = wr.bottom - wr.top;

    settings[L"notelist.folder_width"] = m_folderListWidth;
    settings[L"notelist.preview"] = m_previewEnabled ? 1 : 0;

    if (m_hListView) {
        settings[L"notelist.col0_width"] = ListView_GetColumnWidth(m_hListView, COL_TITLE);
        settings[L"notelist.col1_width"] = ListView_GetColumnWidth(m_hListView, COL_TEXT);
        settings[L"notelist.col2_width"] = ListView_GetColumnWidth(m_hListView, COL_FOLDER);
        settings[L"notelist.col3_width"] = ListView_GetColumnWidth(m_hListView, COL_CREATED);
    }

    Storage::SaveSettings(settings);
}

// ============================================================================
// Sorting
// ============================================================================

void NoteListWindow::SortByColumn(int col) {
    if (col == m_sortColumn) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortColumn = col;
        m_sortAscending = true;
    }

    LPARAM sortParam = static_cast<LPARAM>(col) | (m_sortAscending ? 0x10000 : 0);
    ListView_SortItems(m_hListView, CompareFunc, sortParam);
}

int CALLBACK NoteListWindow::CompareFunc(LPARAM lp1, LPARAM lp2, LPARAM sortParam) {
    int col = static_cast<int>(sortParam & 0xFFFF);
    bool ascending = (sortParam & 0x10000) != 0;

    uint64_t id1 = static_cast<uint64_t>(lp1);
    uint64_t id2 = static_cast<uint64_t>(lp2);

    NoteData* n1 = Application::Get().FindNoteData(id1);
    NoteData* n2 = Application::Get().FindNoteData(id2);
    if (!n1 || !n2) return 0;

    int result = 0;
    switch (col) {
        case COL_TITLE: {
            // Compare by title, fallback to text
            const std::wstring& t1 = n1->title.empty() ? n1->text : n1->title;
            const std::wstring& t2 = n2->title.empty() ? n2->text : n2->title;
            result = _wcsicmp(t1.c_str(), t2.c_str());
            break;
        }
        case COL_TEXT:
            result = _wcsicmp(n1->text.c_str(), n2->text.c_str());
            break;
        case COL_FOLDER:
            result = _wcsicmp(n1->folder.c_str(), n2->folder.c_str());
            break;
        case COL_CREATED:
            if (n1->createdAt < n2->createdAt) result = -1;
            else if (n1->createdAt > n2->createdAt) result = 1;
            break;
    }

    return ascending ? result : -result;
}

// ============================================================================
// Actions
// ============================================================================

void NoteListWindow::EditSelectedNote() {
    int idx = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (idx < 0) return;

    LVITEMW item = {};
    item.mask  = LVIF_PARAM;
    item.iItem = idx;
    ListView_GetItem(m_hListView, &item);

    uint64_t id = static_cast<uint64_t>(item.lParam);

    // If this note is being previewed, restore its original position first
    if (id == m_previewNoteId) {
        RestorePreviewPosition();
        m_previewWasHidden = false;
        m_previewNoteId = 0;
        m_previewNoteIdx = -1;
    }

    Application::Get().BringNoteToFront(id);
}

void NoteListWindow::DeleteSelectedNotes() {
    int count = ListView_GetSelectedCount(m_hListView);
    if (count <= 0) return;

    std::wstring msg;
    if (count == 1) {
        msg = Ls(L"confirm.delete_one");
    } else {
        msg = FormatString(Ls(L"confirm.delete_multi").c_str(), count);
    }

    int result = MessageBoxW(m_hwnd, msg.c_str(), L"UltraNote",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result != IDYES) return;

    std::vector<uint64_t> ids;
    int idx = -1;
    while ((idx = ListView_GetNextItem(m_hListView, idx, LVNI_SELECTED)) >= 0) {
        LVITEMW item = {};
        item.mask  = LVIF_PARAM;
        item.iItem = idx;
        ListView_GetItem(m_hListView, &item);
        ids.push_back(static_cast<uint64_t>(item.lParam));
    }

    for (uint64_t id : ids) {
        Application::Get().RequestDeleteNote(id);
    }
}

void NoteListWindow::RenameSelectedNote() {
    int idx = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (idx < 0) return;

    LVITEMW item = {};
    item.mask  = LVIF_PARAM;
    item.iItem = idx;
    ListView_GetItem(m_hListView, &item);

    uint64_t id = static_cast<uint64_t>(item.lParam);
    NoteData* note = Application::Get().FindNoteData(id);
    if (!note) return;

    std::wstring value = note->title;
    if (InputDialog(m_hwnd, Ls(L"note.enter_title"), L"UltraNote", value)) {
        Application::Get().RenameNote(id, value);
    }
}

void NoteListWindow::ShowSetFolderMenu() {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    // "(No Folder)" as first entry
    AppendMenuW(hPopup, MF_STRING, ID_NL_FOLDER_BASE, Ls(L"note.no_folder").c_str());
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    auto& folders = Application::Get().GetFolders();
    for (size_t i = 0; i < folders.size() && i + 1 < (ID_NL_FOLDER_MAX - ID_NL_FOLDER_BASE); ++i) {
        AppendMenuW(hPopup, MF_STRING,
                    ID_NL_FOLDER_BASE + static_cast<UINT>(i + 1),
                    folders[i].c_str());
    }

    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hPopup);
}

// ============================================================================
// Context menus
// ============================================================================

void NoteListWindow::ShowNoteContextMenu(int screenX, int screenY) {
    int idx = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (idx < 0) return;

    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    AppendMenuW(hPopup, MF_STRING, ID_NL_NOTE_EDIT,   Ls(L"notelist.edit").c_str());
    AppendMenuW(hPopup, MF_STRING, ID_NL_NOTE_RENAME, Ls(L"notelist.rename").c_str());
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    // "Set Folder" submenu
    HMENU hFolderSub = CreatePopupMenu();
    AppendMenuW(hFolderSub, MF_STRING, ID_NL_FOLDER_BASE, Ls(L"note.no_folder").c_str());
    AppendMenuW(hFolderSub, MF_SEPARATOR, 0, nullptr);
    auto& folders = Application::Get().GetFolders();
    for (size_t i = 0; i < folders.size() && i + 1 < (ID_NL_FOLDER_MAX - ID_NL_FOLDER_BASE); ++i) {
        AppendMenuW(hFolderSub, MF_STRING,
                    ID_NL_FOLDER_BASE + static_cast<UINT>(i + 1),
                    folders[i].c_str());
    }
    AppendMenuW(hPopup, MF_POPUP, reinterpret_cast<UINT_PTR>(hFolderSub),
                Ls(L"notelist.set_folder").c_str());

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hPopup, MF_STRING, ID_NL_NOTE_DELETE, Ls(L"notelist.delete").c_str());

    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(hPopup);
}

void NoteListWindow::ShowFolderContextMenu(int screenX, int screenY) {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
    bool isFolder = (sel > 0); // Index 0 = "All Notes"

    AppendMenuW(hPopup, MF_STRING, ID_NL_FOLDER_NEW, Ls(L"folder.new").c_str());
    if (isFolder) {
        AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPopup, MF_STRING, ID_NL_FOLDER_RENAME, Ls(L"folder.rename").c_str());
        AppendMenuW(hPopup, MF_STRING, ID_NL_FOLDER_DELETE, Ls(L"folder.delete").c_str());
    }

    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(hPopup);
}

// ============================================================================
// Folder dialogs
// ============================================================================

void NoteListWindow::NewFolderDialog() {
    std::wstring name;
    if (InputDialog(m_hwnd, Ls(L"folder.enter_name"), Ls(L"folder.new"), name)) {
        if (!name.empty()) {
            Application::Get().AddFolder(name);
        }
    }
}

void NoteListWindow::RenameFolderDialog(const std::wstring& oldName) {
    std::wstring name = oldName;
    if (InputDialog(m_hwnd, Ls(L"folder.enter_name"), Ls(L"folder.rename"), name)) {
        if (!name.empty() && name != oldName) {
            Application::Get().RenameFolder(oldName, name);
        }
    }
}

void NoteListWindow::DeleteFolderConfirm(const std::wstring& name) {
    std::wstring msg = FormatString(Ls(L"folder.confirm_delete").c_str(), name.c_str());
    int result = MessageBoxW(m_hwnd, msg.c_str(), L"UltraNote",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result == IDYES) {
        Application::Get().DeleteFolder(name);
    }
}

// ============================================================================
// Preview
// ============================================================================

void NoteListWindow::StartPreviewTimer() {
    if (!m_previewTimerActive && m_hwnd) {
        SetTimer(m_hwnd, IDT_PREVIEW, 100, nullptr);
        m_previewTimerActive = true;
    }
}

void NoteListWindow::StopPreviewTimer() {
    if (m_previewTimerActive && m_hwnd) {
        KillTimer(m_hwnd, IDT_PREVIEW);
        m_previewTimerActive = false;
    }
    m_previewPendingIdx = -1;
    m_previewNoteIdx = -1;
}

void NoteListWindow::ShowPreviewNote(uint64_t noteId) {
    auto& app = Application::Get();

    NoteWindow* wnd = nullptr;
    if (app.IsNoteVisible(noteId)) {
        // Note already visible - just bring to front and reposition
        m_previewNoteId = noteId;
        m_previewWasHidden = false;
        wnd = app.FindNoteWindow(noteId);
    } else {
        // Show the note (it was hidden)
        wnd = app.ShowNotePreview(noteId);
        m_previewNoteId = noteId;
        m_previewWasHidden = true;
    }

    // Position note near cursor
    if (wnd) {
        NoteData* data = wnd->GetData();
        m_previewOrigX = data->x;
        m_previewOrigY = data->y;

        POINT cursorPt;
        GetCursorPos(&cursorPt);

        RECT noteRect;
        GetWindowRect(wnd->GetHwnd(), &noteRect);
        int noteW = noteRect.right - noteRect.left;
        int noteH = noteRect.bottom - noteRect.top;

        int newX = cursorPt.x;
        int newY = cursorPt.y;

        // Ensure note stays on screen
        HMONITOR hMon = MonitorFromPoint(cursorPt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMon, &mi)) {
            if (newX + noteW > mi.rcWork.right)
                newX = mi.rcWork.right - noteW;
            if (newY + noteH > mi.rcWork.bottom)
                newY = mi.rcWork.bottom - noteH;
        }

        SetWindowPos(wnd->GetHwnd(), HWND_TOPMOST, newX, newY, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
        // Remove topmost after positioning so it doesn't stay permanently on top
        if (!wnd->GetData()->layout.alwaysOnTop) {
            SetWindowPos(wnd->GetHwnd(), HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

void NoteListWindow::RestorePreviewPosition() {
    if (m_previewNoteId == 0) return;

    // Restore original position via Application
    Application::Get().MoveNoteWindow(m_previewNoteId, m_previewOrigX, m_previewOrigY);
}

void NoteListWindow::HidePreviewNote() {
    if (m_previewNoteId > 0) {
        RestorePreviewPosition();
        if (m_previewWasHidden) {
            Application::Get().HideNotePreview(m_previewNoteId);
        }
    }
    m_previewNoteId = 0;
    m_previewNoteIdx = -1;
    m_previewWasHidden = false;
}

// ============================================================================
// Simple input dialog
// ============================================================================

INT_PTR CALLBACK NoteListWindow::InputDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            auto* data = reinterpret_cast<InputDlgData*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
            SetWindowTextW(hwnd, data->title.c_str());

            // Create static label
            CreateWindowExW(0, L"STATIC", data->prompt.c_str(),
                            WS_CHILD | WS_VISIBLE,
                            10, 10, 280, 20,
                            hwnd, nullptr, nullptr, nullptr);

            // Create edit control
            HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", data->value.c_str(),
                                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          10, 35, 280, 24,
                                          hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100)),
                                          nullptr, nullptr);

            // OK / Cancel buttons
            CreateWindowExW(0, L"BUTTON", L"OK",
                            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                            120, 70, 80, 28,
                            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                            nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Cancel",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            210, 70, 80, 28,
                            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                            nullptr, nullptr);

            // Set font for all children
            HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            EnumChildWindows(hwnd, [](HWND child, LPARAM font) -> BOOL {
                SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
                return TRUE;
            }, reinterpret_cast<LPARAM>(hFont));

            SendMessageW(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);

            // Center on parent
            HWND hParent = GetParent(hwnd);
            if (hParent) {
                RECT rcParent, rcDlg;
                GetWindowRect(hParent, &rcParent);
                GetWindowRect(hwnd, &rcDlg);
                int x = rcParent.left + ((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left)) / 2;
                int y = rcParent.top + ((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top)) / 2;
                SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }

            return FALSE; // We set focus manually
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    auto* data = reinterpret_cast<InputDlgData*>(
                        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                    HWND hEdit = GetDlgItem(hwnd, 100);
                    int len = GetWindowTextLengthW(hEdit);
                    data->value.resize(static_cast<size_t>(len));
                    if (len > 0) GetWindowTextW(hEdit, &data->value[0], len + 1);
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

bool NoteListWindow::InputDialog(HWND parent, const std::wstring& prompt,
                                  const std::wstring& title, std::wstring& value) {
    // Create a dialog template in memory
    // We use a minimal DLGTEMPLATE structure
    struct {
        DLGTEMPLATE tmpl;
        WORD menu;
        WORD windowClass;
        WORD title;
    } dlg = {};

    dlg.tmpl.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.tmpl.cx = 200;  // Dialog units
    dlg.tmpl.cy = 70;

    InputDlgData data;
    data.prompt = prompt;
    data.title = title;
    data.value = value;

    INT_PTR result = DialogBoxIndirectParamW(
        nullptr,
        &dlg.tmpl,
        parent,
        InputDlgProc,
        reinterpret_cast<LPARAM>(&data)
    );

    if (result == IDOK) {
        value = data.value;
        return true;
    }
    return false;
}
