#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include "Note.h"

class FindInNoteDialog;
class AlarmConfigDialog;

class NoteWindow {
public:
    NoteWindow(NoteData* data, HINSTANCE hInst);
    ~NoteWindow();

    static bool RegisterWindowClass(HINSTANCE hInst);

    HWND     GetHwnd() const { return m_hwnd; }
    uint64_t GetNoteId() const { return m_data->id; }
    NoteData* GetData() const { return m_data; }

    void Show(bool show);
    void BringToFront();

    void SetSelected(bool selected);
    bool IsSelected() const { return m_selected; }

    void EnterEditMode();
    void ExitEditMode(bool save);
    bool IsEditing() const { return m_inEditMode; }

    // In-note search (Find Next). Returns true if a match was highlighted.
    bool FindNextInNote(const std::wstring& term, bool caseSensitive);
    void OpenFindDialog();

    // Alarm configuration dialog (one instance per note window)
    void OpenAlarmDialog();

    // Offset window position by delta (for multi-select drag)
    void OffsetPosition(int dx, int dy);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Subclass proc for the EDIT control
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                              LPARAM lParam, UINT_PTR subclassId,
                                              DWORD_PTR refData);

    // Painting
    void Paint(HDC hdc);
    void PaintBackground(HDC hdc, const RECT& rc);
    void PaintBorder(HDC hdc, const RECT& rc);
    void PaintText(HDC hdc, const RECT& rc);
    void PaintTextWithLinks(HDC hdc, const RECT& textRc);
    void PaintAttachmentBar(HDC hdc, const RECT& rc);

    // Attachment bar
    int  GetAttachmentBarHeight() const;
    RECT GetAttachmentBarRect() const;
    int  AttachmentHitTest(int x, int y) const;  // Returns attachment index or -1
    int  UrlHitTest(int x, int y) const;        // Returns index into m_urlRects or -1
    void HandleDropFiles(HDROP hDrop);
    void OpenAttachment(int index);
    void RemoveAttachment(int index);
    void ShowAttachmentContextMenu(int index, int screenX, int screenY);
    void UpdateTooltip();
    void DestroyAttachmentIcons();

    // Hit testing: returns HTCLIENT for body, or resize HTXXX codes for edges
    int HitTest(int x, int y) const;

    // Drag/resize
    void StartDrag(int x, int y);
    void UpdateDrag(int x, int y);
    void EndDrag();

    void StartResize(int hitZone, int x, int y);
    void UpdateResize(int x, int y);
    void EndResize();

    // Context menu
    void ShowContextMenu(int screenX, int screenY);
    void HandleMenuCommand(int cmd);

    // Sync NoteData position from current window rect
    void SyncDataFromWindow();

    // Notify application of changes
    void NotifyChanged();

    HWND       m_hwnd          = nullptr;
    HWND       m_hEditCtrl     = nullptr;
    HWND       m_hTooltip      = nullptr;
    NoteData*  m_data          = nullptr;
    HINSTANCE  m_hInst         = nullptr;
    HBRUSH     m_hEditBrush    = nullptr;

    bool       m_selected      = false;
    bool       m_inEditMode    = false;

    // Drag state
    bool       m_dragging      = false;
    POINT      m_dragStartCursor = {};
    POINT      m_dragStartPos    = {};

    // Resize state
    bool       m_resizing      = false;
    int        m_resizeHitZone = 0;
    POINT      m_resizeStartCursor = {};
    RECT       m_resizeStartRect   = {};

    // Cached URL rectangles for hit-testing (valid only in non-edit mode)
    struct UrlRect {
        RECT rect;
        std::wstring url;
    };
    std::vector<UrlRect> m_urlRects;
    int m_pendingUrlClick = -1;

    // Attachment icon cache
    std::vector<HICON> m_attachIcons;

    // In-note find dialog (modeless, one per note window)
    std::unique_ptr<FindInNoteDialog> m_findDialog;
    // Alarm configuration dialog (modal-ish, one per note window)
    std::unique_ptr<AlarmConfigDialog> m_alarmDialog;
    // Tracks where the next FindNext should begin. Independent of the
    // EDIT control's current selection so repeated matches advance reliably.
    size_t m_findSearchPos = 0;
    size_t m_findLastMatchStart = std::wstring::npos;
    size_t m_findLastMatchEnd   = std::wstring::npos;

    static constexpr int RESIZE_BORDER = 6;
    static constexpr int MIN_WIDTH     = 80;
    static constexpr int MIN_HEIGHT    = 40;
    static constexpr int TEXT_PADDING  = 6;
    static constexpr int ATTACH_ROW_HEIGHT = 20;
    static constexpr int ATTACH_ICON_SIZE  = 16;
    static constexpr int ATTACH_ITEM_PAD   = 4;
    static constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;
};
