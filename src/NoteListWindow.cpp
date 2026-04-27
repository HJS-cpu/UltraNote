#include "NoteListWindow.h"
#include "Application.h"
#include "NoteWindow.h"
#include "SettingsDialog.h"
#include "Localization.h"
#include "Storage.h"
#include "Utils.h"
#include "Resource.h"
#include "AlarmScheduler.h"
#include "AlarmConfigDialog.h"
#include "HeaderDragOverlay.h"
#include <windowsx.h>
#include <uxtheme.h>
#include <ctime>
#include <algorithm>
#include <vector>

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
    if (m_hSearchIcon) { DestroyIcon(m_hSearchIcon); m_hSearchIcon = nullptr; }
    if (m_hSearchFont) { DeleteObject(m_hSearchFont); m_hSearchFont = nullptr; }
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
        WS_OVERLAPPEDWINDOW & ~WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 400,
        nullptr, nullptr, m_hInst, this
    );

    if (!m_hwnd) return false;

    // Set window icon (title bar + taskbar)
    HICON hIconSmall = static_cast<HICON>(LoadImageW(m_hInst, MAKEINTRESOURCE(IDI_NOTELIST),
                                                      IMAGE_ICON,
                                                      GetSystemMetrics(SM_CXSMICON),
                                                      GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    HICON hIconBig = static_cast<HICON>(LoadImageW(m_hInst, MAKEINTRESOURCE(IDI_NOTELIST),
                                                    IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXICON),
                                                    GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    if (hIconSmall) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));
    if (hIconBig)   SendMessageW(m_hwnd, WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(hIconBig));

    // Helpers: append menu item with shell stock icon or resource icon
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
    auto addItemRes = [&](HMENU hMenu, UINT id, const wchar_t* text, UINT iconResId) {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP;
        mii.wID        = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);
        mii.hbmpItem   = app.GetResourceBitmap(iconResId);
        InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
    };

    // Build menu bar programmatically
    HMENU hMenuBar = CreateMenu();

    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    addItemRes(hFileMenu, ID_NL_NOTE_NEW, Ls(L"notelist.new_note").c_str(), IDI_NEW);
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    addItemRes(hFileMenu, ID_NL_FILE_CLOSE, Ls(L"notelist.close").c_str(), IDI_EXIT);
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu),
                Ls(L"notelist.file").c_str());

    // Note menu
    HMENU hNoteMenu = CreatePopupMenu();
    addItem(hNoteMenu, ID_NL_NOTE_EDIT,   Ls(L"notelist.edit").c_str(),   SIID_RENAME);
    addItem(hNoteMenu, ID_NL_NOTE_RENAME, Ls(L"notelist.rename").c_str(), SIID_DOCASSOC);
    addItemRes(hNoteMenu, ID_NL_NOTE_DELETE, Ls(L"notelist.delete").c_str(), IDI_DELETE);
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hNoteMenu),
                Ls(L"notelist.note").c_str());

    // Folder menu
    HMENU hFolderMenu = CreatePopupMenu();
    addItem(hFolderMenu, ID_NL_FOLDER_NEW,    Ls(L"folder.new").c_str(),    SIID_FOLDER);
    addItem(hFolderMenu, ID_NL_FOLDER_RENAME, Ls(L"folder.rename").c_str(), SIID_RENAME);
    addItemRes(hFolderMenu, ID_NL_FOLDER_DELETE, Ls(L"folder.delete").c_str(), IDI_DELETE);
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFolderMenu),
                Ls(L"notelist.folder_menu").c_str());

    // Options menu
    HMENU hOptionsMenu = CreatePopupMenu();
    {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP;
        mii.wID        = ID_NL_SETTINGS;
        mii.dwTypeData = const_cast<wchar_t*>(Ls(L"menu.settings").c_str());
        mii.hbmpItem   = app.GetResourceBitmap(IDI_SETTINGS);
        InsertMenuItemW(hOptionsMenu, GetMenuItemCount(hOptionsMenu), TRUE, &mii);
    }
    AppendMenuW(hOptionsMenu, MF_SEPARATOR, 0, nullptr);
    addItemRes(hOptionsMenu, ID_NL_ABOUT, Ls(L"menu.about").c_str(), IDI_ABOUT);
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
    CreateSearchEdit();
    CreateStatusBar();
    CreateFolderList();
    CreateListView();
    LoadSettings();
    return true;
}

void NoteListWindow::Show() {
    Refresh();
    // Restore if minimized, then force to foreground. SetForegroundWindow
    // alone can fail due to Windows' foreground-lock — the AttachThreadInput
    // trick bypasses it when the call isn't coming from the foreground thread.
    if (IsIconic(m_hwnd)) {
        ShowWindow(m_hwnd, SW_RESTORE);
    } else {
        ShowWindow(m_hwnd, SW_SHOW);
    }

    HWND hFore = GetForegroundWindow();
    DWORD myThread   = GetCurrentThreadId();
    DWORD foreThread = hFore ? GetWindowThreadProcessId(hFore, nullptr) : 0;
    bool attached = false;
    if (foreThread && foreThread != myThread) {
        attached = AttachThreadInput(foreThread, myThread, TRUE) != 0;
    }
    BringWindowToTop(m_hwnd);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
    if (attached) {
        AttachThreadInput(foreThread, myThread, FALSE);
    }

    if (m_previewEnabled) StartPreviewTimer();
}

void NoteListWindow::Hide() {
    StopPreviewTimer();
    HidePreviewNote();
    Application::Get().SetSearchHighlight(L"");
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

            // Search edit change - live filter
            if (reinterpret_cast<HWND>(lParam) == m_hSearchEdit && code == EN_CHANGE) {
                int len = GetWindowTextLengthW(m_hSearchEdit);
                if (len > 0) {
                    m_searchQuery.resize(static_cast<size_t>(len));
                    GetWindowTextW(m_hSearchEdit, &m_searchQuery[0], len + 1);
                } else {
                    m_searchQuery.clear();
                }
                Application::Get().SetSearchHighlight(m_searchQuery);
                PopulateList();
                ApplyCurrentSort();
                return 0;
            }

            // Folder ListBox selection change
            if (reinterpret_cast<HWND>(lParam) == m_hFolderList && code == LBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
                if (sel == 0) {
                    m_folderFilter = FolderFilter::All;
                    m_selectedFolder.clear();
                } else if (sel == 1) {
                    m_folderFilter = FolderFilter::Unfiled;
                    m_selectedFolder.clear();
                } else if (sel > 1) {
                    int len = static_cast<int>(SendMessageW(m_hFolderList, LB_GETTEXTLEN, sel, 0));
                    if (len > 0) {
                        std::wstring text(static_cast<size_t>(len), L'\0');
                        SendMessageW(m_hFolderList, LB_GETTEXT, sel,
                                     reinterpret_cast<LPARAM>(&text[0]));
                        m_folderFilter = FolderFilter::Named;
                        m_selectedFolder = text;
                    }
                }
                PopulateList();
                ApplyCurrentSort();
                return 0;
            }

            // Column visibility toggle (from header right-click menu)
            if (id >= ID_NL_COLVIS_BASE && id <= ID_NL_COLVIS_MAX) {
                int colIdx = id - ID_NL_COLVIS_BASE;
                if (colIdx >= 0 && colIdx < COL_COUNT) {
                    SetColumnVisible(colIdx, !m_columnVisible[colIdx]);
                    SaveSettings();
                }
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
                // Collect all selected note IDs first — SetNoteFolder triggers
                // RefreshNoteList which repopulates the ListView and clears selection
                std::vector<uint64_t> ids;
                int idx = -1;
                while ((idx = ListView_GetNextItem(m_hListView, idx, LVNI_SELECTED)) >= 0) {
                    LVITEMW item = {};
                    item.mask  = LVIF_PARAM;
                    item.iItem = idx;
                    ListView_GetItem(m_hListView, &item);
                    ids.push_back(static_cast<uint64_t>(item.lParam));
                }
                for (uint64_t noteId : ids) {
                    Application::Get().SetNoteFolder(noteId, targetFolder);
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
                case ID_NL_NOTE_ALARM:
                    OpenAlarmForSelected();
                    return 0;
                case ID_NL_NOTE_DELETE:
                    DeleteSelectedNotes();
                    return 0;
                case ID_NL_SETTINGS:
                    Application::Get().ShowSettingsDialog();
                    return 0;
                case ID_NL_ABOUT:
                    Application::Get().ShowAboutDialog(m_hwnd);
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
                    // Rename: skip fixed entries (0 = "All Notes", 1 = "Unfiled")
                    int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
                    if (sel > 1) {
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
                    if (sel > 1) {
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
                    case LVN_COLUMNCLICK:
                        // Handled by HeaderSubclassProc (WM_LBUTTONUP)
                        return 0;
                    case NM_CLICK: {
                        LVHITTESTINFO htInfo = {};
                        GetCursorPos(&htInfo.pt);
                        ScreenToClient(m_hListView, &htInfo.pt);
                        int hitIdx = ListView_SubItemHitTest(m_hListView, &htInfo);
                        if (hitIdx >= 0 && (htInfo.iSubItem == COL_HIDDEN || htInfo.iSubItem == COL_ONTOP)) {
                            LVITEMW lvItem = {};
                            lvItem.mask  = LVIF_PARAM;
                            lvItem.iItem = hitIdx;
                            ListView_GetItem(m_hListView, &lvItem);
                            uint64_t noteId = static_cast<uint64_t>(lvItem.lParam);
                            if (htInfo.iSubItem == COL_HIDDEN) {
                                ToggleNoteHidden(noteId);
                            } else {
                                ToggleNoteAlwaysOnTop(noteId);
                            }
                        }
                        break;
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

                        // Configurable shortcuts from settings
                        {
                            auto settings = SettingsDialog::LoadFromStorage();
                            if (MatchesShortcut(settings.shortcuts[SC_DELETE], kd->wVKey)) {
                                DeleteSelectedNotes();
                                return 0;
                            }
                            if (MatchesShortcut(settings.shortcuts[SC_ALWAYS_ON_TOP], kd->wVKey)) {
                                // Toggle always-on-top for all selected notes
                                int idx = -1;
                                while ((idx = ListView_GetNextItem(m_hListView, idx, LVNI_SELECTED)) != -1) {
                                    LVITEMW item = {};
                                    item.mask = LVIF_PARAM;
                                    item.iItem = idx;
                                    if (ListView_GetItem(m_hListView, &item)) {
                                        uint64_t noteId = static_cast<uint64_t>(item.lParam);
                                        ToggleNoteAlwaysOnTop(noteId);
                                        NoteData* note = Application::Get().FindNoteData(noteId);
                                        if (note) {
                                            ListView_SetItemText(m_hListView, idx, COL_ONTOP,
                                                const_cast<LPWSTR>(note->layout.alwaysOnTop ? L"\u2611" : L"\u2610"));
                                        }
                                    }
                                }
                                return 0;
                            }
                        }
                        break;
                    }
                    case NM_CUSTOMDRAW: {
                        auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
                        switch (cd->nmcd.dwDrawStage) {
                            case CDDS_PREPAINT:
                                return CDRF_NOTIFYITEMDRAW;
                            case CDDS_ITEMPREPAINT: {
                                // Zebra striping for odd rows
                                int itemIdx = static_cast<int>(cd->nmcd.dwItemSpec);
                                bool selected = (ListView_GetItemState(m_hListView, itemIdx, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                                if (!selected && m_zebraStriping && (itemIdx % 2 == 1)) {
                                    cd->clrTextBk = RGB(245, 245, 245);
                                }

                                // Highlight rows with an alarm firing today
                                if (!selected) {
                                    LVITEMW it = {};
                                    it.mask = LVIF_PARAM;
                                    it.iItem = itemIdx;
                                    if (ListView_GetItem(m_hListView, &it)) {
                                        NoteData* note = Application::Get().FindNoteData(
                                            static_cast<uint64_t>(it.lParam));
                                        if (note && note->alarm.has_value() && !note->alarm->paused) {
                                            SYSTEMTIME now; GetLocalTime(&now);
                                            auto next = AlarmScheduler::ComputeNextFireTime(*note->alarm, now);
                                            if (next.has_value() &&
                                                next->wYear == now.wYear &&
                                                next->wMonth == now.wMonth &&
                                                next->wDay == now.wDay) {
                                                cd->clrText = RGB(200, 30, 30);
                                            }
                                        }
                                    }
                                }
                                return CDRF_NOTIFYSUBITEMDRAW;
                            }
                            case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
                                int sub = cd->iSubItem;
                                int itemIdx = static_cast<int>(cd->nmcd.dwItemSpec);

                                // Check if item is selected + focus state for correct colors
                                bool selected = (ListView_GetItemState(m_hListView, itemIdx, LVIS_SELECTED) & LVIS_SELECTED) != 0;
                                HWND hFocus = GetFocus();
                                bool hasFocus = (hFocus == m_hListView || IsChild(m_hwnd, hFocus));
                                COLORREF bgColor, txColor;
                                if (selected && hasFocus) {
                                    bgColor = GetSysColor(COLOR_HIGHLIGHT);
                                    txColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
                                } else if (selected) {
                                    bgColor = GetSysColor(COLOR_BTNFACE);
                                    txColor = GetSysColor(COLOR_BTNTEXT);
                                } else {
                                    bgColor = GetSysColor(COLOR_WINDOW);
                                    txColor = GetSysColor(COLOR_WINDOWTEXT);
                                    // Apply zebra striping
                                    if (m_zebraStriping && (itemIdx % 2 == 1)) {
                                        bgColor = RGB(245, 245, 245);
                                    }
                                }

                                if (sub == COL_HIDDEN || sub == COL_ONTOP) {
                                    RECT rc = {};
                                    ListView_GetSubItemRect(m_hListView, itemIdx, sub, LVIR_BOUNDS, &rc);

                                    wchar_t buf[4] = {};
                                    ListView_GetItemText(m_hListView, itemIdx, sub, buf, 4);

                                    HFONT hLargeFont = CreateFontW(
                                        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                        DEFAULT_PITCH, L"Segoe UI Symbol");
                                    HFONT hOldFont = static_cast<HFONT>(
                                        SelectObject(cd->nmcd.hdc, hLargeFont));

                                    SetBkColor(cd->nmcd.hdc, bgColor);
                                    SetTextColor(cd->nmcd.hdc, txColor);
                                    ExtTextOutW(cd->nmcd.hdc, 0, 0, ETO_OPAQUE,
                                                &rc, nullptr, 0, nullptr);

                                    DrawTextW(cd->nmcd.hdc, buf, -1, &rc,
                                              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                                    SelectObject(cd->nmcd.hdc, hOldFont);
                                    DeleteObject(hLargeFont);
                                    return CDRF_SKIPDEFAULT;
                                }
                                if (sub == COL_ATTACH) {
                                    RECT rc = {};
                                    ListView_GetSubItemRect(m_hListView, itemIdx, sub, LVIR_BOUNDS, &rc);

                                    HBRUSH hBg = CreateSolidBrush(bgColor);
                                    FillRect(cd->nmcd.hdc, &rc, hBg);
                                    DeleteObject(hBg);

                                    wchar_t buf[4] = {};
                                    ListView_GetItemText(m_hListView, itemIdx, sub, buf, 4);
                                    if (buf[0] != L'\0') {
                                        int iconCx = GetSystemMetrics(SM_CXSMICON);
                                        int iconCy = GetSystemMetrics(SM_CYSMICON);
                                        int iconX = rc.left + (rc.right - rc.left - iconCx) / 2;
                                        int iconY = rc.top + (rc.bottom - rc.top - iconCy) / 2;
                                        HICON hIcon = static_cast<HICON>(LoadImageW(
                                            GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_ATTACHMENT),
                                            IMAGE_ICON, iconCx, iconCy, LR_DEFAULTCOLOR));
                                        if (hIcon) {
                                            DrawIconEx(cd->nmcd.hdc, iconX, iconY, hIcon,
                                                       iconCx, iconCy, 0, nullptr, DI_NORMAL);
                                            DestroyIcon(hIcon);
                                        }
                                    }
                                    return CDRF_SKIPDEFAULT;
                                }
                                return CDRF_DODEFAULT;
                            }
                        }
                        break;
                    }
                }
            }
            // Header NM_CUSTOMDRAW: CDDS_ITEMPREPAINT/POSTPAINT are never
            // delivered (ListView swallows the return value from CDDS_PREPAINT).
            // The gray header appearance comes from SetWindowTheme alone.
            // Sort arrows are set via HDF_SORTUP/HDF_SORTDOWN in SortByColumn().
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
                    case ID_NL_NOTE_ALARM:  key = L"notelist.tb_alarm";    break;
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
                bool isUnfiled  = (dis->itemID == 1);

                // Background
                COLORREF bgColor;
                COLORREF txColor;
                if (isSelected) {
                    bgColor = GetSysColor(COLOR_HIGHLIGHT);
                    txColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
                } else if (isAllNotes || isUnfiled) {
                    bgColor = RGB(230, 230, 230); // Light gray for fixed entries
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
                int iconX = dis->rcItem.left + 3;
                HICON hIcon = isAllNotes ? m_hAllNotesIcon : m_hFolderIcon;
                if (hIcon) {
                    DrawIconEx(dis->hDC, iconX, iconY,
                               hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
                }
                // "Unfiled": overlay a diagonal red line over the folder icon
                if (isUnfiled) {
                    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(200, 40, 40));
                    HPEN hOldPen = static_cast<HPEN>(SelectObject(dis->hDC, hPen));
                    MoveToEx(dis->hDC, iconX + 1,            iconY + iconSize - 1, nullptr);
                    LineTo  (dis->hDC, iconX + iconSize - 1, iconY + 1);
                    SelectObject(dis->hDC, hOldPen);
                    DeleteObject(hPen);
                }
                int textLeft = iconX + iconSize + 4;

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
            if (wParam == IDT_HEADER_DRAG) {
                UpdateHeaderDragOverlay();
                return 0;
            }
            if (wParam == IDT_PREVIEW && m_previewEnabled && m_hListView) {
                POINT pt;
                GetCursorPos(&pt);
                POINT clientPt = pt;
                ScreenToClient(m_hListView, &clientPt);

                RECT lvRect;
                GetClientRect(m_hListView, &lvRect);

                int hitIdx = -1;
                // Only show preview if cursor is actually over our ListView
                // (also allow the preview note window itself, to avoid blink loops)
                HWND hwndUnderCursor = WindowFromPoint(pt);
                bool cursorOverList = (hwndUnderCursor == m_hListView ||
                    hwndUnderCursor == ListView_GetHeader(m_hListView));
                if (!cursorOverList && m_previewNoteId > 0) {
                    NoteWindow* previewWnd = Application::Get().FindNoteWindow(m_previewNoteId);
                    if (previewWnd && hwndUnderCursor == previewWnd->GetHwnd())
                        cursorOverList = true;
                }
                if (PtInRect(&lvRect, clientPt) && cursorOverList) {
                    LVHITTESTINFO htInfo = {};
                    htInfo.pt = clientPt;
                    hitIdx = ListView_SubItemHitTest(m_hListView, &htInfo);
                    // Only preview when hovering over the Note column
                    if (hitIdx >= 0 && htInfo.iSubItem != COL_TEXT)
                        hitIdx = -1;
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
                    DWORD previewDelay = static_cast<DWORD>(SettingsDialog::LoadFromStorage().previewDelay);
                    if (elapsed >= previewDelay) {
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

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                return 0;
            }
            break;

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
    m_hToolbarImages = ImageList_Create(cx, cy, ILC_COLOR32, 7, 1);

    // Icon sources: -1 = resource icon, otherwise SHSTOCKICONID
    struct { int siid; UINT resId; } icons[] = {
        { -1,                  IDI_NEW },      // 0: New
        { SIID_RENAME,         0 },            // 1: Edit
        { -1,                  IDI_DELETE },   // 2: Delete
        { -1,                  IDI_SHOW_ALL }, // 3: Show All
        { -1,                  IDI_HIDE_ALL }, // 4: Hide All
        { SIID_DOCASSOC,       0 },            // 5: Rename
        { -1,                  IDI_ALARM },    // 6: Alarm
    };
    for (auto& icon : icons) {
        HBITMAP hBmp = nullptr;
        if (icon.siid == -1) {
            hBmp = LoadResourceMenuBitmap(icon.resId);
        } else {
            hBmp = LoadShellMenuBitmap(static_cast<SHSTOCKICONID>(icon.siid));
        }
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
        { 2, ID_NL_NOTE_DELETE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 6, ID_NL_NOTE_ALARM,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 0, 0,                 0,               BTNS_SEP,    {0}, 0, 0 },
        { 3, ID_NL_SHOW_ALL,    TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 4, ID_NL_HIDE_ALL,    TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
    };

    SendMessageW(m_hToolbar, TB_ADDBUTTONS, _countof(buttons),
                 reinterpret_cast<LPARAM>(buttons));
    SendMessageW(m_hToolbar, TB_AUTOSIZE, 0, 0);
}

// ============================================================================
// Search edit in toolbar
// ============================================================================

void NoteListWindow::CreateSearchEdit() {
    if (!m_hToolbar) return;

    // Get toolbar height for vertical centering
    RECT tbRect;
    GetWindowRect(m_hToolbar, &tbRect);
    int tbH = tbRect.bottom - tbRect.top;

    int editH = 22;
    int editW = 160;
    int editY = (tbH - editH) / 2;

    // Position will be set properly in ResizeControls; create with placeholder coords
    m_hSearchEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, editY, editW, editH,
        m_hToolbar,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SEARCH_EDIT)),
        m_hInst, nullptr
    );

    if (m_hSearchEdit) {
        // Use the modern system message font (Segoe UI 9pt on Win 10/11) to
        // match the look of the InputDialog (Folder Rename / Note Rename),
        // instead of DEFAULT_GUI_FONT (MS Sans Serif 8pt).
        if (!m_hSearchFont) {
            NONCLIENTMETRICSW ncm = { sizeof(ncm) };
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            m_hSearchFont = CreateFontIndirectW(&ncm.lfMessageFont);
        }
        HFONT hFont = m_hSearchFont
                          ? m_hSearchFont
                          : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(m_hSearchEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        // Load magnifier icon at 16x16 for in-field rendering
        if (!m_hSearchIcon) {
            m_hSearchIcon = static_cast<HICON>(LoadImageW(
                m_hInst, MAKEINTRESOURCEW(IDI_SEARCH), IMAGE_ICON,
                16, 16, LR_DEFAULTCOLOR));
        }

        // Set cue banner (placeholder text)
        SendMessageW(m_hSearchEdit, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(Ls(L"notelist.search_placeholder").c_str()));

        // Subclass for ESC handling and magnifier overlay paint (NC-strip)
        SetWindowSubclass(m_hSearchEdit, SearchEditSubclassProc, 0,
                          reinterpret_cast<DWORD_PTR>(this));

        // Force WM_NCCALCSIZE recalculation so the right-side NC strip is reserved
        SetWindowPos(m_hSearchEdit, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

LRESULT CALLBACK NoteListWindow::SearchEditSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR /*subId*/, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<NoteListWindow*>(refData);

    // Width of the NC strip reserved on the right side for the magnifier.
    // 16 px icon + 2 px padding on each side.
    static constexpr int kIconStripW = 20;
    static constexpr int kIconSize   = 16;

    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        // Clear search and move focus to listview
        SetWindowTextW(hwnd, L"");
        if (self->m_hListView)
            SetFocus(self->m_hListView);
        return 0;
    }

    // Reserve a non-client strip on the right of the EDIT for the magnifier.
    // EDIT cannot paint into NC area, so the icon survives in-place character
    // updates that bypass WM_PAINT (typing path uses GetDC + ExtTextOut and
    // wipes the entire client rect — but never the NC area).
    if (msg == WM_NCCALCSIZE && wParam == TRUE) {
        LRESULT lr = DefSubclassProc(hwnd, msg, wParam, lParam);
        auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        if (p->rgrc[0].right - p->rgrc[0].left > kIconStripW) {
            p->rgrc[0].right -= kIconStripW;
        }
        return lr;
    }

    if (msg == WM_NCPAINT && self && self->m_hSearchIcon) {
        // Let the default first draw the WS_EX_CLIENTEDGE frame
        LRESULT lr = DefSubclassProc(hwnd, msg, wParam, lParam);

        HDC hdc = GetWindowDC(hwnd);
        if (hdc) {
            RECT wr;
            GetWindowRect(hwnd, &wr);
            const int ww = wr.right  - wr.left;
            const int wh = wr.bottom - wr.top;

            // CLIENTEDGE frame thickness (typically 2 px)
            const int fx = GetSystemMetrics(SM_CXEDGE);
            const int fy = GetSystemMetrics(SM_CYEDGE);

            // The strip we reserved sits just inside the right frame
            RECT strip = {
                ww - fx - kIconStripW,
                fy,
                ww - fx,
                wh - fy
            };

            FillRect(hdc, &strip, GetSysColorBrush(COLOR_WINDOW));

            const int iconX = strip.left + (kIconStripW - kIconSize) / 2;
            const int iconY = strip.top  + ((strip.bottom - strip.top) - kIconSize) / 2;
            DrawIconEx(hdc, iconX, iconY,
                       self->m_hSearchIcon, kIconSize, kIconSize,
                       0, nullptr, DI_NORMAL);

            ReleaseDC(hwnd, hdc);
        }
        return lr;
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, SearchEditSubclassProc, 0);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK NoteListWindow::HeaderSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR /*subId*/, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<NoteListWindow*>(refData);

    if (msg == WM_LBUTTONDOWN) {
        if (self) self->m_headerMouseDown = true;
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_LBUTTONUP) {
        bool wasDown = self && self->m_headerMouseDown;
        if (self) self->m_headerMouseDown = false;

        HDHITTESTINFO htInfo = {};
        htInfo.pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        SendMessageW(hwnd, HDM_HITTEST, 0, reinterpret_cast<LPARAM>(&htInfo));

        // Let default processing finish first (may reset header state)
        LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);

        // Only sort if WM_LBUTTONDOWN was received on the header first
        if (wasDown && htInfo.iItem >= 0 && (htInfo.flags & HHT_ONHEADER)) {
            if (self)
                self->SortByColumn(htInfo.iItem);
        }
        return result;
    }
    if (msg == WM_RBUTTONUP) {
        // Show column-visibility context menu regardless of hit location
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &pt);
        if (self) self->ShowHeaderContextMenu(pt.x, pt.y);
        return 0;
    }
    if (msg == WM_CAPTURECHANGED || msg == WM_CANCELMODE) {
        if (self) self->m_headerMouseDown = false;
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, HeaderSubclassProc, 0);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Header drag-drop feedback (ATnotes-style red insertion arrows)
// ============================================================================

LRESULT CALLBACK NoteListWindow::ListViewSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR /*subId*/, DWORD_PTR refData)
{
    auto* self = reinterpret_cast<NoteListWindow*>(refData);
    if (msg == WM_NOTIFY && self) {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm->code == HDN_BEGINDRAG) {
            auto* hd = reinterpret_cast<NMHEADERW*>(lParam);
            self->OnHeaderBeginDrag(hd->iItem);
        } else if (nm->code == HDN_ENDDRAG) {
            self->OnHeaderEndDrag();
        }
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, ListViewSubclassProc, 0);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void NoteListWindow::OnHeaderBeginDrag(int sourceIdx) {
    m_headerDragging   = true;
    m_headerDragSource = sourceIdx;
    if (!m_headerDragOverlay)
        m_headerDragOverlay = std::make_unique<HeaderDragOverlay>(m_hInst);
    SetTimer(m_hwnd, IDT_HEADER_DRAG, HEADER_DRAG_POLL_MS, nullptr);
    UpdateHeaderDragOverlay();
}

void NoteListWindow::OnHeaderEndDrag() {
    m_headerDragging   = false;
    m_headerDragSource = -1;
    KillTimer(m_hwnd, IDT_HEADER_DRAG);
    if (m_headerDragOverlay) m_headerDragOverlay->Hide();
}

void NoteListWindow::UpdateHeaderDragOverlay() {
    if (!m_headerDragging || !m_headerDragOverlay || !m_hListView) return;

    HWND hHeader = ListView_GetHeader(m_hListView);
    if (!hHeader) return;

    int count = Header_GetItemCount(hHeader);
    if (count <= 0) { m_headerDragOverlay->Hide(); return; }

    POINT pt;
    if (!GetCursorPos(&pt)) return;
    POINT ptHdr = pt;
    ScreenToClient(hHeader, &ptHdr);

    // Visual order → logical-index array
    std::vector<int> order(static_cast<size_t>(count));
    Header_GetOrderArray(hHeader, count, order.data());

    // Determine insertion slot (0..count). Slot i means: insert before visual item i.
    int insertSlot = count;
    for (int i = 0; i < count; ++i) {
        RECT rc;
        Header_GetItemRect(hHeader, order[i], &rc);
        int mid = (rc.left + rc.right) / 2;
        if (ptHdr.x < mid) { insertSlot = i; break; }
    }

    // X of the gap in header client coordinates
    int gapXClient;
    if (insertSlot == 0) {
        RECT rc; Header_GetItemRect(hHeader, order[0], &rc);
        gapXClient = rc.left;
    } else if (insertSlot >= count) {
        RECT rc; Header_GetItemRect(hHeader, order[count - 1], &rc);
        gapXClient = rc.right;
    } else {
        RECT rcL; Header_GetItemRect(hHeader, order[insertSlot - 1], &rcL);
        RECT rcR; Header_GetItemRect(hHeader, order[insertSlot],     &rcR);
        gapXClient = (rcL.right + rcR.left) / 2;
    }

    POINT gap = { gapXClient, 0 };
    ClientToScreen(hHeader, &gap);

    RECT hdrRc;
    GetWindowRect(hHeader, &hdrRc);

    m_headerDragOverlay->Show(gap.x, hdrRc.top, hdrRc.bottom);
}

void NoteListWindow::FocusSearchField() {
    if (m_hSearchEdit) {
        SetFocus(m_hSearchEdit);
        SendMessageW(m_hSearchEdit, EM_SETSEL, 0, -1);
    }
}

// ============================================================================
// Status bar
// ============================================================================

void NoteListWindow::CreateStatusBar() {
    m_hStatusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd, nullptr, m_hInst, nullptr
    );
}

void NoteListWindow::UpdateStatusBar() {
    if (!m_hStatusBar) return;
    int total = static_cast<int>(Application::Get().GetAllNotes().size());
    int shown = ListView_GetItemCount(m_hListView);
    const wchar_t* unitKey = (total == 1) ? L"notelist.status_notes_one" : L"notelist.status_notes";
    std::wstring text = (shown == total)
        ? FormatString(L"%d %s", total, Ls(unitKey).c_str())
        : FormatString(L"%d / %d %s", shown, total, Ls(unitKey).c_str());
    SendMessageW(m_hStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
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

    // Index 0: "All Notes"
    SendMessageW(m_hFolderList, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(Ls(L"note.all_folders").c_str()));
    // Index 1: "Unfiled" (notes without any folder assignment)
    SendMessageW(m_hFolderList, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(Ls(L"note.unfiled").c_str()));

    // Index 2+: User-defined folders (sorted)
    auto& folders = Application::Get().GetFolders();
    for (const auto& f : folders) {
        SendMessageW(m_hFolderList, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(f.c_str()));
    }

    // Restore selection
    if (m_folderFilter == FolderFilter::All) {
        SendMessageW(m_hFolderList, LB_SETCURSEL, 0, 0);
    } else if (m_folderFilter == FolderFilter::Unfiled) {
        SendMessageW(m_hFolderList, LB_SETCURSEL, 1, 0);
    } else {
        int count = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCOUNT, 0, 0));
        bool found = false;
        for (int i = 2; i < count; ++i) {
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
            m_folderFilter = FolderFilter::All;
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
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER |
        LVS_EX_HEADERDRAGDROP);

    SetupColumns();

    // Disable theme on header so our NM_CUSTOMDRAW gray background works
    HWND hHeader = ListView_GetHeader(m_hListView);
    if (hHeader) {
        SetWindowTheme(hHeader, L"", L"");
        // Subclass header for reliable click detection — the unthemed header's
        // internal state machine sometimes fails to fire HDN_ITEMCLICK/LVN_COLUMNCLICK
        SetWindowSubclass(hHeader, HeaderSubclassProc, 0,
                          reinterpret_cast<DWORD_PTR>(this));
    }

    // Subclass the ListView itself to observe HDN_BEGINDRAG/HDN_ENDDRAG
    // (header notifications are sent to the header's parent = listview, and are
    // not reflected further up to our NoteListWindow).
    SetWindowSubclass(m_hListView, ListViewSubclassProc, 0,
                      reinterpret_cast<DWORD_PTR>(this));

    // Default sort by title column (ascending)
    m_sortColumn = COL_TITLE;
    m_sortAscending = true;
    // Set initial sort arrow on header
    if (hHeader) {
        HDITEMW hdi = {};
        hdi.mask = HDI_FORMAT;
        Header_GetItem(hHeader, COL_TITLE, &hdi);
        hdi.fmt |= HDF_SORTUP;
        Header_SetItem(hHeader, COL_TITLE, &hdi);
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

    col.cx = 70;
    col.fmt = LVCFMT_CENTER;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_hidden").c_str());
    ListView_InsertColumn(m_hListView, COL_HIDDEN, &col);

    col.cx = 70;
    col.fmt = LVCFMT_CENTER;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_ontop").c_str());
    ListView_InsertColumn(m_hListView, COL_ONTOP, &col);

    col.fmt = LVCFMT_LEFT;
    col.cx = 130;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_created").c_str());
    ListView_InsertColumn(m_hListView, COL_CREATED, &col);

    col.cx = 30;
    col.fmt = LVCFMT_CENTER;
    col.pszText = const_cast<LPWSTR>(L"");
    ListView_InsertColumn(m_hListView, COL_ATTACH, &col);

    col.fmt = LVCFMT_LEFT;
    col.cx = 130;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_next_alarm").c_str());
    ListView_InsertColumn(m_hListView, COL_NEXT_ALARM, &col);

    col.cx = 150;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_interval").c_str());
    ListView_InsertColumn(m_hListView, COL_INTERVAL, &col);

    col.cx = 60;
    col.fmt = LVCFMT_CENTER;
    col.pszText = const_cast<LPWSTR>(Ls(L"notelist.col_alarm_status").c_str());
    ListView_InsertColumn(m_hListView, COL_ALARM_STATUS, &col);

    // Visual order: Attach first, then the rest (alarm columns appended at the end)
    int order[COL_COUNT] = { COL_ATTACH, COL_TITLE, COL_TEXT, COL_FOLDER,
                             COL_HIDDEN, COL_ONTOP, COL_CREATED,
                             COL_NEXT_ALARM, COL_INTERVAL, COL_ALARM_STATUS };
    ListView_SetColumnOrderArray(m_hListView, COL_COUNT, order);

    // All columns visible by default
    for (int i = 0; i < COL_COUNT; ++i) {
        m_columnVisible[i] = true;
        m_columnSavedWidths[i] = ListView_GetColumnWidth(m_hListView, i);
    }
}

void NoteListWindow::PopulateList() {
    ListView_DeleteAllItems(m_hListView);

    // Determine date format once (0 = YYYY-MM-DD HH:MM, 1 = DD.MM.YYYY HH:MM)
    auto intSettings = Storage::LoadSettings();
    auto itDateFmt = intSettings.find(L"notelist.dateFormat");
    const wchar_t* dateFmt = L"%Y-%m-%d %H:%M";
    if (itDateFmt != intSettings.end() && itDateFmt->second == 1) {
        dateFmt = L"%d.%m.%Y %H:%M";
    }

    auto& notes = Application::Get().GetAllNotes();

    int insertIdx = 0;
    for (size_t i = 0; i < notes.size(); ++i) {
        auto& note = *notes[i];

        // Filter by selected folder
        if (m_folderFilter == FolderFilter::Unfiled) {
            if (!note.folder.empty()) continue;
        } else if (m_folderFilter == FolderFilter::Named) {
            if (note.folder != m_selectedFolder) continue;
        }

        // Filter by search query (case-insensitive substring match)
        if (!m_searchQuery.empty()) {
            // Convert search query to lowercase for comparison
            std::wstring queryLower = m_searchQuery;
            for (auto& c : queryLower) c = towlower(c);

            std::wstring titleLower = note.title;
            for (auto& c : titleLower) c = towlower(c);

            std::wstring textLower = note.text;
            for (auto& c : textLower) c = towlower(c);

            if (titleLower.find(queryLower) == std::wstring::npos &&
                textLower.find(queryLower) == std::wstring::npos) {
                continue;
            }
        }

        // Title: use title field, show placeholder if empty
        std::wstring titleDisplay = note.title;
        if (titleDisplay.empty()) {
            titleDisplay = Ls(L"note.untitled");
        }

        // Text: first line
        std::wstring textDisplay = note.text;
        auto nl = textDisplay.find(L'\n');
        if (nl != std::wstring::npos) textDisplay = textDisplay.substr(0, nl);
        if (textDisplay.empty()) textDisplay = Ls(L"note.empty");

        // Folder display
        std::wstring folderDisplay = note.folder.empty()
            ? Ls(L"note.no_folder") : note.folder;

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

        // Format timestamp (prefer modifiedAt, fallback to createdAt)
        {
            int64_t ts = note.modifiedAt > 0 ? note.modifiedAt : note.createdAt;
            if (ts > 0) {
                time_t t = static_cast<time_t>(ts);
                struct tm tm = {};
                localtime_s(&tm, &t);
                wchar_t buf[64];
                wcsftime(buf, 64, dateFmt, &tm);
                ListView_SetItemText(m_hListView, idx, COL_CREATED, buf);
            }
        }

        // Checkbox columns
        ListView_SetItemText(m_hListView, idx, COL_HIDDEN,
                             const_cast<LPWSTR>(note.isHidden ? L"\u2611" : L"\u2610"));
        ListView_SetItemText(m_hListView, idx, COL_ONTOP,
                             const_cast<LPWSTR>(note.layout.alwaysOnTop ? L"\u2611" : L"\u2610"));

        // Attachment indicator
        if (!note.attachments.empty())
            ListView_SetItemText(m_hListView, idx, COL_ATTACH, const_cast<LPWSTR>(L"\x1"));

        // Alarm columns
        if (note.alarm.has_value()) {
            SYSTEMTIME now;
            GetLocalTime(&now);
            auto next = AlarmScheduler::ComputeNextFireTime(*note.alarm, now);

            std::wstring nextStr;
            std::wstring statusStr;
            if (!next.has_value()) {
                nextStr = Ls(L"alarm.status.expired_short");
                statusStr = L"\u25CB"; // hollow circle
            } else {
                wchar_t dateBuf[64], timeBuf[32];
                GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &*next, nullptr, dateBuf, 64);
                GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &*next, nullptr, timeBuf, 32);
                nextStr = std::wstring(dateBuf) + L" " + timeBuf;

                // Status symbol: paused = hollow, else solid
                if (note.alarm->paused) statusStr = L"\u25CB"; // hollow circle
                else statusStr = L"\u25CF"; // solid circle
            }

            ListView_SetItemText(m_hListView, idx, COL_NEXT_ALARM,
                                 const_cast<LPWSTR>(nextStr.c_str()));
            std::wstring intervalStr = AlarmScheduler::DescribeInterval(*note.alarm);
            ListView_SetItemText(m_hListView, idx, COL_INTERVAL,
                                 const_cast<LPWSTR>(intervalStr.c_str()));
            ListView_SetItemText(m_hListView, idx, COL_ALARM_STATUS,
                                 const_cast<LPWSTR>(statusStr.c_str()));
        }

        ++insertIdx;
    }

    UpdateStatusBar();
}

void NoteListWindow::Refresh() {
    if (!m_hListView) return;
    // Cache display settings to avoid repeated LoadFromStorage calls
    auto settings = SettingsDialog::LoadFromStorage();
    m_zebraStriping = settings.zebraStriping;

    PopulateFolderList();
    PopulateList();
    ApplyCurrentSort();
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

        // Position search edit at right side of toolbar
        if (m_hSearchEdit) {
            int editW = 160;
            int editH = 22;
            int editY = (tbHeight - editH) / 2;
            int editX = rc.right - editW - 4;
            if (editX < 0) editX = 0;
            MoveWindow(m_hSearchEdit, editX, editY, editW, editH, TRUE);
        }
    }

    int sbHeight = 0;
    if (m_hStatusBar) {
        SendMessageW(m_hStatusBar, WM_SIZE, 0, 0);
        RECT sbRect;
        GetWindowRect(m_hStatusBar, &sbRect);
        sbHeight = sbRect.bottom - sbRect.top;
    }

    int contentHeight = rc.bottom - tbHeight - sbHeight;

    if (m_hFolderList) {
        MoveWindow(m_hFolderList, 0, tbHeight, m_folderListWidth,
                   contentHeight, TRUE);
    }

    int lvLeft = m_folderListWidth + SPLITTER_WIDTH;
    if (m_hListView) {
        MoveWindow(m_hListView, lvLeft, tbHeight,
                   rc.right - lvLeft, contentHeight, TRUE);
    }

    // Invalidate splitter area
    RECT splitterRc = { m_folderListWidth, tbHeight,
                        m_folderListWidth + SPLITTER_WIDTH, rc.bottom - sbHeight };
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

    // Column widths
    if (m_hListView) {
        for (int i = 0; i < COL_COUNT; ++i) {
            wchar_t key[40];
            wsprintfW(key, L"notelist.col%d_width", i);
            auto it = settings.find(key);
            if (it != settings.end() && it->second > 20) {
                ListView_SetColumnWidth(m_hListView, i, it->second);
                m_columnSavedWidths[i] = it->second;
            }
        }

        // Column order (restore user's drag & drop arrangement).
        // If any index is missing (e.g. pre-Phase-7 settings with only 7 columns),
        // discard the saved order entirely and keep the SetupColumns default.
        bool hasOrder = true;
        int order[COL_COUNT] = {};
        for (int i = 0; i < COL_COUNT; ++i) {
            wchar_t key[40];
            wsprintfW(key, L"notelist.col%d_order", i);
            auto it = settings.find(key);
            if (it == settings.end()) { hasOrder = false; break; }
            order[i] = it->second;
        }
        if (hasOrder) {
            ListView_SetColumnOrderArray(m_hListView, COL_COUNT, order);
        }

        // Column visibility (default: all visible)
        for (int i = 0; i < COL_COUNT; ++i) {
            wchar_t key[40];
            wsprintfW(key, L"notelist.col%d_visible", i);
            auto it = settings.find(key);
            bool visible = (it == settings.end()) ? true : (it->second != 0);
            m_columnVisible[i] = true;  // start visible so SetColumnVisible can save width
            SetColumnVisible(i, visible);
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
        for (int i = 0; i < COL_COUNT; ++i) {
            wchar_t key[40];
            // Save the visible width for shown columns; for hidden ones keep the
            // saved width so the user's preferred size is preserved across sessions.
            int width = m_columnVisible[i]
                          ? ListView_GetColumnWidth(m_hListView, i)
                          : m_columnSavedWidths[i];
            wsprintfW(key, L"notelist.col%d_width", i);
            settings[key] = width;

            wsprintfW(key, L"notelist.col%d_visible", i);
            settings[key] = m_columnVisible[i] ? 1 : 0;
        }

        // Save column order
        int order[COL_COUNT] = {};
        ListView_GetColumnOrderArray(m_hListView, COL_COUNT, order);
        for (int i = 0; i < COL_COUNT; ++i) {
            wchar_t key[40];
            wsprintfW(key, L"notelist.col%d_order", i);
            settings[key] = order[i];
        }
    }

    Storage::SaveSettings(settings);
}

// ============================================================================
// Sorting
// ============================================================================

void NoteListWindow::SortByColumn(int col) {
    // Toggle direction if clicking the same column, otherwise reset to ascending
    if (col == m_sortColumn) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortColumn = col;
        m_sortAscending = true;
    }
    ApplyCurrentSort();
}

bool NoteListWindow::IsColumnVisible(int col) const {
    if (col < 0 || col >= COL_COUNT) return false;
    return m_columnVisible[col];
}

void NoteListWindow::SetColumnVisible(int col, bool visible) {
    if (col < 0 || col >= COL_COUNT || !m_hListView) return;
    if (m_columnVisible[col] == visible) return;

    static const int defaultWidths[COL_COUNT] = {
        /*TITLE*/ 150, /*TEXT*/ 200, /*FOLDER*/ 100, /*HIDDEN*/ 70, /*ONTOP*/ 70,
        /*CREATED*/ 130, /*ATTACH*/ 30,
        /*NEXT_ALARM*/ 130, /*INTERVAL*/ 150, /*ALARM_STATUS*/ 60
    };

    if (!visible) {
        // Hide: remember current width, set width to 0
        int cur = ListView_GetColumnWidth(m_hListView, col);
        if (cur > 0) m_columnSavedWidths[col] = cur;
        ListView_SetColumnWidth(m_hListView, col, 0);
    } else {
        // Show: restore saved width (or default if none remembered)
        int w = m_columnSavedWidths[col];
        if (w <= 20) w = defaultWidths[col];
        ListView_SetColumnWidth(m_hListView, col, w);
    }
    m_columnVisible[col] = visible;
}

void NoteListWindow::ShowHeaderContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    static const wchar_t* const kColKeys[COL_COUNT] = {
        L"notelist.col_title",       L"notelist.col_text",
        L"notelist.col_folder",      L"notelist.col_hidden",
        L"notelist.col_ontop",       L"notelist.col_created",
        L"notelist.col_attach",      L"notelist.col_next_alarm",
        L"notelist.col_interval",    L"notelist.col_alarm_status"
    };

    for (int i = 0; i < COL_COUNT; ++i) {
        UINT flags = MF_STRING;
        if (m_columnVisible[i]) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, ID_NL_COLVIS_BASE + i, Ls(kColKeys[i]).c_str());
    }

    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void NoteListWindow::ApplyCurrentSort() {
    if (m_sortColumn < 0 || !m_hListView) return;

    LPARAM sortParam = static_cast<LPARAM>(m_sortColumn) | (m_sortAscending ? 0x10000 : 0);
    ListView_SortItems(m_hListView, CompareFunc, sortParam);

    // Update header sort arrows
    HWND hHeader = ListView_GetHeader(m_hListView);
    if (hHeader) {
        int count = Header_GetItemCount(hHeader);
        for (int i = 0; i < count; ++i) {
            HDITEMW hdi = {};
            hdi.mask = HDI_FORMAT;
            Header_GetItem(hHeader, i, &hdi);
            hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            if (i == m_sortColumn) {
                hdi.fmt |= m_sortAscending ? HDF_SORTUP : HDF_SORTDOWN;
            }
            Header_SetItem(hHeader, i, &hdi);
        }
    }
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
        case COL_CREATED: {
            int64_t t1 = n1->modifiedAt > 0 ? n1->modifiedAt : n1->createdAt;
            int64_t t2 = n2->modifiedAt > 0 ? n2->modifiedAt : n2->createdAt;
            if (t1 < t2) result = -1;
            else if (t1 > t2) result = 1;
            break;
        }
        case COL_HIDDEN:
            result = static_cast<int>(n1->isHidden) - static_cast<int>(n2->isHidden);
            break;
        case COL_ONTOP:
            result = static_cast<int>(n1->layout.alwaysOnTop) - static_cast<int>(n2->layout.alwaysOnTop);
            break;
        case COL_NEXT_ALARM: {
            // Notes without a future alarm sort to the end regardless of direction
            SYSTEMTIME now; GetLocalTime(&now);
            auto f1 = n1->alarm.has_value() ? AlarmScheduler::ComputeNextFireTime(*n1->alarm, now) : std::nullopt;
            auto f2 = n2->alarm.has_value() ? AlarmScheduler::ComputeNextFireTime(*n2->alarm, now) : std::nullopt;
            if (!f1 && !f2) return 0;
            if (!f1) return 1;     // n1 to the end
            if (!f2) return -1;    // n2 to the end
            int cmp = AlarmScheduler::CompareSysTime(*f1, *f2);
            return ascending ? cmp : -cmp;
        }
        case COL_INTERVAL: {
            std::wstring i1 = n1->alarm.has_value() ? AlarmScheduler::DescribeInterval(*n1->alarm) : L"";
            std::wstring i2 = n2->alarm.has_value() ? AlarmScheduler::DescribeInterval(*n2->alarm) : L"";
            if (i1.empty() && i2.empty()) return 0;
            if (i1.empty()) return 1;
            if (i2.empty()) return -1;
            result = _wcsicmp(i1.c_str(), i2.c_str());
            break;
        }
        case COL_ALARM_STATUS: {
            // 0 = no alarm, 1 = paused, 2 = active
            auto stateOf = [](const NoteData* n) {
                if (!n->alarm.has_value()) return 0;
                return n->alarm->paused ? 1 : 2;
            };
            result = stateOf(n1) - stateOf(n2);
            break;
        }
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

    // Active search: skip edit mode so highlighted matches stay visible
    bool enterEdit = m_searchQuery.empty();
    Application::Get().BringNoteToFront(id, enterEdit);
}

void NoteListWindow::OpenAlarmForSelected() {
    // Only single-selection — the alarm dialog configures exactly one note.
    if (ListView_GetSelectedCount(m_hListView) != 1) return;

    int idx = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (idx < 0) return;

    LVITEMW item = {};
    item.mask  = LVIF_PARAM;
    item.iItem = idx;
    ListView_GetItem(m_hListView, &item);
    uint64_t id = static_cast<uint64_t>(item.lParam);

    // Reuse existing dialog only if its HWND is still valid AND it's for the
    // same note. Otherwise destroy/recreate so the dialog always matches the
    // current selection. The dialog disables this window while open.
    if (m_alarmDialog && m_alarmDialog->GetHwnd()) {
        DestroyWindow(m_alarmDialog->GetHwnd());
    }
    m_alarmDialog.reset();
    m_alarmDialog = std::make_unique<AlarmConfigDialog>(m_hInst, m_hwnd, id);
    if (!m_alarmDialog->Create()) {
        m_alarmDialog.reset();
    }
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
                              MB_YESNO | MB_ICONQUESTION);
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
        Application::Get().DeleteNote(id);
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
// Toggle hidden / always-on-top from list
// ============================================================================

void NoteListWindow::ToggleNoteHidden(uint64_t noteId) {
    auto& app = Application::Get();
    NoteData* note = app.FindNoteData(noteId);
    if (!note) return;

    if (note->isHidden) {
        // Show the note (BringNoteToFront already triggers RefreshNoteList)
        app.BringNoteToFront(noteId);
    } else {
        // Hide the note
        note->isHidden = true;
        NoteWindow* wnd = app.FindNoteWindow(noteId);
        if (wnd) wnd->Show(false);
        app.MarkDirty();
        Refresh();
    }
}

void NoteListWindow::ToggleNoteAlwaysOnTop(uint64_t noteId) {
    auto& app = Application::Get();
    NoteData* note = app.FindNoteData(noteId);
    if (!note) return;

    note->layout.alwaysOnTop = !note->layout.alwaysOnTop;
    NoteWindow* wnd = app.FindNoteWindow(noteId);
    if (wnd) {
        SetWindowPos(wnd->GetHwnd(),
                     note->layout.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    app.MarkDirty();
    Refresh();
}

// ============================================================================
// Context menus
// ============================================================================

void NoteListWindow::ShowNoteContextMenu(int screenX, int screenY) {
    int selCount = ListView_GetSelectedCount(m_hListView);
    if (selCount <= 0) return;
    bool multiSelect = (selCount > 1);

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

    // Edit/Rename only for single selection
    UINT singleFlag = multiSelect ? MF_GRAYED : 0;
    addItem(ID_NL_NOTE_EDIT,   Ls(L"notelist.edit").c_str(),   SIID_RENAME,   singleFlag);
    addItem(ID_NL_NOTE_RENAME, Ls(L"notelist.rename").c_str(), SIID_DOCASSOC, singleFlag);
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    // "Set Folder" submenu — for single selection, mark current folder with a check.
    // For multi-select we skip the check (notes may be in different folders).
    std::wstring currentFolder;
    bool haveCurrent = false;
    if (!multiSelect) {
        int idx = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
        if (idx >= 0) {
            LVITEMW item = {};
            item.mask  = LVIF_PARAM;
            item.iItem = idx;
            ListView_GetItem(m_hListView, &item);
            NoteData* nd = app.FindNoteData(static_cast<uint64_t>(item.lParam));
            if (nd) { currentFolder = nd->folder; haveCurrent = true; }
        }
    }

    HMENU hFolderSub = CreatePopupMenu();
    UINT noFolderFlags = MF_STRING;
    if (haveCurrent && currentFolder.empty()) noFolderFlags |= MF_CHECKED;
    AppendMenuW(hFolderSub, noFolderFlags, ID_NL_FOLDER_BASE,
                Ls(L"note.no_folder").c_str());
    AppendMenuW(hFolderSub, MF_SEPARATOR, 0, nullptr);
    auto& folders = app.GetFolders();
    for (size_t i = 0; i < folders.size() && i + 1 < (ID_NL_FOLDER_MAX - ID_NL_FOLDER_BASE); ++i) {
        UINT flags = MF_STRING;
        if (haveCurrent && currentFolder == folders[i]) flags |= MF_CHECKED;
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

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    addItemRes(ID_NL_NOTE_ALARM, Ls(L"notelist.alarm").c_str(), IDI_ALARM, singleFlag);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    addItemRes(ID_NL_NOTE_DELETE, Ls(L"notelist.delete").c_str(), IDI_DELETE);

    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(hPopup);
}

void NoteListWindow::ShowFolderContextMenu(int screenX, int screenY) {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    int sel = static_cast<int>(SendMessageW(m_hFolderList, LB_GETCURSEL, 0, 0));
    // Index 0 = "All Notes", index 1 = "Unfiled" — both fixed entries.
    // Rename/delete only apply to user folders (index >= 2).
    bool isUserFolder = (sel > 1);

    auto& app = Application::Get();

    auto addItem = [&](UINT id, const wchar_t* text, SHSTOCKICONID iconId) {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE;
        mii.fType      = MFT_STRING;
        mii.wID        = id;
        mii.dwTypeData = const_cast<wchar_t*>(text);
        mii.hbmpItem   = app.GetMenuBitmap(iconId);
        InsertMenuItemW(hPopup, GetMenuItemCount(hPopup), TRUE, &mii);
    };

    addItem(ID_NL_FOLDER_NEW, Ls(L"folder.new").c_str(), SIID_FOLDER);
    if (isUserFolder) {
        AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
        addItem(ID_NL_FOLDER_RENAME, Ls(L"folder.rename").c_str(), SIID_RENAME);
        app.AppendMenuItemRes(hPopup, ID_NL_FOLDER_DELETE,
                              Ls(L"folder.delete").c_str(), IDI_DELETE);
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

void NoteListWindow::SetPreviewEnabled(bool enabled) {
    m_previewEnabled = enabled;
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
}

void NoteListWindow::SetPreviewPaused(bool paused) {
    if (paused) {
        StopPreviewTimer();
        HidePreviewNote();
    } else {
        if (m_previewEnabled && IsVisible()) {
            StartPreviewTimer();
        }
    }
}

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

    // Skip preview for notes that are already visible on screen —
    // moving them to the cursor and back causes distracting flickering
    if (app.IsNoteVisible(noteId)) {
        return;
    }

    // Show the note (it was hidden)
    NoteWindow* wnd = app.ShowNotePreview(noteId);
    m_previewNoteId = noteId;
    m_previewWasHidden = true;

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

        int newX = cursorPt.x + 15;
        int newY = cursorPt.y + 15;

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

    // Restore focus and redraw so selection highlight stays blue
    if (m_hListView) {
        SetFocus(m_hListView);
        InvalidateRect(m_hListView, nullptr, FALSE);
    }
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

            // Compact layout: 250px wide content area
            int contentW = 250;
            int margin = 10;
            int btnW = 75;
            int btnH = 24;
            int btnGap = 8;

            // Create static label
            CreateWindowExW(0, L"STATIC", data->prompt.c_str(),
                            WS_CHILD | WS_VISIBLE,
                            margin, 8, contentW, 16,
                            hwnd, nullptr, nullptr, nullptr);

            // Create edit control
            HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", data->value.c_str(),
                                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          margin, 28, contentW, 22,
                                          hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100)),
                                          nullptr, nullptr);

            // OK / Cancel buttons (right-aligned)
            int btnY = 58;
            int cancelX = margin + contentW - btnW;
            int okX = cancelX - btnGap - btnW;

            CreateWindowExW(0, L"BUTTON", Ls(L"settings.ok").c_str(),
                            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                            okX, btnY, btnW, btnH,
                            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
                            nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", Ls(L"settings.cancel").c_str(),
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            cancelX, btnY, btnW, btnH,
                            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
                            nullptr, nullptr);

            // Use the modern system message font (Segoe UI 9pt on Win 10/11)
            // instead of DEFAULT_GUI_FONT (MS Sans Serif 8pt) so labels,
            // edit and buttons match the visual weight of the rest of the OS.
            NONCLIENTMETRICSW ncm = { sizeof(ncm) };
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            HFONT hFont = CreateFontIndirectW(&ncm.lfMessageFont);
            if (!hFont) hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(hFont));
            EnumChildWindows(hwnd, [](HWND child, LPARAM font) -> BOOL {
                SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
                return TRUE;
            }, reinterpret_cast<LPARAM>(hFont));

            SendMessageW(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);

            // Resize window to exactly fit the pixel-sized controls (dialog
            // template uses DLU which scales with DPI/font while controls are
            // in pixels, leaving extra empty space on the right otherwise).
            int clientW = margin + contentW + margin;
            int clientH = btnY + btnH + margin;
            RECT wrc = { 0, 0, clientW, clientH };
            DWORD style   = static_cast<DWORD>(GetWindowLongW(hwnd, GWL_STYLE));
            DWORD exStyle = static_cast<DWORD>(GetWindowLongW(hwnd, GWL_EXSTYLE));
            AdjustWindowRectEx(&wrc, style, FALSE, exStyle);
            int winW = wrc.right - wrc.left;
            int winH = wrc.bottom - wrc.top;

            // Center on parent with new size
            HWND hParent = GetParent(hwnd);
            int x = 0, y = 0;
            if (hParent) {
                RECT rcParent;
                GetWindowRect(hParent, &rcParent);
                x = rcParent.left + ((rcParent.right - rcParent.left) - winW) / 2;
                y = rcParent.top + ((rcParent.bottom - rcParent.top) - winH) / 2;
            }
            SetWindowPos(hwnd, nullptr, x, y, winW, winH,
                         (hParent ? 0 : SWP_NOMOVE) | SWP_NOZORDER);

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

        case WM_DESTROY: {
            HFONT hFont = reinterpret_cast<HFONT>(GetWindowLongPtrW(hwnd, DWLP_USER));
            if (hFont) DeleteObject(hFont);
            break;
        }
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
    dlg.tmpl.cx = 180;  // Dialog units
    dlg.tmpl.cy = 58;

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
