#include "Application.h"
#include "NoteWindow.h"
#include "NoteListWindow.h"
#include "Storage.h"
#include "Localization.h"
#include "Utils.h"
#include "Resource.h"
#include <algorithm>
#include <ctime>

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
        if (!note.isHidden) {
            CreateNoteWindow(note);
        }
    }

    // Start auto-save timer
    SetTimer(m_hAppWnd, IDT_AUTOSAVE, AUTOSAVE_INTERVAL_MS, nullptr);

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
                CreateNewNote();
            }
            return 0;
        }

        case WM_COMMAND: {
            UINT cmd = LOWORD(wParam);
            if (cmd >= ID_LANG_BASE && cmd <= ID_LANG_MAX) {
                size_t idx = cmd - ID_LANG_BASE;
                if (idx < m_availableLangs.size()) {
                    ChangeLanguage(m_availableLangs[idx].first);
                }
                return 0;
            }
            switch (cmd) {
                case ID_TRAY_NEWNOTE:   CreateNewNote(); break;
                case ID_TRAY_SHOWNOTES: ShowAllNotes(); break;
                case ID_TRAY_HIDENOTES: HideAllNotes(); break;
                case ID_TRAY_NOTELIST:  ToggleNoteList(); break;
                case ID_TRAY_EXIT:      PostQuitMessage(0); break;
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
    // Pre-load all shell stock icons used in menus
    static const SHSTOCKICONID icons[] = {
        SIID_DOCNOASSOC,    // New Note
        SIID_FOLDER,        // Show Notes
        SIID_FOLDERBACK,    // Hide Notes
        SIID_STACK,         // Note List
        SIID_WORLD,         // Settings
        SIID_DELETE,        // Exit / Delete
        SIID_RENAME,        // Edit
        SIID_LOCK,          // Always on Top (pin-like)
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

void Application::ShowTrayMenu() {
    HMENU hPopup = CreatePopupMenu();
    if (!hPopup) return;

    AppendMenuItem(hPopup, ID_TRAY_NEWNOTE, Ls(L"menu.new_note").c_str(),
                   SIID_DOCNOASSOC);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItem(hPopup, ID_TRAY_SHOWNOTES, Ls(L"menu.show_notes").c_str(),
                   SIID_FOLDER, m_notesVisible ? MF_GRAYED : 0);
    AppendMenuItem(hPopup, ID_TRAY_HIDENOTES, Ls(L"menu.hide_notes").c_str(),
                   SIID_FOLDERBACK, m_notesVisible ? 0 : MF_GRAYED);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItem(hPopup, ID_TRAY_NOTELIST, Ls(L"menu.note_list").c_str(),
                   SIID_STACK);

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    // Settings > Language submenu
    HMENU hLangMenu = CreatePopupMenu();
    auto langs = Localization::Get().GetAvailableLanguages();
    const auto& currentLang = Localization::Get().GetCurrentLanguage();
    m_availableLangs = langs;
    for (size_t i = 0; i < langs.size() && i < (ID_LANG_MAX - ID_LANG_BASE + 1); ++i) {
        UINT flags = MF_STRING;
        if (langs[i].first == currentLang) flags |= MF_CHECKED;
        AppendMenuW(hLangMenu, flags, ID_LANG_BASE + static_cast<UINT>(i),
                    langs[i].second.c_str());
    }

    HMENU hSettingsMenu = CreatePopupMenu();
    AppendMenuW(hSettingsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hLangMenu),
                Ls(L"menu.language").c_str());

    // Insert Settings as popup with icon
    {
        MENUITEMINFOW mii = {};
        mii.cbSize     = sizeof(mii);
        mii.fMask      = MIIM_STRING | MIIM_SUBMENU | MIIM_BITMAP;
        mii.hSubMenu   = hSettingsMenu;
        mii.dwTypeData = const_cast<wchar_t*>(Ls(L"menu.settings").c_str());
        mii.hbmpItem   = GetMenuBitmap(SIID_WORLD);
        InsertMenuItemW(hPopup, GetMenuItemCount(hPopup), TRUE, &mii);
    }

    AppendMenuW(hPopup, MF_SEPARATOR, 0, nullptr);

    AppendMenuItem(hPopup, ID_TRAY_EXIT, Ls(L"menu.exit").c_str(),
                   SIID_DELETE);

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
    NoteData note;
    note.id = m_nextId++;
    note.x = m_cascadeX;
    note.y = m_cascadeY;
    note.createdAt = static_cast<int64_t>(std::time(nullptr));
    note.modifiedAt = note.createdAt;

    // Cascade position for next note
    m_cascadeX += 20;
    m_cascadeY += 20;
    if (m_cascadeX > 500) m_cascadeX = 100;
    if (m_cascadeY > 500) m_cascadeY = 100;

    m_notes.push_back(std::move(note));
    NoteWindow* wnd = CreateNoteWindow(m_notes.back());

    m_dirty = true;
    RefreshNoteList();
    return wnd;
}

NoteWindow* Application::CreateNoteWindow(NoteData& data) {
    auto* wnd = new NoteWindow(&data, m_hInst);
    m_noteWindows[data.id] = wnd;
    return wnd;
}

void Application::RequestDeleteNote(uint64_t id) {
    // Check how many are selected
    auto selected = GetSelectedIds();
    if (selected.size() > 1 && std::find(selected.begin(), selected.end(), id) != selected.end()) {
        DeleteSelectedNotes();
        return;
    }

    int result = MessageBoxW(nullptr, Ls(L"confirm.delete_one").c_str(), L"UltraNote",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result != IDYES) return;

    // Destroy window
    auto it = m_noteWindows.find(id);
    if (it != m_noteWindows.end()) {
        delete it->second;
        m_noteWindows.erase(it);
    }

    // Remove data
    m_notes.erase(std::remove_if(m_notes.begin(), m_notes.end(),
        [id](const NoteData& n) { return n.id == id; }), m_notes.end());

    m_dirty = true;
    SaveAll();
    RefreshNoteList();
}

void Application::DeleteSelectedNotes() {
    auto selected = GetSelectedIds();
    if (selected.empty()) return;

    std::wstring msg;
    if (selected.size() == 1) {
        msg = Ls(L"confirm.delete_one");
    } else {
        msg = FormatString(Ls(L"confirm.delete_multi").c_str(), static_cast<int>(selected.size()));
    }

    int result = MessageBoxW(nullptr, msg.c_str(), L"UltraNote",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (result != IDYES) return;

    for (uint64_t id : selected) {
        auto it = m_noteWindows.find(id);
        if (it != m_noteWindows.end()) {
            delete it->second;
            m_noteWindows.erase(it);
        }
        m_notes.erase(std::remove_if(m_notes.begin(), m_notes.end(),
            [id](const NoteData& n) { return n.id == id; }), m_notes.end());
    }

    m_dirty = true;
    SaveAll();
    RefreshNoteList();
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
        if (note.id == id) return &note;
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
        note.isHidden = false;
        auto it = m_noteWindows.find(note.id);
        if (it != m_noteWindows.end()) {
            it->second->Show(true);
        } else {
            CreateNoteWindow(note);
        }
    }
    m_dirty = true;
}

void Application::HideAllNotes() {
    m_notesVisible = false;
    for (auto& note : m_notes) {
        note.isHidden = true;
        auto it = m_noteWindows.find(note.id);
        if (it != m_noteWindows.end()) {
            it->second->Show(false);
        }
    }
    m_dirty = true;
}

// ============================================================================
// Bring note to front
// ============================================================================

void Application::BringNoteToFront(uint64_t id) {
    auto it = m_noteWindows.find(id);
    if (it == m_noteWindows.end()) {
        // Note has no window (hidden) - create one
        NoteData* data = FindNoteData(id);
        if (!data) return;
        data->isHidden = false;
        CreateNoteWindow(*data);
        it = m_noteWindows.find(id);
        m_dirty = true;
    } else if (it->second->GetData()->isHidden) {
        it->second->GetData()->isHidden = false;
        m_dirty = true;
    }
    it->second->BringToFront();
    it->second->EnterEditMode();
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
        CreateNoteWindow(*data);
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
        if (note.folder == oldName) note.folder = newName;
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
        if (note.folder == name) note.folder.clear();
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
