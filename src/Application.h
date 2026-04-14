#pragma once

#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>
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
    NoteWindow* CreateNoteFromClipboard();
    void        RequestDeleteNote(uint64_t id);
    void        DeleteNote(uint64_t id);       // Delete without confirmation
    void        DeleteSelectedNotes();
    void        MarkDirty();
    void        SaveAll();

    // Note access
    NoteData*              FindNoteData(uint64_t id);
    std::vector<std::unique_ptr<NoteData>>& GetAllNotes() { return m_notes; }

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

    // Preview: show/hide note without entering edit mode
    NoteWindow* ShowNotePreview(uint64_t id);
    void HideNotePreview(uint64_t id);
    bool IsNoteVisible(uint64_t id) const;
    NoteWindow* FindNoteWindow(uint64_t id) const;
    void MoveNoteWindow(uint64_t id, int x, int y);

    // Note list window
    void ToggleNoteList();
    void RefreshNoteList();
    void ShowSearchInNoteList();

    // Settings dialog
    void ShowSettingsDialog();
    void ApplySettings();

    // About dialog
    void ShowAboutDialog(HWND hParent);
    void RegisterGlobalHotkeys();
    void UnregisterGlobalHotkeys();

    HINSTANCE GetInstance() const { return m_hInst; }
    bool      AreClickableLinksEnabled() const { return m_clickableLinks; }
    HBITMAP   GetMenuBitmap(SHSTOCKICONID id);
    HBITMAP   GetResourceBitmap(UINT iconResId);

private:
    Application() = default;

    bool CreateAppWindow();
    bool SetupTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void LoadMenuBitmaps();
    void AppendMenuItem(HMENU hMenu, UINT id, const wchar_t* text,
                        SHSTOCKICONID iconId, UINT flags = 0);
    void AppendMenuItemRes(HMENU hMenu, UINT id, const wchar_t* text,
                           UINT iconResId, UINT flags = 0);
    void ChangeLanguage(const std::wstring& langCode);

    static LRESULT CALLBACK AppWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    NoteWindow* CreateNoteWindow(NoteData* data);

    HINSTANCE       m_hInst     = nullptr;
    HWND            m_hAppWnd   = nullptr;
    NOTIFYICONDATA  m_nid       = {};

    std::vector<std::unique_ptr<NoteData>>       m_notes;
    std::vector<std::wstring>                    m_folders;
    std::unordered_map<uint64_t, NoteWindow*>    m_noteWindows;
    std::unique_ptr<NoteListWindow>              m_noteListWindow;

    uint64_t m_nextId       = 1;
    bool     m_dirty        = false;
    bool     m_notesVisible = true;
    bool     m_clickableLinks = true;
    int      m_cascadeX     = 100;
    int      m_cascadeY     = 100;

    // Cached menu bitmaps (shell stock icon id -> HBITMAP)
    std::unordered_map<int, HBITMAP> m_menuBitmaps;
    // Cached resource icon bitmaps (resource id -> HBITMAP)
    std::unordered_map<UINT, HBITMAP> m_resBitmaps;
};
