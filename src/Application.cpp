#include "Application.h"
#include "NoteWindow.h"
#include "NoteListWindow.h"
#include "SettingsDialog.h"
#include "Storage.h"
#include "Localization.h"
#include "Utils.h"
#include "Resource.h"
#include "AlarmScheduler.h"
#include "AlarmPopupWindow.h"
#include "TrayBubbleWindow.h"
#include <algorithm>
#include <ctime>
#include <optional>
#include <unordered_set>
#include <shellapi.h>
#include <mmsystem.h>   // PlaySoundW for the windowless sound-only alarm path

static const wchar_t* APP_WND_CLASS = L"UltraNoteApp";
static const wchar_t* MUTEX_NAME    = L"UltraNoteInstance";

// Defined later in this translation unit; forward-declared so the startup
// off-screen-healing pass in Initialize can reuse the same monitor check.
static bool IsRectOnAnyMonitor(int x, int y, int w, int h);

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

    m_trayBubble = std::make_unique<TrayBubbleWindow>();
    m_trayBubble->Create(hInst);

    LoadMenuBitmaps();

    // Load saved notes. If notes.json was partially unparsable, LoadNotes has
    // already backed up the raw file (notes.json.bak); warn the user so the
    // partial save that follows doesn't look like silent data loss.
    bool notesCorrupt = false;
    m_notes = Storage::LoadNotes(m_nextId, m_folders, notesCorrupt);
    if (notesCorrupt) {
        MessageBoxW(nullptr, Ls(L"error.notes_corrupt").c_str(), L"UltraNote",
                    MB_OK | MB_ICONWARNING);
    }

    // Heal nextId and drop duplicate ids: if the stored nextId is <= an existing
    // note id (corrupted/hand-edited file), CreateNewNote would hand out a
    // colliding id and clobber a live note (orphaned window, double-delete UAF).
    {
        std::unordered_set<uint64_t> seenIds;
        for (auto it = m_notes.begin(); it != m_notes.end(); ) {
            uint64_t id = (*it)->id;
            if (!seenIds.insert(id).second) {
                it = m_notes.erase(it);   // duplicate id: drop the later note
                continue;
            }
            if (id >= m_nextId) m_nextId = id + 1;
            ++it;
        }
    }

    // Apply saved settings (autosave interval, cascade positions, etc.) BEFORE
    // creating note windows so off-screen healing below can use the configured
    // cascade start position. ApplySettings guards its m_noteListWindow uses, so
    // running it while that window does not yet exist is safe.
    ApplySettings();

    // Off-screen safety: if a saved note's rectangle no longer intersects any
    // monitor (display setup changed since last run), move it onto the cascade
    // position used for new notes — otherwise it would be permanently invisible.
    // The note list and import already do this; only the normal startup did not.
    {
        auto settings = SettingsDialog::LoadFromStorage();
        for (auto& note : m_notes) {
            if (!IsRectOnAnyMonitor(note->x, note->y,
                                     note->width  > 0 ? note->width  : 200,
                                     note->height > 0 ? note->height : 150)) {
                note->x = m_cascadeX;
                note->y = m_cascadeY;
                m_cascadeX += settings.cascadeStep;
                m_cascadeY += settings.cascadeStep;
                if (m_cascadeX > settings.cascadeReset) m_cascadeX = settings.newNoteX;
                if (m_cascadeY > settings.cascadeReset) m_cascadeY = settings.newNoteY;
                m_dirty = true;
            }
        }
    }

    // Create windows for visible notes (now at on-screen positions)
    for (auto& note : m_notes) {
        if (!note->isHidden) {
            CreateNoteWindow(note.get());
        }
    }

    // Pre-create the note list window (hidden) so the very first Show() goes
    // through the warm path. Doing the heavy CreateWindowExW + child setup in
    // the same stack as the activation request from the tray click breaks
    // foreground transition on the first open — the title bar stays inactive
    // because Windows skips re-activation when the window already sits at the
    // top of the z-order. Pre-create avoids that by separating creation from
    // the first activation.
    m_noteListWindow = std::make_unique<NoteListWindow>(m_hInst);
    m_noteListWindow->Create();

    return true;
}

void Application::RegisterModelessDialog(HWND hwnd) {
    if (!hwnd) return;
    for (HWND h : m_modelessDialogs) if (h == hwnd) return;  // no duplicates
    m_modelessDialogs.push_back(hwnd);
}

void Application::UnregisterModelessDialog(HWND hwnd) {
    for (auto it = m_modelessDialogs.begin(); it != m_modelessDialogs.end(); ++it) {
        if (*it == hwnd) { m_modelessDialogs.erase(it); return; }
    }
}

int Application::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // Give each live modeless dialog a chance to consume the message for
        // keyboard navigation (Tab/arrows/default-button/Esc). IsDialogMessageW
        // returns TRUE when it handled the message, in which case we must NOT
        // also Translate/Dispatch it. Iterate a copy: dialog procs invoked from
        // inside IsDialogMessageW (e.g. Esc -> WM_COMMAND IDCANCEL -> destroy)
        // can mutate m_modelessDialogs mid-iteration.
        bool handled = false;
        if (!m_modelessDialogs.empty()) {
            std::vector<HWND> snapshot = m_modelessDialogs;
            for (HWND h : snapshot) {
                if (IsWindow(h) && IsDialogMessageW(h, &msg)) { handled = true; break; }
            }
        }
        if (handled) continue;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Shutdown();
    return static_cast<int>(msg.wParam);
}

void Application::Shutdown() {
    CommitEditingNotes();   // flush in-progress edits before the save
    SaveAll();
    UnregisterGlobalHotkeys();
    KillTimer(m_hAppWnd, IDT_AUTOSAVE);
    KillTimer(m_hAppWnd, IDT_ALARM);

    // Close any open alarm popups (they self-delete on WM_NCDESTROY)
    // Make a local copy because DestroyWindow triggers map mutation via OnAlarmPopupClosed
    auto popups = m_alarmPopups;
    for (auto& [id, popup] : popups) {
        if (popup && popup->GetHwnd()) DestroyWindow(popup->GetHwnd());
    }
    m_alarmPopups.clear();

    // Destroying popups fires OnAlarmPopupClosed(Dismiss) -> AdvanceAfterFire +
    // MarkDirty. That happens AFTER the SaveAll above, so persist once more here,
    // otherwise a once-alarm re-pops on the next start (SaveAll early-outs if not
    // dirty, so this is cheap when nothing changed).
    SaveAll();

    if (m_trayBubble) {
        m_trayBubble->Destroy();
        m_trayBubble.reset();
    }

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
    for (auto& [id, bmp] : m_resBitmaps) {
        if (bmp) DeleteObject(bmp);
    }
    m_resBitmaps.clear();

    if (m_hAppWnd) {
        DestroyWindow(m_hAppWnd);
        m_hAppWnd = nullptr;
    }
}

// ============================================================================
// App window (hidden, receives tray messages)
// ============================================================================

// Shell broadcast sent when the taskbar (Explorer) (re)starts. Registered at
// window creation; on receipt we re-add the tray icon. A message-only window
// would NOT receive this broadcast (nor WM_QUERYENDSESSION), which is why
// m_hAppWnd is a real — but never-shown — top-level window (see CreateAppWindow).
static UINT s_taskbarCreatedMsg = 0;

bool Application::CreateAppWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = AppWndProc;
    wc.hInstance     = m_hInst;
    wc.lpszClassName = APP_WND_CLASS;
    RegisterClassExW(&wc);

    // Real top-level window (not HWND_MESSAGE) so it receives the TaskbarCreated
    // broadcast after an Explorer restart. Never shown (no WS_VISIBLE, no
    // ShowWindow); WS_EX_TOOLWINDOW keeps it out of the taskbar / Alt-Tab.
    s_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    m_hAppWnd = CreateWindowExW(WS_EX_TOOLWINDOW, APP_WND_CLASS, L"UltraNote",
                                 WS_POPUP, 0, 0, 0, 0,
                                 nullptr, nullptr, m_hInst, nullptr);
    return m_hAppWnd != nullptr;
}

LRESULT CALLBACK Application::AppWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return Application::Get().HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT Application::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Explorer restarted: re-add the tray icon (re-applies NIM_ADD +
    // NIM_SETVERSION incl. all tray-bubble prerequisites). Dynamic message id,
    // so it can't be a switch case.
    if (msg == s_taskbarCreatedMsg && s_taskbarCreatedMsg != 0) {
        SetupTrayIcon();
        return 0;
    }

    switch (msg) {
        case WM_TRAY_CALLBACK: {
            UINT trayMsg = LOWORD(lParam);
            if (trayMsg == WM_RBUTTONUP) {
                HideTrayBubble();
                ShowTrayMenu();
            } else if (trayMsg == WM_LBUTTONDBLCLK) {
                HideTrayBubble();
                auto data = SettingsDialog::LoadFromStorage();
                switch (data.trayDoubleClick) {
                    case 1:  ToggleNoteList(); break;
                    case 2:  ShowAllNotes(); break;
                    default: CreateNewNote(); break;
                }
            } else if (trayMsg == NIN_POPUPOPEN) {
                ShowTrayBubble();
            } else if (trayMsg == NIN_POPUPCLOSE) {
                HideTrayBubble();
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
            } else if (wParam == IDT_ALARM) {
                CheckDueAlarms();
            }
            return 0;
        }

        case WM_QUERYENDSESSION:
            CommitEditingNotes();   // commit edits in flight before the session ends
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
    // NIF_TIP + populated szTip is required for the shell to dispatch
    // NIN_POPUPOPEN/CLOSE hover events. Without NIF_SHOWTIP under V4 the
    // standard tooltip is still suppressed — our custom bubble replaces it.
    // The szTip also serves as a fallback for shells that don't support V4.
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_CALLBACK;
    m_nid.hIcon            = LoadIconW(m_hInst, MAKEINTRESOURCE(IDI_TRAY));
    wcscpy_s(m_nid.szTip, L"UltraNote");
    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) return false;

    // NOTIFYICON_VERSION_4: enables NIN_POPUPOPEN/CLOSE hover notifications.
    // Changes callback lParam encoding, but our existing LOWORD(lParam)-based
    // mouse-event dispatch continues to work (event code is still in LOWORD).
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
    return true;
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
    // Build in-memory dialog template. Growable buffer sized ONCE (no realloc,
    // so the dlg/item1..3 pointers taken into it stay valid); writeStr/align4
    // clamp against the end so a long translated title can never overflow.
    std::vector<BYTE> buf(4096, 0);
    BYTE* const base = buf.data();
    BYTE* const endp = base + buf.size();
    BYTE* p = base;

    auto writeStr = [&p, endp](const wchar_t* s) {
        size_t bytes = (wcslen(s) + 1) * sizeof(wchar_t);
        if (p + bytes > endp) bytes = (endp > p) ? static_cast<size_t>(endp - p) : 0;
        if (bytes >= sizeof(wchar_t)) {
            memcpy(p, s, bytes - sizeof(wchar_t));
            *reinterpret_cast<wchar_t*>(p + bytes - sizeof(wchar_t)) = L'\0';
        }
        p += bytes;
    };
    auto align4 = [&p, endp]() {
        p = reinterpret_cast<BYTE*>((reinterpret_cast<uintptr_t>(p) + 3) & ~3);
        if (p > endp) p = endp;
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

    // Title - strip & accelerator prefix. Hold the localized string in a local
    // (not a const wchar_t* to a temporary's c_str()) so it stays alive here.
    std::wstring titleStr = Ls(L"menu.about");
    std::wstring cleanTitle;
    for (wchar_t c : titleStr)
        if (c != L'&') cleanTitle += c;
    // Bound the title: writeStr/align4 clamp, but the raw DLGITEMTEMPLATE/WORD
    // writes that follow do NOT — a pathological 2000+ char .lng title could push
    // p to the buffer end and make those struct writes land out of bounds.
    if (cleanTitle.size() > 256) cleanTitle.resize(256);
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
            case WM_DESTROY: {
                // Free the non-shared small icon set in WM_INITDIALOG
                HICON h = reinterpret_cast<HICON>(
                    SendMessageW(hDlg, WM_GETICON, ICON_SMALL, 0));
                if (h) DestroyIcon(h);
                break;
            }
        }
        return FALSE;
    };

    DialogBoxIndirectParamW(m_hInst,
                            reinterpret_cast<DLGTEMPLATE*>(buf.data()),
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
    note->layout.alwaysOnTop     = settings.newNoteAlwaysOnTop;

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
        NoteData* data = wnd->GetData();
        data->text = clipText;
        // CreateNewNote applied the initial-text template (incl. a possible
        // %%p cursor marker); that position is meaningless for pasted text, so
        // reset it (EnterEditMode then drops the caret at the end). Stamp the
        // modify time and refresh the list so it shows the pasted text, not the
        // initial-text placeholder it was populated with a moment ago.
        data->cursorPos  = -1;
        data->modifiedAt = static_cast<int64_t>(std::time(nullptr));
        InvalidateRect(wnd->GetHwnd(), nullptr, TRUE);
        RefreshNoteList();
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
                                  MB_YESNO | MB_ICONQUESTION);
        if (result != IDYES) return;
    }

    DeleteNote(id);
}

void Application::RemoveNote(uint64_t id) {
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
}

void Application::FinalizeDeletions() {
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

void Application::DeleteNote(uint64_t id) {
    RemoveNote(id);
    FinalizeDeletions();
}

void Application::DeleteNotesByIds(const std::vector<uint64_t>& ids) {
    if (ids.empty()) return;
    for (uint64_t id : ids) {
        RemoveNote(id);
    }
    FinalizeDeletions();   // single SaveAll + RefreshNoteList + re-show pass
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
            msg = FormatCount(Ls(L"confirm.delete_multi"), static_cast<int>(selected.size()));
        }

        HWND ownerWnd = nullptr;
        auto wit = m_noteWindows.find(selected.front());
        if (wit != m_noteWindows.end())
            ownerWnd = wit->second->GetHwnd();
        int result = MessageBoxW(ownerWnd, msg.c_str(), L"UltraNote",
                                  MB_YESNO | MB_ICONQUESTION);
        if (result != IDYES) return;
    }

    for (uint64_t id : selected) {
        RemoveNote(id);
    }
    FinalizeDeletions();
}

void Application::MarkDirty() {
    m_dirty = true;
}

void Application::SaveAll() {
    if (!m_dirty) return;
    Storage::SaveNotes(m_notes, m_nextId, m_folders);
    m_dirty = false;
}

void Application::CommitEditingNotes() {
    // During OS shutdown / logoff / tray-exit the freshly typed text still lives
    // in the EDIT control: the KILLFOCUS path that normally moves it into
    // m_data->text never runs. CommitEditText() performs that move; set m_dirty
    // directly because NotifyChanged()'s posted WM_NOTE_CHANGED won't be pumped.
    for (auto& [id, wnd] : m_noteWindows) {
        if (wnd && wnd->IsEditing()) {
            wnd->CommitEditText();
            m_dirty = true;
        }
    }
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
// Import / Export
// ============================================================================

static bool IsRectOnAnyMonitor(int x, int y, int w, int h) {
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    RECT r = { x, y, x + w, y + h };
    return MonitorFromRect(&r, MONITOR_DEFAULTTONULL) != nullptr;
}

// Build a "(N)" buffer for printf-style success messages.
// Safe single-integer placeholder substitution. NEVER passes the (localized,
// translator-editable) .lng string to the CRT format engine — a wrong specifier
// there would crash or overflow a fixed buffer.
// Convert "Filter Label|*.ext|...||" with '|' as separators into a buffer with
// '\0' separators expected by GetSaveFileName/GetOpenFileName.
static std::wstring BuildOfnFilter(const std::wstring& src) {
    std::wstring out;
    out.reserve(src.size() + 2);
    for (wchar_t ch : src) {
        out += (ch == L'|') ? L'\0' : ch;
    }
    if (out.empty() || out.back() != L'\0') out += L'\0';
    out += L'\0';
    return out;
}

void Application::ExportNotesByIds(HWND owner, const std::vector<uint64_t>& ids) {
    // Resolve IDs to NoteData* in their natural order from m_notes
    std::unordered_set<uint64_t> idSet(ids.begin(), ids.end());
    std::vector<const NoteData*> selected;
    selected.reserve(ids.size());
    for (const auto& note : m_notes) {
        if (idSet.count(note->id)) {
            selected.push_back(note.get());
        }
    }
    if (selected.empty()) return;

    // Default filename: "<export.default_name>-YYYY-MM-DD.unote"
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t dateBuf[16];
    swprintf_s(dateBuf, L"-%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    std::wstring defaultName = Ls(L"export.default_name") + dateBuf + L".unote";

    // GetSaveFileName: editable buffer must be MAX_PATH-sized
    wchar_t fileBuf[MAX_PATH];
    wcsncpy_s(fileBuf, defaultName.c_str(), _TRUNCATE);

    std::wstring filter = BuildOfnFilter(Ls(L"export.filter"));
    std::wstring title  = Ls(L"export.title");

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile   = fileBuf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = title.c_str();
    ofn.lpstrDefExt = L"unote";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&ofn)) return;

    if (!Storage::ExportNotes(fileBuf, selected)) {
        MessageBoxW(owner, Ls(L"export.error_save").c_str(), L"UltraNote",
                    MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring msg = FormatCount(Ls(L"export.success"), static_cast<int>(selected.size()));
    MessageBoxW(owner, msg.c_str(), L"UltraNote", MB_OK | MB_ICONINFORMATION);
}

void Application::ImportNotesFromFile(HWND owner) {
    wchar_t fileBuf[MAX_PATH] = {};
    std::wstring filter = BuildOfnFilter(Ls(L"import.filter"));
    std::wstring title  = Ls(L"import.title");

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile   = fileBuf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = title.c_str();
    ofn.lpstrDefExt = L"unote";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) return;

    std::vector<std::unique_ptr<NoteData>> imported;
    if (!Storage::ImportNotes(fileBuf, imported)) {
        MessageBoxW(owner, Ls(L"import.error_parse").c_str(), L"UltraNote",
                    MB_OK | MB_ICONERROR);
        return;
    }
    if (imported.empty()) {
        MessageBoxW(owner, Ls(L"import.error_empty").c_str(), L"UltraNote",
                    MB_OK | MB_ICONWARNING);
        return;
    }

    auto settings = SettingsDialog::LoadFromStorage();
    int64_t nowTs = static_cast<int64_t>(std::time(nullptr));

    for (auto& note : imported) {
        // Always assign a fresh ID — the source-file IDs may collide with
        // existing notes, and we never want to risk overwriting them.
        note->id = m_nextId++;

        // Sane timestamps if the file omitted them
        if (note->createdAt == 0)  note->createdAt  = nowTs;
        if (note->modifiedAt == 0) note->modifiedAt = nowTs;

        // Off-screen safety: if the saved coordinates do not intersect any
        // monitor (different display setup on import target), fall back to
        // the cascade position used for new notes.
        if (!IsRectOnAnyMonitor(note->x, note->y,
                                 note->width  > 0 ? note->width  : 200,
                                 note->height > 0 ? note->height : 150)) {
            note->x = m_cascadeX;
            note->y = m_cascadeY;
            m_cascadeX += settings.cascadeStep;
            m_cascadeY += settings.cascadeStep;
            if (m_cascadeX > settings.cascadeReset) m_cascadeX = settings.newNoteX;
            if (m_cascadeY > settings.cascadeReset) m_cascadeY = settings.newNoteY;
        }

        // Folder: create on demand if it does not exist locally
        if (!note->folder.empty()) {
            bool found = false;
            for (const auto& f : m_folders) {
                if (f == note->folder) { found = true; break; }
            }
            if (!found) {
                m_folders.push_back(note->folder);
                std::sort(m_folders.begin(), m_folders.end(),
                          [](const std::wstring& a, const std::wstring& b) {
                              return _wcsicmp(a.c_str(), b.c_str()) < 0;
                          });
            }
        }

        // Move into live store and create window if visible
        NoteData* raw = note.get();
        m_notes.push_back(std::move(note));
        if (!raw->isHidden) {
            CreateNoteWindow(raw);
        }
    }

    m_dirty = true;
    RefreshNoteList();

    std::wstring msg = FormatCount(Ls(L"import.success"), static_cast<int>(imported.size()));
    MessageBoxW(owner, msg.c_str(), L"UltraNote", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Print
// ============================================================================

namespace {

int PtToDevY(HDC hdc, int pts) {
    return MulDiv(pts, GetDeviceCaps(hdc, LOGPIXELSY), 72);
}

HFONT MakePrintFont(HDC hdc, const std::wstring& face, int sizePts,
                    bool bold, bool italic) {
    LOGFONTW lf = {};
    lf.lfHeight         = -PtToDevY(hdc, sizePts);
    lf.lfWeight         = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic         = italic ? TRUE : FALSE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    if (!face.empty()) {
        wcsncpy_s(lf.lfFaceName, face.c_str(), _TRUNCATE);
    }
    return CreateFontIndirectW(&lf);
}

int LineHeight(HDC hdc, HFONT hFont) {
    HFONT old = static_cast<HFONT>(SelectObject(hdc, hFont));
    TEXTMETRICW tm = {};
    GetTextMetricsW(hdc, &tm);
    SelectObject(hdc, old);
    return tm.tmHeight + tm.tmExternalLeading;
}

// Word-wrap `text` so each output entry fits within `maxWidth` device pixels
// when rendered with `hFont` on `hdc`. Hard line breaks come from '\n' in the
// source text. Long lines are broken at the last whitespace inside the longest
// prefix that still fits; if no whitespace is available the cut is hard.
std::vector<std::wstring> WrapTextLines(HDC hdc, HFONT hFont,
                                         const std::wstring& text, int maxWidth) {
    std::vector<std::wstring> out;
    if (maxWidth <= 0) return out;
    HFONT old = static_cast<HFONT>(SelectObject(hdc, hFont));

    auto width = [&](const wchar_t* s, int n) -> int {
        if (n <= 0) return 0;
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, s, n, &sz);
        return sz.cx;
    };

    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find(L'\n', pos);
        std::wstring line = (nl == std::wstring::npos)
            ? text.substr(pos)
            : text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        if (line.empty()) {
            out.push_back(L"");
        } else {
            size_t lp = 0;
            while (lp < line.size()) {
                std::wstring rest = line.substr(lp);
                if (width(rest.c_str(), static_cast<int>(rest.size())) <= maxWidth) {
                    out.push_back(rest);
                    break;
                }
                // Binary search for max prefix length that fits
                int lo = 1, hi = static_cast<int>(rest.size()), fit = 1;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    if (width(rest.c_str(), mid) <= maxWidth) {
                        fit = mid;
                        lo  = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                // Prefer breaking at last whitespace within the fitting prefix
                int br = -1;
                for (int j = fit; j > 0; --j) {
                    wchar_t c = rest[j - 1];
                    if (c == L' ' || c == L'\t') { br = j; break; }
                }
                int take = (br > 0) ? br : fit;
                std::wstring chunk = rest.substr(0, take);
                while (!chunk.empty() &&
                       (chunk.back() == L' ' || chunk.back() == L'\t')) {
                    chunk.pop_back();
                }
                out.push_back(chunk);
                lp += take;
                while (lp < line.size() && line[lp] == L' ') ++lp;
            }
        }

        if (nl == std::wstring::npos) break;
        pos = nl + 1;
    }

    SelectObject(hdc, old);
    return out;
}

} // namespace

void Application::PrintNoteByIds(HWND owner, const std::vector<uint64_t>& ids) {
    if (ids.empty()) return;

    // Resolve IDs to NoteData* in the natural order of m_notes
    std::unordered_set<uint64_t> idSet(ids.begin(), ids.end());
    std::vector<const NoteData*> notes;
    notes.reserve(ids.size());
    for (const auto& n : m_notes) {
        if (idSet.count(n->id)) {
            notes.push_back(n.get());
        }
    }
    if (notes.empty()) return;

    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner   = owner;
    pd.Flags       = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
    pd.nCopies     = 1;
    if (!PrintDlgW(&pd)) {
        if (pd.hDevMode)  GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }

    HDC hdc = pd.hDC;
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    int marginX = MulDiv(50, dpiX, 254);   // 5 mm
    int marginY = MulDiv(75, dpiY, 254);   // 7.5 mm
    RECT printable = {
        marginX, marginY,
        GetDeviceCaps(hdc, HORZRES) - marginX,
        GetDeviceCaps(hdc, VERTRES) - marginY
    };

    std::wstring docName = Ls(L"print.doc_name");
    DOCINFOW di = {};
    di.cbSize     = sizeof(di);
    di.lpszDocName = docName.c_str();

    if (StartDocW(hdc, &di) <= 0) {
        DeleteDC(hdc);
        if (pd.hDevMode)  GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        MessageBoxW(owner, Ls(L"print.error_failed").c_str(), L"UltraNote",
                    MB_OK | MB_ICONERROR);
        return;
    }

    // Date format from settings (matches NoteListWindow::PopulateList)
    auto intSettings = Storage::LoadSettings();
    auto itDateFmt = intSettings.find(L"notelist.dateFormat");
    const wchar_t* dateFmt = L"%Y-%m-%d %H:%M";
    if (itDateFmt != intSettings.end() && itDateFmt->second == 1) {
        dateFmt = L"%d.%m.%Y %H:%M";
    }

    int  pageNumber = 0;
    bool ok = true;
    std::wstring pageFmt = Ls(L"print.page_label");

    for (const NoteData* note : notes) {
        int bodyPt  = note->layout.fontSizePts > 0 ? note->layout.fontSizePts : 10;
        int titlePt = bodyPt + 4;
        int metaPt  = (bodyPt - 2 > 8) ? (bodyPt - 2) : 8;
        const std::wstring& face = note->layout.fontFace;

        HFONT hTitleFont = MakePrintFont(hdc, face, titlePt, true,  false);
        HFONT hBodyFont  = MakePrintFont(hdc, face, bodyPt,
                                          note->layout.fontBold,
                                          note->layout.fontItalic);
        HFONT hMetaFont  = MakePrintFont(hdc, face, metaPt,  false, true);
        if (!hTitleFont || !hBodyFont || !hMetaFont) {
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hBodyFont)  DeleteObject(hBodyFont);
            if (hMetaFont)  DeleteObject(hMetaFont);
            ok = false;
            break;
        }

        int titleH = LineHeight(hdc, hTitleFont);
        int bodyH  = LineHeight(hdc, hBodyFont);
        int metaH  = LineHeight(hdc, hMetaFont);

        std::wstring title = note->title;
        if (title.empty()) {
            title = note->text;
            auto nl = title.find(L'\n');
            if (nl != std::wstring::npos) title = title.substr(0, nl);
            if (title.empty()) title = Ls(L"note.untitled");
        }

        std::wstring meta;
        if (!note->folder.empty()) meta = note->folder + L"  •  ";
        int64_t ts = note->modifiedAt > 0 ? note->modifiedAt : note->createdAt;
        if (ts > 0) {
            time_t t = static_cast<time_t>(ts);
            struct tm tm = {};
            wchar_t dbuf[64] = {};
            // Guard localtime_s: a ms- instead of s-timestamp (> year 3000) makes
            // it fail and would trip the debug-build wcsftime assertion otherwise.
            if (localtime_s(&tm, &t) == 0 && wcsftime(dbuf, 64, dateFmt, &tm) > 0)
                meta += dbuf;
        }

        int contentWidth = printable.right - printable.left;
        std::vector<std::wstring> titleLines =
            WrapTextLines(hdc, hTitleFont, title, contentWidth);
        std::vector<std::wstring> metaLines  = meta.empty()
            ? std::vector<std::wstring>{}
            : WrapTextLines(hdc, hMetaFont, meta, contentWidth);
        std::vector<std::wstring> bodyLines  =
            WrapTextLines(hdc, hBodyFont, note->text, contentWidth);

        size_t bodyIdx = 0;
        bool firstPage = true;

        do {
            if (StartPage(hdc) <= 0) { ok = false; break; }
            ++pageNumber;

            SetBkMode(hdc, TRANSPARENT);
            int y = printable.top;

            if (firstPage) {
                SelectObject(hdc, hTitleFont);
                SetTextColor(hdc, RGB(0, 0, 0));
                for (const auto& tl : titleLines) {
                    if (y + titleH > printable.bottom) break;
                    TextOutW(hdc, printable.left, y, tl.c_str(),
                             static_cast<int>(tl.size()));
                    y += titleH;
                }
                if (!metaLines.empty()) {
                    y += titleH / 4;
                    SelectObject(hdc, hMetaFont);
                    SetTextColor(hdc, RGB(80, 80, 80));
                    for (const auto& ml : metaLines) {
                        if (y + metaH > printable.bottom) break;
                        TextOutW(hdc, printable.left, y, ml.c_str(),
                                 static_cast<int>(ml.size()));
                        y += metaH;
                    }
                }
                y += metaH / 2;
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
                HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
                MoveToEx(hdc, printable.left, y, nullptr);
                LineTo(hdc, printable.right, y);
                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
                y += titleH / 2;
            }

            // Reserve footer height (one meta line + small gap)
            int footerReserve = metaH + metaH / 2;
            int bodyBottom    = printable.bottom - footerReserve;

            SelectObject(hdc, hBodyFont);
            SetTextColor(hdc, RGB(0, 0, 0));
            while (bodyIdx < bodyLines.size() && y + bodyH <= bodyBottom) {
                const std::wstring& bl = bodyLines[bodyIdx];
                if (!bl.empty()) {
                    TextOutW(hdc, printable.left, y, bl.c_str(),
                             static_cast<int>(bl.size()));
                }
                y += bodyH;
                ++bodyIdx;
            }

            // Footer: page number, right-aligned
            std::wstring pageLabel = FormatCount(pageFmt, pageNumber);
            int labelLen = static_cast<int>(pageLabel.size());
            SelectObject(hdc, hMetaFont);
            SetTextColor(hdc, RGB(80, 80, 80));
            SIZE pSz = {};
            GetTextExtentPoint32W(hdc, pageLabel.c_str(), labelLen, &pSz);
            TextOutW(hdc, printable.right - pSz.cx,
                     printable.bottom - metaH, pageLabel.c_str(), labelLen);

            if (EndPage(hdc) <= 0) { ok = false; break; }
            firstPage = false;
        } while (bodyIdx < bodyLines.size());

        DeleteObject(hTitleFont);
        DeleteObject(hBodyFont);
        DeleteObject(hMetaFont);

        if (!ok) break;
    }

    if (ok) {
        EndDoc(hdc);
    } else {
        AbortDoc(hdc);
    }
    DeleteDC(hdc);
    if (pd.hDevMode)  GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);

    if (!ok) {
        MessageBoxW(owner, Ls(L"print.error_failed").c_str(), L"UltraNote",
                    MB_OK | MB_ICONERROR);
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

    // Use the note list window as owner when it's visible. With the message-only
    // app window as owner, Windows treats the dialog as ownerless from the user's
    // perspective — the note list can cover it as soon as the dialog loses focus.
    // An owned dialog is kept above its owner in z-order, fixing that.
    HWND hOwner = (m_noteListWindow && m_noteListWindow->IsVisible())
                  ? m_noteListWindow->GetHwnd()
                  : m_hAppWnd;
    SettingsDialog::Show(hOwner);

    // Resume preview after dialog closes
    if (m_noteListWindow) {
        m_noteListWindow->SetPreviewPaused(false);
    }
    m_settingsOpen = false;

    // Now that the modal dialog has closed, run any note-list rebuild that a
    // language change deferred (deferred so it couldn't destroy the dialog owner).
    if (m_pendingNoteListRebuild) {
        m_pendingNoteListRebuild = false;
        RebuildNoteListForLanguage();
    }
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

    // Alarm-check timer (fixed interval, not configurable)
    KillTimer(m_hAppWnd, IDT_ALARM);
    SetTimer(m_hAppWnd, IDT_ALARM, ALARM_CHECK_INTERVAL_MS, nullptr);

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

    // Register global hotkeys (reuse the data already loaded above)
    RegisterGlobalHotkeys(data);

    // Apply language if changed. Swap the string table + settings.ini right away
    // (harmless — only replaces the in-memory map), but defer the note-list
    // rebuild while the settings dialog is open: the rebuild reset()s
    // m_noteListWindow, which owns the modal dialog and would destroy it mid
    // WM_COMMAND (see ShowSettingsDialog owner choice). Outside the dialog there
    // is no owner conflict, so rebuild immediately.
    if (data.language != Localization::Get().GetCurrentLanguage()) {
        Localization::Get().LoadLanguage(data.language);
        std::wstring iniPath = GetExeDirectory() + L"\\settings.ini";
        WritePrivateProfileStringW(L"general", L"language",
                                    data.language.c_str(), iniPath.c_str());
        if (m_settingsOpen) m_pendingNoteListRebuild = true;
        else                RebuildNoteListForLanguage();
    }

    // Heal autostart entry: if the setting is on, ensure the Run-key value
    // exists and points at the current EXE (covers EXE-move/rename cases).
    SettingsDialog::SyncAutostart(data.autostartEnabled);
}

// Convert HOTKEYF_* flags to MOD_* flags for RegisterHotKey
static UINT HotkeyModsToRegisterMods(BYTE mods) {
    UINT result = MOD_NOREPEAT;
    if (mods & HOTKEYF_CONTROL) result |= MOD_CONTROL;
    if (mods & HOTKEYF_SHIFT)   result |= MOD_SHIFT;
    if (mods & HOTKEYF_ALT)     result |= MOD_ALT;
    return result;
}

void Application::RegisterGlobalHotkeys(const SettingsData& data) {
    UnregisterGlobalHotkeys();

    bool anyFailed = false;

    // Register global new note hotkey
    WORD hkNew = data.shortcuts[SC_GLOBAL_NEWNOTE];
    if (hkNew != 0) {
        if (!RegisterHotKey(m_hAppWnd, IDH_GLOBAL_NEWNOTE,
                            HotkeyModsToRegisterMods(HIBYTE(hkNew)), LOBYTE(hkNew)))
            anyFailed = true;
    }

    // Register global note list hotkey
    WORD hkList = data.shortcuts[SC_GLOBAL_NOTELIST];
    if (hkList != 0) {
        if (!RegisterHotKey(m_hAppWnd, IDH_GLOBAL_NOTELIST,
                            HotkeyModsToRegisterMods(HIBYTE(hkList)), LOBYTE(hkList)))
            anyFailed = true;
    }

    // A configured combo already owned by another process fails silently;
    // surface it once via a non-blocking tray balloon (a MessageBox would
    // block at startup). Restore uFlags so later NIM_MODIFY calls don't replay.
    if (anyFailed && m_nid.hWnd) {
        UINT savedFlags = m_nid.uFlags;
        m_nid.uFlags = NIF_INFO;
        m_nid.dwInfoFlags = NIIF_WARNING;
        wcscpy_s(m_nid.szInfoTitle, L"UltraNote");
        wcsncpy_s(m_nid.szInfo, Ls(L"hotkey.register_failed").c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
        m_nid.uFlags = savedFlags;
    }
}

void Application::UnregisterGlobalHotkeys() {
    UnregisterHotKey(m_hAppWnd, IDH_GLOBAL_NEWNOTE);
    UnregisterHotKey(m_hAppWnd, IDH_GLOBAL_NOTELIST);
}

void Application::RebuildNoteListForLanguage() {
    // Recreate the note-list window so it picks up the newly loaded strings. The
    // string table + settings.ini are already updated by the caller; this is kept
    // as a separate step from the string swap so it can be deferred until an open
    // settings dialog (owned by this window) has closed — see ApplySettings.
    if (!m_noteListWindow) return;
    bool wasVisible = m_noteListWindow->IsVisible();
    m_noteListWindow.reset();
    // Always recreate (hidden) so the documented pre-create warm path is
    // preserved — otherwise the next Show() would do the heavy create in the
    // activation stack and the title bar would stay inactive. Only the Show()
    // itself is gated on the previous visibility.
    m_noteListWindow = std::make_unique<NoteListWindow>(m_hInst);
    m_noteListWindow->Create();
    if (wasVisible) {
        m_noteListWindow->Show();
    }
}

// ============================================================================
// Alarms
// ============================================================================

static std::wstring FirstLinesOfText(const std::wstring& text, int maxLines, size_t maxChars) {
    std::wstring out;
    int lines = 0;
    for (size_t i = 0; i < text.size() && out.size() < maxChars; ++i) {
        wchar_t c = text[i];
        if (c == L'\r') continue;
        if (c == L'\n') {
            if (++lines >= maxLines) break;
            out += L' ';
            continue;
        }
        out += c;
    }
    if (out.size() >= maxChars) out += L"...";
    return out;
}

void Application::CheckDueAlarms() {
    SYSTEMTIME now;
    GetLocalTime(&now);

    for (auto& note : m_notes) {
        if (!note->alarm.has_value()) continue;
        // Skip if a popup is already showing for this note
        if (m_alarmPopups.count(note->id)) continue;

        auto nextFire = AlarmScheduler::ComputeNextFireTime(*note->alarm, now);
        if (!nextFire.has_value()) continue;

        if (AlarmScheduler::CompareSysTime(*nextFire, now) <= 0) {
            // Throttle: don't re-fire the same occurrence-minute (the 30s timer
            // can tick twice in one minute, and a quick dismiss would re-pop).
            int64_t fireMin = AlarmScheduler::SysTimeToNaiveSec(*nextFire) / 60;
            auto it = m_lastFiredMinute.find(note->id);
            if (it != m_lastFiredMinute.end() && it->second == fireMin) continue;
            m_lastFiredMinute[note->id] = fireMin;
            TriggerAlarm(*note);
        }
    }
}

void Application::TriggerAlarm(NoteData& note) {
    if (!note.alarm.has_value()) return;
    const auto& a = *note.alarm;

    // No popup requested: handle the sound-only (and no-popup-no-sound) case here
    // without ever creating a window. Fire bookkeeping mirrors the Dismiss path
    // (OnAlarmPopupClosed) so recurring alarms advance/re-schedule identically.
    if (!a.popup) {
        if (a.sound) {
            // One-shot (no SND_LOOP): without a popup there is no Dismiss to stop
            // a looping sound. File check mirrors AlarmPopupWindow::StartSound.
            bool played = false;
            if (!a.soundFile.empty()) {
                DWORD attrs = GetFileAttributesW(a.soundFile.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    PlaySoundW(a.soundFile.c_str(), nullptr,
                               SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
                    played = true;
                }
            }
            if (!played) MessageBeep(MB_ICONEXCLAMATION);
        }
        AlarmScheduler::AdvanceAfterFire(note.alarm.value());
        MarkDirty();
        RefreshNoteList();
        return;
    }

    std::wstring title = note.title;
    if (title.empty()) {
        title = FirstLinesOfText(note.text, 1, 80);
        if (title.empty()) title = Ls(L"note.untitled");
    }
    std::wstring preview = FirstLinesOfText(note.text, 3, 200);

    // Lowest free stack slot (NOT map size): a dismissed mid-stack popup frees
    // its slot, and size() would collide with a still-open higher popup.
    int stackIndex = 0;
    {
        std::vector<bool> used;
        for (auto& [id, p] : m_alarmPopups) {
            if (!p) continue;
            int idx = p->GetStackIndex();
            if (idx < 0) continue;
            if (idx >= static_cast<int>(used.size())) used.resize(idx + 1, false);
            used[idx] = true;
        }
        while (stackIndex < static_cast<int>(used.size()) && used[stackIndex]) ++stackIndex;
    }
    auto popup = new AlarmPopupWindow(m_hInst, note.id, title, preview,
                                      a.sound, a.soundFile,
                                      a.snoozeMinutes, stackIndex);
    if (!popup->Create()) {
        delete popup;
        return;
    }
    m_alarmPopups[note.id] = popup;
}

void Application::OnAlarmPopupClosed(uint64_t noteId, AlarmAction action) {
    m_alarmPopups.erase(noteId);

    NoteData* note = FindNoteData(noteId);
    if (!note || !note->alarm.has_value()) return;

    SYSTEMTIME now;
    GetLocalTime(&now);

    switch (action) {
        case AlarmAction::Dismiss:
            AlarmScheduler::AdvanceAfterFire(note->alarm.value());
            break;
        case AlarmAction::Snooze:
            AlarmScheduler::AdvanceAfterSnooze(note->alarm.value(), now);
            break;
        case AlarmAction::OpenNote:
            AlarmScheduler::AdvanceAfterFire(note->alarm.value());
            BringNoteToFront(noteId);
            break;
    }

    MarkDirty();
    RefreshNoteList();
}

// ============================================================================
// Tray hover bubble (custom-drawn tooltip replacement)
// ============================================================================

void Application::HideTrayBubble() {
    if (m_trayBubble) m_trayBubble->Hide();
}

void Application::ShowTrayBubble() {
    if (!m_trayBubble) return;

    // Anchor rect: use Shell_NotifyIconGetRect (Win7+). Fall back to cursor
    // position if the shell can't locate the icon (e.g. in the overflow area).
    RECT iconRc = {};
    NOTIFYICONIDENTIFIER nii = { sizeof(nii) };
    nii.hWnd = m_hAppWnd;
    nii.uID  = 1;
    if (FAILED(Shell_NotifyIconGetRect(&nii, &iconRc))) {
        POINT pt; GetCursorPos(&pt);
        iconRc = { pt.x - 8, pt.y - 8, pt.x + 8, pt.y + 8 };
    }

    // Find the earliest upcoming fire time across all non-paused alarms.
    SYSTEMTIME now; GetLocalTime(&now);
    std::optional<SYSTEMTIME> earliest;
    const NoteData* earliestNote = nullptr;
    for (auto& note : m_notes) {
        if (!note->alarm.has_value()) continue;
        auto next = AlarmScheduler::ComputeNextFireTime(*note->alarm, now);
        if (!next.has_value()) continue;
        if (!earliest.has_value() ||
            AlarmScheduler::CompareSysTime(*next, *earliest) < 0) {
            earliest     = next;
            earliestNote = note.get();
        }
    }

    std::wstring header = L"UltraNote";
    std::wstring body;
    if (earliest.has_value() && earliestNote) {
        wchar_t dateBuf[64] = {}, timeBuf[32] = {};
        if (!GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &*earliest,
                            nullptr, dateBuf, 64)) dateBuf[0] = L'\0';
        if (!GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &*earliest,
                            nullptr, timeBuf, 32)) timeBuf[0] = L'\0';
        std::wstring title = earliestNote->title;
        if (title.empty()) {
            // Derive from first non-empty line of note text
            const std::wstring& t = earliestNote->text;
            size_t end = t.find_first_of(L"\r\n");
            title = t.substr(0, end);
            if (title.empty()) title = Ls(L"tray.bubble.untitled");
        }
        body = Ls(L"tray.bubble.next_alarm") + L" " +
               std::wstring(dateBuf) + L" " + timeBuf + L"\n" + title;
    } else {
        body = Ls(L"tray.bubble.no_alarms");
    }

    m_trayBubble->Show(iconRc, header, body);
}
