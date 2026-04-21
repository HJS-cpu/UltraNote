#pragma once

#include <windows.h>
#include <string>

class NoteWindow;

// Small modeless find-in-note dialog, one instance per NoteWindow.
// Opens via the note's context menu entry "Search...". Searches the note's
// text starting at the current selection end, wrapping around on no-match.
class FindInNoteDialog {
public:
    FindInNoteDialog(HINSTANCE hInst, NoteWindow* owner);
    ~FindInNoteDialog();

    bool Create();
    void ShowAndFocus();
    HWND GetHwnd() const { return m_hwnd; }

private:
    static bool EnsureClassRegistered(HINSTANCE hInst);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK EditSubclassProc(HWND, UINT, WPARAM, LPARAM,
                                             UINT_PTR, DWORD_PTR);

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void LayoutControls();
    void OnFindNext();

    HINSTANCE   m_hInst     = nullptr;
    NoteWindow* m_owner     = nullptr;
    HWND        m_hwnd      = nullptr;
    HWND        m_hLabel    = nullptr;
    HWND        m_hEdit     = nullptr;
    HWND        m_hCase     = nullptr;
    HWND        m_hWhole    = nullptr;
    HWND        m_hFindBtn  = nullptr;
    HWND        m_hCloseBtn = nullptr;

    static constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;
    static bool s_classRegistered;
};
