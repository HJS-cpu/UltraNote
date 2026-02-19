#pragma once

#include <windows.h>
#include <commctrl.h>
#include <cstdint>
#include <string>
#include <vector>

class NoteListWindow {
public:
    NoteListWindow(HINSTANCE hInst);
    ~NoteListWindow();

    static bool RegisterWindowClass(HINSTANCE hInst);

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const;

    void Refresh();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateToolbar();
    void CreateFolderList();
    void CreateListView();
    void SetupColumns();
    void PopulateList();
    void PopulateFolderList();
    void ResizeControls();

    void LoadSettings();
    void SaveSettings();

    void SortByColumn(int col);
    static int CALLBACK CompareFunc(LPARAM lp1, LPARAM lp2, LPARAM sortParam);

    void EditSelectedNote();
    void DeleteSelectedNotes();
    void RenameSelectedNote();
    void ShowSetFolderMenu();

    // Folder context menu
    void ShowFolderContextMenu(int screenX, int screenY);
    void NewFolderDialog();
    void RenameFolderDialog(const std::wstring& oldName);
    void DeleteFolderConfirm(const std::wstring& name);

    // ListView context menu
    void ShowNoteContextMenu(int screenX, int screenY);

    // Simple input dialog helper
    static bool InputDialog(HWND parent, const std::wstring& prompt,
                            const std::wstring& title, std::wstring& value);
    static INT_PTR CALLBACK InputDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND       m_hwnd           = nullptr;
    HWND       m_hToolbar       = nullptr;
    HWND       m_hFolderList    = nullptr;
    HWND       m_hListView      = nullptr;
    HINSTANCE  m_hInst          = nullptr;
    HIMAGELIST m_hToolbarImages = nullptr;
    HICON      m_hFolderIcon    = nullptr;
    HICON      m_hAllNotesIcon  = nullptr;

    std::wstring m_selectedFolder;  // Empty = show all notes

    int  m_sortColumn    = -1;
    bool m_sortAscending = true;

    enum Column { COL_TITLE = 0, COL_TEXT, COL_FOLDER, COL_CREATED, COL_COUNT };

    // Splitter between folder list and listview
    int  m_folderListWidth   = 150;
    bool m_splitterDragging  = false;
    int  m_splitterDragStart = 0;
    int  m_splitterWidthStart = 0;

    static constexpr int SPLITTER_WIDTH      = 4;
    static constexpr int FOLDER_MIN_WIDTH    = 80;
    static constexpr int FOLDER_MAX_WIDTH    = 400;
    static constexpr int FOLDER_DEFAULT_WIDTH = 150;

    // Preview state
    bool     m_previewEnabled     = false;
    int      m_previewNoteIdx     = -1;
    uint64_t m_previewNoteId      = 0;
    bool     m_previewWasHidden   = false;
    bool     m_previewTimerActive = false;
    int      m_previewPendingIdx  = -1;
    DWORD    m_previewHoverStart  = 0;
    int      m_previewOrigX       = 0;
    int      m_previewOrigY       = 0;

    void ShowPreviewNote(uint64_t noteId);
    void HidePreviewNote();
    void RestorePreviewPosition();
    void StartPreviewTimer();
    void StopPreviewTimer();

    // Input dialog data
    struct InputDlgData {
        std::wstring prompt;
        std::wstring title;
        std::wstring value;
    };
};
