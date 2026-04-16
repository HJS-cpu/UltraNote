#include "Application.h"
#include "NoteWindow.h"
#include "NoteListWindow.h"
#include "SettingsDialog.h"
#include "Storage.h"
#include "Localization.h"
#include "Utils.h"
#include "Resource.h"
#include <algorithm>
#include <ctime>
#include <shellapi.h>

static const wchar_t* APP_WND_CLASS = L"UltraNoteApp";
static const wchar_t* MUTEX_NAME    = L"UltraNoteInstance";

Application& Application::Get() {
    static Application instance;
    return instance;
}

bool Application::Initialize(HINSTANCE hInst) {
    m_hInst = hInst;

    // Load language from settings.ini
    std::wstring settingsPath = GetExeDirectory() + L"\\settings.ini";
    wchar_t langBuf[32];
    GetPrivateProfileStringW(L"general", L"language", L"en",
                              langBuf, 32, settingsPath.c_str());
    Localization::Get().LoadLanguage(langBuf);

    // Single-instance check
    CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, Ls(L"app.already_running").c_str(), L"UltraNote",
                    MB_OK | MB_ICONINFORMATION);
        return false;
    }

    // Register window classes
    NoteWindow::RegisterWindowClass(hInst);
    NoteListWindow::RegisterWindowClass(hInst);

    if (!CreateAppWindow()) return false;
    if (!SetupTrayIcon()) return false;

    LoadMenuBitmaps();

    // Load saved notes
    m_notes = Storage::LoadNotes(m_nextId, m_folders);

    // Create windows for visible notes
    for (auto& note : m_notes) {
        if (!note->isHidden) {
            CreateNoteWindow(note.get());
        }
    }

    // Apply saved settings (autosave interval, cascade positions, etc.)
    ApplySettings();

    return true;
}

int Application::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Shutdown();
    return static_cast<int>(msg.wParam);
}

void Application::Shutdown() {
    SaveAll();
    UnregisterGlobalHotkeys();
    KillTimer(m_hAppWnd, IDT_AUTOSAVE);
    RemoveTrayIcon();

    // Destroy note windows
    for (auto& [id, wnd] : m_noteWindows) {
        delete wnd;
    }
    m_noteWindows.clear();
    m_noteListWindow.reset();

    // Free cached menu bitmaps
    for (auto& [id, bmp] : m_menuBitmaps) {
        if (bmp) DeleteObject(bmp);
    }
    m_menuBitmaps.clear();

    if (m_hAppWnd) {
        DestroyWindow(m_hAppWnd);
        m_hAppWnd = nullptr;
    }
}

// ============================================================================
// App window (hidden, receives tray messages)
// ============================================================================

bool Application::CreateAppWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = AppWndProc;
    wc.hInstance     = m_hInst;
    wc.lpszClassName = APP_WND_CLASS;
    RegisterClassExW(&wc);

    m_hAppWnd = CreateWindowExW(0, APP_WND_CLASS, L"UltraNote",
                                 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, m_hInst, nullptr);
    return m_hAppWnd != nullptr;
}

LRESULT CALLBACK Application::AppWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Application::Get().HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT Application::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAY_CALLBACK: {
            UINT trayMsg = LOWORD(lParam);
            if (trayMsg == WM_RBUTTONUP) {
                ShowTrayMenu();
            } else if (trayMsg == WM_LBUTTONDBLCLK) {
                auto data = SettingsDialog::LoadFromStorage();
                switch (data.trayDoubleClick) {
                    case 1:  ToggleNoteList(); break;
                    case 2:  ShowAllNotes(); break;
                    default: CreateNewNote(); break;
                }
            }
            return 0;
        }

        case WM_COMMAND: {
            UINT cmd = LOWORD(wParam);
            switch (cmd) {
                case ID_TRAY_NEWNOTE:    CreateNewNote(); break;
                case ID_TRAY_PASTENOTE:  CreateNoteFromClipboard(); break;
                case ID_TRAY_SHOWNOTES:  ShowAllNotes(); break;
                case ID_TRAY_HIDENOTES:  HideAllNotes(); break;
                case ID_TRAY_NOTELIST:   ToggleNoteList(); break;
                case ID_TRAY_SEARCH:     ShowSearchInNoteList(); break;
                case ID_TRAY_SETTINGS:   ShowSettingsDialog(); break;
                case ID_TRAY_ABOUT:      ShowAboutDialog(m_hAppWnd); break;
                case ID_TRAY_EXIT:       PostQuitMessage(0); break;
            }
            return 0;
        }

        case WM_NOTE_CHANGED: {
            m_dirty = true;
            RefreshNoteList();
            return 0;
        }

        case WM_NOTE_REQUEST_DELETE: {
            uint64_t id = static_cast<uint64_t>(wParam);
            RequestDeleteNote(id);
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == IDH_GLOBAL_NEWNOTE) {
                CreateNewNote();
            } else if (wParam == IDH_GLOBAL_NOTELIST) {
                ToggleNoteList();
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == IDT_AUTOSAVE && m_dirty) {
                SaveAll();
            }
            return 0;
        }

        case WM_QUERYENDSESSION:
            SaveAll();
            return TRUE;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// System tray
// ============================================================================

bool Application::SetupTrayIcon() {
    m_nid.cbSize           = sizeof(NOTIFYICONDATA);
    m_nid.hWnd             = m_hAppWnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_CALLBACK;
    m_nid.hIcon            = LoadIconW(m_hInst, MAKEINTRESOURCE(IDI_TRAY));
    wcscpy_s(m_nid.szTip, L"UltraNote");
    return Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
}

void Application::RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
}

void Application::LoadMenuBitmaps() {
    // Pre-load shell stock icons still used in menus
    static const SHSTOCKICONID icons[] = {
        SIID_FOLDER,        // Folder list icon
        SIID_STACK,         // Note List
        SIID_RENAME,        // Edit
        SIID_DOCASSOC,      // Rename (note title)
    };
    for (auto id : icons) {
        if (m_menuBitmaps.find(static_cast<int>(id)) == m_menuBitmaps.end()) {
            HBITMAP bmp = LoadShellMenuBitmap(id);
            if (bmp) m_menuBitmaps[static_cast<int>(id)] = bmp;
        }
    }

}

HBITMAP Application::GetMenuBitmap(SHSTOCKICONID id) {
    auto it = m_menuBitmaps.find(static_cast<int>(id));
    return (it != m_menuBitmaps.end()) ? it->second : nullptr;
}

void Application::AppendMenuItem(HMENU hMenu, UINT id, const wchar_t* text,
                                  SHSTOCKICONID iconId, UINT flags) {
    MENUITEMINFOW mii = {};
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE | MIIM_STATE;
    mii.fType      = MFT_STRING;
    mii.fState     = (flags & MF_GRAYED) ? MFS_GRAYED : MFS_ENABLED;
    if (flags & MF_CHECKED) mii.fState |= MFS_CHECKED;
    mii.wID        = id;
    mii.dwTypeData = const_cast<wchar_t*>(text);
    mii.hbmpItem   = GetMenuBitmap(iconId);
    InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
}

HBITMAP Application::GetResourceBitmap(UINT iconResId) {
    auto it = m_resBitmaps.find(iconResId);
    if (it != m_resBitmaps.end()) return it->second;
    HBITMAP bmp = LoadResourceMenuBitmap(iconResId);
    if (bmp) m_resBitmaps[iconResId] = bmp;
    return bmp;
}

void Application::AppendMenuItemRes(HMENU hMenu, UINT id, const wchar_t* text,
                                     UINT iconResId, UINT flags) {
    MENUITEMINFOW mii = {};
    mii.cbSize     = sizeof(mii);
    mii.fMask      = MIIM_ID | MIIM_STRING | MIIM_BITMAP | MIIM_FTYPE | MIIM_STATE;
    mii.fType      = MFT_STRING;
    mii.fState     = (flags & MF_GRAYED) ? MFS_GRAYED : MFS_ENABLED;
    if (flags & MF_CHECKED) mii.fState |= MFS_CHECKED;
    mii.wID        = id;
    mii.dwTypeData = const_cast<wchar_t*>(text);
    mii.hbmpItem   = GetResourceBitmap(iconResId);
    InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &mii);
}

// Subclass proc for the about-dialog link label: hover tracking + hand cursor + click
static LRESULT CALLBACK AboutLinkSubclassProc(HWND hwnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam,
                                               UINT_PTR /*subId*/, DWORD_PTR /*refData*/) {
    switch (msg) {
        case WM_MOUSEMOVE: {
            BOOL hovering = static_cast<BOOL>(reinterpret_cast<INT_PTR>(
                GetPropW(hwnd, L"hover")));
            if (!hovering) {
                SetPropW(hwnd, L"hover", reinterpret_cast<HANDLE>(1));
                InvalidateRect(hwnd, nullptr, TRUE);
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            break;
        }
        case WM_MOUSELEAVE:
            SetPropW(hwnd, L"hover", reinterpret_cast<HANDLE>(0));
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        case WM_LBUTTONUP:
            ShellExecuteW(hwnd, L"open",
                          L"https://github.com/HJS-cpu/UltraNote",
                          nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"hover");
            RemoveWindowSubclass(hwnd, AboutLinkSubclassProc, 0);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void Application::ShowAboutDialog(HWND hParent) {
    // Build in-memory dialog template
    alignas(4) BYTE buf[2048] = {};
    BYTE* p = buf;

    auto writeStr = [&p](const wchar_t* s) {
        size_t bytes = (wcslen(s) + 1) * sizeof(wchar_t);
        memcpy(p, s, bytes);
        p += bytes;
    };
    auto align4 = [&p]() {
        p = reinterpret_cast<BYTE*>((reinterpret_cast<uintptr_t>(p) + 3) & ~3);
    };

    auto* dlg = reinterpret_cast<DLGTEMPLATE*>(p);
    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg->cdit = 3;  // static text, link static, OK button
    dlg->cx = 170;
    dlg->cy = 80;
    p += sizeof(DLGTEMPLATE);

    // Menu (none), Class (default)
    *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);
    *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);

    // Title - strip & accelerator prefix
    const wchar_t* title = Ls(L"menu.about").c_str();
    std::wstring cleanTitle;
    for (const wchar_t* t = title; *t; ++t)
        if (*t != L'&') cleanTitle += *t;
    writeStr(cleanTitle.c_str());
    align4();

    // Control 1: Static text (version + copyright)
    auto* item1 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    item1->style = WS_CHILD | WS_VISIBLE | SS_CENTER;
    item1->x = 10; item1->y = 8; item1->cx = 150; item1->cy = 30;
    item1->id = 1000;
    p += sizeof(DLGITEMTEMPLATE);
    *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD);
    *reinterpret_cast<WORD*>(p) = 0x0082; p += sizeof(WORD);
    std::wstring staticText = L"UltraNote ";
    staticText += ULTRANOTE_VERSION;
    staticText += L"\n\n\u00A9 2026 by HJS (Hans Joachim Schlingensief)";
    writeStr(staticText.c_str());
    *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);
    align4();

    // Control 2: Static label as clickable link (SS_CENTER | SS_NOTIFY)
    auto* item2 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    item2->style = WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY;
    item2->x = 10; item2->y = 42; item2->cx = 150; item2->cy = 10;
    item2->id = 1001;
    p += sizeof(DLGITEMTEMPLATE);
    *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD);
    *reinterpret_cast<WORD*>(p) = 0x0082; p += sizeof(WORD);
    writeStr(L"github.com/HJS-cpu/UltraNote");
    *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);
    align4();

    // Control 3: OK button
    auto* item3 = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    item3->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item3->x = 60; item3->y = 60; item3->cx = 50; item3->cy = 14;
    item3->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE);
    *reinterpret_cast<WORD*>(p) = 0xFFFF; p += sizeof(WORD);
    *reinterpret_cast<WORD*>(p) = 0x0080; p += sizeof(WORD);
    writeStr(L"OK");
    *reinterpret_cast<WORD*>(p) = 0; p += sizeof(WORD);

    // Dialog proc
    auto dlgProc = [](HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) -> INT_PTR {
        switch (msg) {
            case WM_INITDIALOG: {
                // Set about icon in title bar
                HICON hAboutIcon = static_cast<HICON>(LoadImageW(
                    GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_ABOUT),
                    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                    GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
                if (hAboutIcon)
                    SendMessageW(hDlg, WM_SETICON, ICON_SMALL,
                                 reinterpret_cast<LPARAM>(hAboutIcon));
                // Subclass the link label for hover tracking
                HWND hLink = GetDlgItem(hDlg, 1001);
                SetWindowSubclass(hLink, AboutLinkSubclassProc, 0, 0);
                return TRUE;
            }
            case WM_CTLCOLORSTATIC: {
                HWND hCtrl = reinterpret_cast<HWND>(lParam);
                if (GetDlgCtrlID(hCtrl) == 1001) {
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    BOOL hovering = static_cast<BOOL>(reinterpret_cast<INT_PTR>(
                        GetPropW(hCtrl, L"hover")));
                    SetTextColor(hdc, hovering ? RGB(200, 50, 50) : RGB(0, 80, 180));
                    SetBkMode(hdc, TRANSPARENT);
                    return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
                }
                break;
            }
            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                    EndDialog(hDlg, 0);
                    return TRUE;
                }
                break;
        }
        return FALSE;
    };

    DialogBoxIndirectParamW(m_hInst,
                            reinterpret_cast<DLGTEMPLATE*>(buf),
                            hParent, dlgProc, 0);
}

void Application::ShowTrayMenu() {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    AppendMenuItemRes(hPopup, ID_TRAY_NEWNOTE, Ls(L"menu.new_note").c_str(),
                      IDI_NEW);
    AppendMenuItemRes(hPopup, ID_TRAY_PASTENOTE, Ls(L"menu.paste_note").c_str(),
                      IDI_PASTE);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItemRes(hPopup, ID_TRAY_SHOWNOTES, Ls(L"menu.show_notes").c_str(),
                      IDI_SHOW_ALL);
    AppendMenuItemRes(hPopup, ID_TRAY_HIDENOTES, Ls(L"menu.hide_notes").c_str(),
                      IDI_HIDE_ALL);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItemRes(hPopup, ID_TRAY_NOTELIST, Ls(L"menu.note_list").c_str(),
                      IDI_NOTELIST);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItemRes(hPopup, ID_TRAY_SEARCH, Ls(L"menu.search").c_str(),
                      IDI_SEARCH);
    // Settings menu entry (opens the settings dialog with language tab etc.)
    AppendMenuItemRes(hPopup, ID_TRAY_SETTINGS, Ls(L"menu.settings").c_str(),
                      IDI_SETTINGS);
    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);
    AppendMenuItemRes(hPopup, ID_TRAY_ABOUT, Ls(L"menu.about").c_str(),
                      IDI_ABOUT);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItemRes(hPopup, ID_TRAY_EXIT, Ls(L"menu.exit").c_str(),
                      IDI_EXIT);

    POINT pt;
    GetCursorPos(&pt);

    // Required for tray menus to dismiss properly
    SetForegroundWindow(m_hAppWnd);
    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hAppWnd, nullptr);
    PostMessageW(m_hAppWnd, WM_NULL, 0, 0);

    DestroyMenu(hPopup);
}

// ============================================================================
// Note lifecycle
// ============================================================================

NoteWindow* Application::CreateNewNote() {
    auto settings = SettingsDialog::LoadFromStorage();

    auto note = std::make_unique<NoteData>();
    note->id = m_nextId++;
    note->x = m_cascadeX;
    note->y = m_cascadeY;
    note->createdAt = static_cast<int64_t>(std::time(nullptr));
    note->modifiedAt = note->createdAt;

    // Apply default layout from settings
    note->layout.backgroundColor = settings.bgColor;
    note->layout.textColor       = settings.textColor;
    note->layout.borderColor     = settings.borderColor;
    note->layout.fontFace        = settings.fontFace;
    note->layout.fontSizePts     = settings.fontSize;
    note->layout.fontBold        = settings.fontBold;
    note->layout.fontItalic      = settings.fontItalic;

    // Apply default folder from settings
    note->folder = settings.defaultFolder;

    // Apply initial text with variable expansion
    if (!settings.initialText.empty()) {
        int cursorPos = -1;
        note->text = ExpandInitialText(settings.initialText, cursorPos);
        note->cursorPos = cursorPos;
    }

    // Cascade position for next note
    m_cascadeX += settings.cascadeStep;
    m_cascadeY += settings.cascadeStep;
    if (m_cascadeX > settings.cascadeReset) m_cascadeX = settings.newNoteX;
    if (m_cascadeY > settings.cascadeReset) m_cascadeY = settings.newNoteY;

    NoteData* raw = note.get();
    m_notes.push_back(std::move(note));
    NoteWindow* wnd = CreateNoteWindow(raw);

    m_dirty = true;
    RefreshNoteList();
    return wnd;
}

NoteWindow* Application::CreateNoteFromClipboard() {
    std::wstring clipText;
    if (OpenClipboard(m_hAppWnd)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t* pText = static_cast<const wchar_t*>(GlobalLock(hData));
            if (pText) {
                clipText = pText;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    if (clipText.empty()) return nullptr;

    NoteWindow* wnd = CreateNewNote();
    if (wnd) {
        wnd->GetData()->text = clipText;
        InvalidateRect(wnd->GetHwnd(), nullptr, TRUE);
    }
    return wnd;
}

NoteWindow* Application::CreateNoteWindow(NoteData* data) {
    auto* wnd = new NoteWindow(data, m_hInst);
    m_noteWindows[data->id] = wnd;
    return wnd;
}

void Application::RequestDeleteNote(uint64_t id) {
    // Check how many are selected
    auto selected = GetSelectedIds();
    if (selected.size() > 1 && std::find(selected.begin(), selected.end(), id) != selected.end()) {
        DeleteSelectedNotes();
        return;
    }

    auto settings = SettingsDialog::LoadFromStorage();
    if (settings.confirmDelete) {
        HWND ownerWnd = nullptr;
        auto wit = m_noteWindows.find(id);
        if (wit != m_noteWindows.end())
            ownerWnd = wit->second->GetHwnd();
        int result = MessageBoxW(ownerWnd, Ls(L"confirm.delete_one").c_str(), L"UltraNote",
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (result != IDYES) return;
    }

    DeleteNote(id);
}

void Application::DeleteNote(uint64_t id) {
    // Destroy window
    auto it = m_noteWindows.find(id);
    if (it != m_noteWindows.end()) {
        delete it->second;
        m_noteWindows.erase(it);
    }

    // Remove data
    m_notes.erase(std::remove_if(m_notes.begin(), m_notes.end(),
        [id](const std::unique_ptr<NoteData>& n) { return n->id == id; }), m_notes.end());

    m_dirty = true;
    SaveAll();
    RefreshNoteList();

    // Re-show remaining visible notes (destroying a WS_POPUP|WS_EX_TOOLWINDOW
    // window can cause sibling tool windows to lose visibility)
    for (auto& [nid, wnd] : m_noteWindows) {
        if (!wnd->GetData()->isHidden) {
            SetWindowPos(wnd->GetHwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            if (!wnd->GetData()->layout.alwaysOnTop) {
                SetWindowPos(wnd->GetHwnd(), HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
    }
}

void Application::DeleteSelectedNotes() {
    auto selected = GetSelectedIds();
    if (selected.empty()) return;

    auto settingsData = SettingsDialog::LoadFromStorage();
    if (settingsData.confirmDelete) {
        std::wstring msg;
        if (selected.size() == 1) {
            msg = Ls(L"confirm.delete_one");
        } else {
            msg = FormatString(Ls(L"confirm.delete_multi").c_str(), static_cast<int>(selected.size()));
        }

        HWND ownerWnd = nullptr;
        auto wit = m_noteWindows.find(selected.front());
        if (wit != m_noteWindows.end())
            ownerWnd = wit->second->GetHwnd();
        int result = MessageBoxW(ownerWnd, msg.c_str(), L"UltraNote",
                                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (result != IDYES) return;
    }

    for (uint64_t id : selected) {
        DeleteNote(id);
    }
}

void Application::MarkDirty() {
    m_dirty = true;
}

void Application::SaveAll() {
    if (!m_dirty) return;
    Storage::SaveNotes(m_notes, m_nextId, m_folders);
    m_dirty = false;
}

NoteData* Application::FindNoteData(uint64_t id) {
    for (auto& note : m_notes) {
        if (note->id == id) return note.get();
    }
    return nullptr;
}

// ============================================================================
// Selection
// ============================================================================

void Application::SelectNote(uint64_t id, bool addToSelection) {
    if (!addToSelection) {
        ClearSelection();
    }

    auto it = m_noteWindows.find(id);
    if (it != m_noteWindows.end()) {
        it->second->SetSelected(true);
    }
}

void Application::DeselectNote(uint64_t id) {
    auto it = m_noteWindows.find(id);
    if (it != m_noteWindows.end()) {
        it->second->SetSelected(false);
    }
}

void Application::ClearSelection() {
    for (auto& [id, wnd] : m_noteWindows) {
        wnd->SetSelected(false);
    }
}

std::vector<uint64_t> Application::GetSelectedIds() const {
    std::vector<uint64_t> ids;
    for (auto& [id, wnd] : m_noteWindows) {
        if (wnd->IsSelected()) ids.push_back(id);
    }
    return ids;
}

void Application::MoveSelectedNotes(int dx, int dy, uint64_t excludeId) {
    for (auto& [id, wnd] : m_noteWindows) {
        if (id != excludeId && wnd->IsSelected()) {
            wnd->OffsetPosition(dx, dy);
        }
    }
}

// ============================================================================
// Visibility
// ============================================================================

void Application::ShowAllNotes() {
    m_notesVisible = true;
    for (auto& note : m_notes) {
        note->isHidden = false;
        auto it = m_noteWindows.find(note->id);
        if (it != m_noteWindows.end()) {
            it->second->Show(true);
        } else {
            CreateNoteWindow(note.get());
        }
    }
    m_dirty = true;
    RefreshNoteList();
}

void Application::HideAllNotes() {
    m_notesVisible = false;
    for (auto& note : m_notes) {
        note->isHidden = true;
        auto it = m_noteWindows.find(note->id);
        if (it != m_noteWindows.end()) {
            it->second->Show(false);
        }
    }
    m_dirty = true;
    RefreshNoteList();
}

// ============================================================================
// Bring note to front
// ============================================================================

void Application::BringNoteToFront(uint64_t id, bool enterEdit) {
    bool hiddenChanged = false;
    auto it = m_noteWindows.find(id);
    if (it == m_noteWindows.end()) {
        // Note has no window (hidden) - create one
        NoteData* data = FindNoteData(id);
        if (!data) return;
        data->isHidden = false;
        CreateNoteWindow(data);
        it = m_noteWindows.find(id);
        m_dirty = true;
        hiddenChanged = true;
    } else if (it->second->GetData()->isHidden) {
        it->second->GetData()->isHidden = false;
        m_dirty = true;
        hiddenChanged = true;
    }
    it->second->BringToFront();
    if (enterEdit) it->second->EnterEditMode();
    if (hiddenChanged) RefreshNoteList();
}

void Application::SetSearchHighlight(const std::wstring& term) {
    if (m_searchHighlight == term) return;
    m_searchHighlight = term;
    for (auto& kv : m_noteWindows) {
        HWND hwnd = kv.second->GetHwnd();
        if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ============================================================================
// Preview
// ============================================================================

NoteWindow* Application::ShowNotePreview(uint64_t id) {
    auto it = m_noteWindows.find(id);
    if (it == m_noteWindows.end()) {
        // Note has no window (hidden) - create one
        NoteData* data = FindNoteData(id);
        if (!data) return nullptr;
        data->isHidden = false;
        CreateNoteWindow(data);
        it = m_noteWindows.find(id);
        m_dirty = true;
    } else if (it->second->GetData()->isHidden) {
        it->second->GetData()->isHidden = false;
        it->second->Show(true);
        m_dirty = true;
    }
    it->second->BringToFront();
    return it->second;
}

void Application::HideNotePreview(uint64_t id) {
    NoteData* data = FindNoteData(id);
    if (!data) return;
    data->isHidden = true;
    auto it = m_noteWindows.find(id);
    if (it != m_noteWindows.end()) {
        it->second->Show(false);
    }
    m_dirty = true;
}

bool Application::IsNoteVisible(uint64_t id) const {
    auto it = m_noteWindows.find(id);
    if (it == m_noteWindows.end()) return false;
    return !it->second->GetData()->isHidden;
}

NoteWindow* Application::FindNoteWindow(uint64_t id) const {
    auto it = m_noteWindows.find(id);
    if (it == m_noteWindows.end()) return nullptr;
    return it->second;
}

void Application::MoveNoteWindow(uint64_t id, int x, int y) {
    NoteData* data = FindNoteData(id);
    if (data) {
        data->x = x;
        data->y = y;
    }
    auto it = m_noteWindows.find(id);
    if (it != m_noteWindows.end()) {
        SetWindowPos(it->second->GetHwnd(), nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

// ============================================================================
// Note list
// ============================================================================

void Application::ToggleNoteList() {
    if (!m_noteListWindow) {
        m_noteListWindow = std::make_unique<NoteListWindow>(m_hInst);
        m_noteListWindow->Create();
    }

    if (m_noteListWindow->IsVisible()) {
        m_noteListWindow->Hide();
    } else {
        m_noteListWindow->Show();
    }
}

void Application::RefreshNoteList() {
    if (m_noteListWindow && m_noteListWindow->IsVisible()) {
        m_noteListWindow->Refresh();
    }
}

void Application::ShowSearchInNoteList() {
    if (!m_noteListWindow) {
        m_noteListWindow = std::make_unique<NoteListWindow>(m_hInst);
        m_noteListWindow->Create();
    }

    if (!m_noteListWindow->IsVisible()) {
        m_noteListWindow->Show();
    }

    m_noteListWindow->FocusSearchField();
}

// ============================================================================
// Folder management
// ============================================================================

void Application::AddFolder(const std::wstring& name) {
    if (name.empty()) return;
    for (const auto& f : m_folders) {
        if (f == name) return; // Already exists
    }
    m_folders.push_back(name);
    std::sort(m_folders.begin(), m_folders.end(),
              [](const std::wstring& a, const std::wstring& b) {
                  return _wcsicmp(a.c_str(), b.c_str()) < 0;
              });
    m_dirty = true;
    RefreshNoteList();
}

void Application::RenameFolder(const std::wstring& oldName, const std::wstring& newName) {
    if (oldName.empty() || newName.empty() || oldName == newName) return;

    // Rename in folder list
    for (auto& f : m_folders) {
        if (f == oldName) { f = newName; break; }
    }
    std::sort(m_folders.begin(), m_folders.end(),
              [](const std::wstring& a, const std::wstring& b) {
                  return _wcsicmp(a.c_str(), b.c_str()) < 0;
              });

    // Update all notes in this folder
    for (auto& note : m_notes) {
        if (note->folder == oldName) note->folder = newName;
    }
    m_dirty = true;
    RefreshNoteList();
}

void Application::DeleteFolder(const std::wstring& name) {
    if (name.empty()) return;

    // Remove from folder list
    m_folders.erase(std::remove(m_folders.begin(), m_folders.end(), name), m_folders.end());

    // Move notes to no folder
    for (auto& note : m_notes) {
        if (note->folder == name) note->folder.clear();
    }
    m_dirty = true;
    RefreshNoteList();
}

void Application::SetNoteFolder(uint64_t noteId, const std::wstring& folder) {
    NoteData* note = FindNoteData(noteId);
    if (!note) return;
    note->folder = folder;
    m_dirty = true;
    RefreshNoteList();
}

void Application::RenameNote(uint64_t noteId, const std::wstring& newTitle) {
    NoteData* note = FindNoteData(noteId);
    if (!note) return;
    note->title = newTitle;
    note->modifiedAt = static_cast<int64_t>(std::time(nullptr));
    m_dirty = true;
    RefreshNoteList();
}

// ============================================================================
// Language
// ============================================================================

// ============================================================================
// Settings
// ============================================================================

void Application::ShowSettingsDialog() {
    // Prevent multiple instances
    if (m_settingsOpen) return;
    m_settingsOpen = true;

    // Pause preview in note list to prevent it from interfering with the dialog
    if (m_noteListWindow) {
        m_noteListWindow->SetPreviewPaused(true);
    }

    SettingsDialog::Show(m_hAppWnd);

    // Resume preview after dialog closes
    if (m_noteListWindow) {
        m_noteListWindow->SetPreviewPaused(false);
    }
    m_settingsOpen = false;
}

void Application::ApplySettings() {
    auto data = SettingsDialog::LoadFromStorage();

    // Apply cascade start position
    m_cascadeX = data.newNoteX;
    m_cascadeY = data.newNoteY;

    // Apply autosave timer
    KillTimer(m_hAppWnd, IDT_AUTOSAVE);
    SetTimer(m_hAppWnd, IDT_AUTOSAVE,
             static_cast<UINT>(data.autosaveInterval) * 1000, nullptr);

    // Apply default layout to all existing notes and repaint
    NoteLayout defaultLayout;
    defaultLayout.backgroundColor = data.bgColor;
    defaultLayout.textColor       = data.textColor;
    defaultLayout.borderColor     = data.borderColor;
    defaultLayout.fontFace        = data.fontFace;
    defaultLayout.fontSizePts     = data.fontSize;
    defaultLayout.fontBold        = data.fontBold;
    defaultLayout.fontItalic      = data.fontItalic;

    for (auto& note : m_notes) {
        // Preserve per-note alwaysOnTop
        bool ontop = note->layout.alwaysOnTop;
        note->layout = defaultLayout;
        note->layout.alwaysOnTop = ontop;
    }
    for (auto& [id, wnd] : m_noteWindows) {
        InvalidateRect(wnd->GetHwnd(), nullptr, TRUE);
    }
    m_dirty = true;

    // Apply clickable links setting
    m_clickableLinks = data.clickableLinks;

    // Apply search highlight color
    m_searchHlColor = data.searchHlColor;

    // Apply preview setting to note list
    if (m_noteListWindow) {
        m_noteListWindow->SetPreviewEnabled(data.previewEnabled);
    }

    // Refresh note list to apply date format, zebra striping etc.
    RefreshNoteList();

    // Register global hotkeys
    RegisterGlobalHotkeys();

    // Apply language if changed
    if (data.language != Localization::Get().GetCurrentLanguage()) {
        ChangeLanguage(data.language);
    }
}

// Convert HOTKEYF_* flags to MOD_* flags for RegisterHotKey
static UINT HotkeyModsToRegisterMods(BYTE mods) {
    UINT result = MOD_NOREPEAT;
    if (mods & HOTKEYF_CONTROL) result |= MOD_CONTROL;
    if (mods & HOTKEYF_SHIFT)   result |= MOD_SHIFT;
    if (mods & HOTKEYF_ALT)     result |= MOD_ALT;
    return result;
}

void Application::RegisterGlobalHotkeys() {
    UnregisterGlobalHotkeys();

    auto data = SettingsDialog::LoadFromStorage();

    // Register global new note hotkey
    WORD hkNew = data.shortcuts[SC_GLOBAL_NEWNOTE];
    if (hkNew != 0) {
        RegisterHotKey(m_hAppWnd, IDH_GLOBAL_NEWNOTE,
                       HotkeyModsToRegisterMods(HIBYTE(hkNew)), LOBYTE(hkNew));
    }

    // Register global note list hotkey
    WORD hkList = data.shortcuts[SC_GLOBAL_NOTELIST];
    if (hkList != 0) {
        RegisterHotKey(m_hAppWnd, IDH_GLOBAL_NOTELIST,
                       HotkeyModsToRegisterMods(HIBYTE(hkList)), LOBYTE(hkList));
    }
}

void Application::UnregisterGlobalHotkeys() {
    UnregisterHotKey(m_hAppWnd, IDH_GLOBAL_NEWNOTE);
    UnregisterHotKey(m_hAppWnd, IDH_GLOBAL_NOTELIST);
}

void Application::ChangeLanguage(const std::wstring& langCode) {
    if (langCode == Localization::Get().GetCurrentLanguage()) return;

    // Load new language
    Localization::Get().LoadLanguage(langCode);

    // Save to settings.ini
    std::wstring settingsPath = GetExeDirectory() + L"\\settings.ini";
    WritePrivateProfileStringW(L"general", L"language", langCode.c_str(),
                                settingsPath.c_str());

    // Update note list window title and menu if open
    if (m_noteListWindow) {
        // Destroy and recreate to pick up new strings
        bool wasVisible = m_noteListWindow->IsVisible();
        m_noteListWindow.reset();
        if (wasVisible) {
            m_noteListWindow = std::make_unique<NoteListWindow>(m_hInst);
            m_noteListWindow->Create();
            m_noteListWindow->Show();
        }
    }
}
