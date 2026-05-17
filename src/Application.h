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
class AlarmPopupWindow;
class TrayBubbleWindow;
enum class AlarmAction;

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
    void BringNoteToFront(uint64_t id, bool enterEdit = true);

    // Search highlight (shown in all note windows while active)
    void SetSearchHighlight(const std::wstring& term);
    const std::wstring& GetSearchHighlight() const { return m_searchHighlight; }

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

    // Import / Export
    void ExportNotesByIds(HWND owner, const std::vector<uint64_t>& ids);
    void ImportNotesFromFile(HWND owner);

    // Print selected notes (resolves IDs against m_notes; shows PrintDlg)
    void PrintNoteByIds(HWND owner, const std::vector<uint64_t>& ids);

    // Settings dialog
    void ShowSettingsDialog();
    void ApplySettings();

    // About dialog
    void ShowAboutDialog(HWND hParent);
    void RegisterGlobalHotkeys();
    void UnregisterGlobalHotkeys();

    // Alarms
    void CheckDueAlarms();
    void OnAlarmPopupClosed(uint64_t noteId, AlarmAction action);

    HINSTANCE GetInstance() const { return m_hInst; }
    bool      AreClickableLinksEnabled() const { return m_clickableLinks; }
    COLORREF  GetSearchHighlightColor() const { return m_searchHlColor; }
    HBITMAP   GetMenuBitmap(SHSTOCKICONID id);
    HBITMAP   GetResourceBitmap(UINT iconResId);
    void      AppendMenuItem(HMENU hMenu, UINT id, const wchar_t* text,
                             SHSTOCKICONID iconId, UINT flags = 0);
    void      AppendMenuItemRes(HMENU hMenu, UINT id, const wchar_t* text,
                                UINT iconResId, UINT flags = 0);

private:
    Application() = default;

    bool CreateAppWindow();
    bool SetupTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void LoadMenuBitmaps();
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
    COLORREF m_searchHlColor  = RGB(255, 165, 0);
    bool     m_settingsOpen  = false;
    int      m_cascadeX     = 100;
    int      m_cascadeY     = 100;

    std::wstring m_searchHighlight;

    // Cached menu bitmaps (shell stock icon id -> HBITMAP)
    std::unordered_map<int, HBITMAP> m_menuBitmaps;
    // Cached resource icon bitmaps (resource id -> HBITMAP)
    std::unordered_map<UINT, HBITMAP> m_resBitmaps;

    // Active alarm popups (keyed by note id, one per note)
    std::unordered_map<uint64_t, AlarmPopupWindow*> m_alarmPopups;

    // Hover-balloon for the systray icon (custom-drawn tooltip replacement)
    std::unique_ptr<TrayBubbleWindow> m_trayBubble;

    void TriggerAlarm(NoteData& note);
    void ShowTrayBubble();
    void HideTrayBubble();
};
