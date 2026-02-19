#pragma once

#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include "Note.h"

class NoteWindow;
class NoteListWindow;

class Application {
public:
    static Application& Get();

    bool Initialize(HINSTANCE hInst);
    int  Run();
    void Shutdown();

    // Note lifecycle
    NoteWindow* CreateNewNote();
    void        RequestDeleteNote(uint64_t id);
    void        DeleteSelectedNotes();
    void        MarkDirty();
    void        SaveAll();

    // Note access
    NoteData*              FindNoteData(uint64_t id);
    std::vector<NoteData>& GetAllNotes() { return m_notes; }

    // Folder management
    const std::vector<std::wstring>& GetFolders() const { return m_folders; }
    void AddFolder(const std::wstring& name);
    void RenameFolder(const std::wstring& oldName, const std::wstring& newName);
    void DeleteFolder(const std::wstring& name);
    void SetNoteFolder(uint64_t noteId, const std::wstring& folder);
    void RenameNote(uint64_t noteId, const std::wstring& newTitle);

    // Selection management
    void SelectNote(uint64_t id, bool addToSelection);
    void DeselectNote(uint64_t id);
    void ClearSelection();
    std::vector<uint64_t> GetSelectedIds() const;

    // Multi-select drag: move all selected notes by delta
    void MoveSelectedNotes(int dx, int dy, uint64_t excludeId);

    // Visibility
    void ShowAllNotes();
    void HideAllNotes();

    // Bring a note to front and optionally enter edit mode
    void BringNoteToFront(uint64_t id);

    // Note list window
    void ToggleNoteList();
    void RefreshNoteList();

    HINSTANCE GetInstance() const { return m_hInst; }
    HBITMAP   GetMenuBitmap(SHSTOCKICONID id);

private:
    Application() = default;

    bool CreateAppWindow();
    bool SetupTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void LoadMenuBitmaps();
    void AppendMenuItem(HMENU hMenu, UINT id, const wchar_t* text,
                        SHSTOCKICONID iconId, UINT flags = 0);
    void ChangeLanguage(const std::wstring& langCode);

    static LRESULT CALLBACK AppWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    NoteWindow* CreateNoteWindow(NoteData& data);

    HINSTANCE       m_hInst     = nullptr;
    HWND            m_hAppWnd   = nullptr;
    NOTIFYICONDATA  m_nid       = {};

    std::vector<NoteData>                        m_notes;
    std::vector<std::wstring>                    m_folders;
    std::unordered_map<uint64_t, NoteWindow*>    m_noteWindows;
    std::unique_ptr<NoteListWindow>              m_noteListWindow;

    uint64_t m_nextId       = 1;
    bool     m_dirty        = false;
    bool     m_notesVisible = true;
    int      m_cascadeX     = 100;
    int      m_cascadeY     = 100;

    // Cached language list from last tray menu build (for command dispatch)
    std::vector<std::pair<std::wstring, std::wstring>> m_availableLangs;

    // Cached menu bitmaps (shell stock icon id -> HBITMAP)
    std::unordered_map<int, HBITMAP> m_menuBitmaps;
};
