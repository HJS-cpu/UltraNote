#pragma once

#include <windows.h>
#include <commctrl.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AlarmConfigDialog;
class HeaderDragOverlay;

class NoteListWindow {
public:
    NoteListWindow(HINSTANCE hInst);
    ~NoteListWindow();

    static bool RegisterWindowClass(HINSTANCE hInst);

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const;
    HWND GetHwnd() const { return m_hwnd; }

    void Refresh();
    void SetPreviewEnabled(bool enabled);
    void SetPreviewPaused(bool paused);
    void FocusSearchField();

    // Simple input dialog helper (also used by NoteWindow for "Rename note"
    // so the two rename flows share pixel-identical look & feel).
    static bool InputDialog(HWND parent, const std::wstring& prompt,
                            const std::wstring& title, std::wstring& value);

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

    void SortByColumn(int col);       // User click: toggles direction
    void ApplyCurrentSort();           // Re-apply current sort without toggling
    static int CALLBACK CompareFunc(LPARAM lp1, LPARAM lp2, LPARAM sortParam);

    void EditSelectedNote();
    void DeleteSelectedNotes();
    void RenameSelectedNote();
    void OpenAlarmForSelected();
    void ShowSetFolderMenu();
    void ToggleNoteHidden(uint64_t noteId);
    void ToggleNoteAlwaysOnTop(uint64_t noteId);

    // Folder context menu
    void ShowFolderContextMenu(int screenX, int screenY);
    void NewFolderDialog();
    void RenameFolderDialog(const std::wstring& oldName);
    void DeleteFolderConfirm(const std::wstring& name);

    // ListView context menu
    void ShowNoteContextMenu(int screenX, int screenY);

    // Dialog proc for InputDialog (kept private; declared in public section above)
    static INT_PTR CALLBACK InputDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateSearchEdit();
    void CreateStatusBar();
    void UpdateStatusBar();
    static LRESULT CALLBACK SearchEditSubclassProc(
        HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subId, DWORD_PTR refData);
    static LRESULT CALLBACK HeaderSubclassProc(
        HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subId, DWORD_PTR refData);
    static LRESULT CALLBACK ListViewSubclassProc(
        HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subId, DWORD_PTR refData);

    // Header drag-drop feedback (ATnotes-style red insertion arrows)
    void OnHeaderBeginDrag(int sourceIdx);
    void OnHeaderEndDrag();
    void UpdateHeaderDragOverlay();

    HWND       m_hwnd           = nullptr;
    HWND       m_hToolbar       = nullptr;
    HWND       m_hFolderList    = nullptr;
    HWND       m_hListView      = nullptr;
    HWND       m_hSearchEdit    = nullptr;
    HWND       m_hStatusBar     = nullptr;
    HINSTANCE  m_hInst          = nullptr;
    HIMAGELIST m_hToolbarImages = nullptr;
    HICON      m_hFolderIcon    = nullptr;
    HICON      m_hAllNotesIcon  = nullptr;
    HICON      m_hSearchIcon    = nullptr;
    HFONT      m_hSearchFont    = nullptr;

    enum class FolderFilter { All, Unfiled, Named };
    FolderFilter m_folderFilter   = FolderFilter::All;
    std::wstring m_selectedFolder;  // Valid only when m_folderFilter == Named
    std::wstring m_searchQuery;    // Empty = no filter

    int  m_sortColumn    = -1;
    bool m_sortAscending = true;

    enum Column { COL_TITLE = 0, COL_TEXT, COL_FOLDER, COL_HIDDEN, COL_ONTOP, COL_CREATED, COL_ATTACH,
                  COL_NEXT_ALARM, COL_INTERVAL, COL_ALARM_STATUS, COL_COUNT };

    // Header context menu: toggle per-column visibility
    void ShowHeaderContextMenu(int screenX, int screenY);
    void SetColumnVisible(int col, bool visible);
    bool IsColumnVisible(int col) const;

    // Cached column-visibility state; widths of hidden columns are saved in m_columnSavedWidths
    // so we can restore them on re-show without losing the user's preferred width.
    bool     m_columnVisible[COL_COUNT] = {};
    int      m_columnSavedWidths[COL_COUNT] = {};

    // Splitter between folder list and listview
    int  m_folderListWidth   = 150;
    bool m_splitterDragging  = false;
    int  m_splitterDragStart = 0;
    int  m_splitterWidthStart = 0;

    static constexpr int SPLITTER_WIDTH      = 1;
    // Extra hit-test padding extending into the ListView's left edge so the
    // splitter is easier to grab even though it is visually only 1 px wide.
    // Realised via ListViewSubclassProc; the folder list side is left untouched
    // because the visual splitter already sits flush against its right border.
    static constexpr int SPLITTER_HIT_PAD    = 4;
    static constexpr int FOLDER_MIN_WIDTH    = 80;
    static constexpr int FOLDER_MAX_WIDTH    = 400;
    static constexpr int FOLDER_DEFAULT_WIDTH = 150;

    // Header click tracking (to ignore spurious WM_LBUTTONUP without prior WM_LBUTTONDOWN)
    bool     m_headerMouseDown    = false;

    // Header drag-drop state
    std::unique_ptr<HeaderDragOverlay> m_headerDragOverlay;
    bool     m_headerDragging     = false;
    int      m_headerDragSource   = -1;

    // Cached display settings (refreshed on Refresh/settings change)
    bool     m_zebraStriping      = false;

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

    // Alarm dialog opened for the note list's currently selected note. Owner
    // is this window, so the list is disabled while the dialog is modal-ish.
    std::unique_ptr<AlarmConfigDialog> m_alarmDialog;
};
