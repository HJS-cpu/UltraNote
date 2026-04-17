#pragma once

#include "Note.h"
#include <windows.h>
#include <string>

// Modeless popup dialog to configure an alarm for a single note.
// Pattern: programmatic controls (no .rc dialog template), own window class.
// Modal-ish: disables the owner note window while open, re-enables on destroy.
// One instance per NoteWindow — duplicate opens should be prevented by caller.
class AlarmConfigDialog {
public:
    // noteId is used only for identification/lookup; the dialog reads the note
    // via Application::FindNoteData when it needs the current state.
    AlarmConfigDialog(HINSTANCE hInst, HWND owner, uint64_t noteId);
    ~AlarmConfigDialog();

    bool Create();
    HWND GetHwnd() const { return m_hwnd; }

private:
    static bool EnsureClassRegistered(HINSTANCE hInst);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT, WPARAM, LPARAM);

    void CreateControls();
    void LoadFromNote();
    void WriteToNote();
    void UpdateControlStates();
    void UpdatePreview();
    void BrowseSoundFile();
    void OnOk();
    void OnRemove();

    HINSTANCE m_hInst;
    HWND      m_hOwner;
    uint64_t  m_noteId;

    HWND m_hwnd = nullptr;
    HFONT m_hFont = nullptr;

    // Control handles (grouped by section)
    HWND m_hStartDate = nullptr;
    HWND m_hStartTime = nullptr;

    HWND m_hRbOnce        = nullptr;
    HWND m_hRbDaily       = nullptr;
    HWND m_hRbEveryN      = nullptr;
    HWND m_hEditEveryN    = nullptr;
    HWND m_hRbWeekly      = nullptr;
    HWND m_hChkWd[7]      = {};  // Mon..Sun (index 0..6); stored order: Mon=0, Tue=1, ..., Sun=6
    HWND m_hRbMonthly     = nullptr;
    HWND m_hRbMonthlyDay  = nullptr;
    HWND m_hEditMonthlyDay= nullptr;
    HWND m_hRbMonthlyNth  = nullptr;
    HWND m_hEditMonthlyNth= nullptr;
    HWND m_hComboMonthlyWd= nullptr;
    HWND m_hRbQuarterly   = nullptr;
    HWND m_hEditQuarterDay= nullptr;
    HWND m_hRbYearly      = nullptr;

    HWND m_hRbEndNever    = nullptr;
    HWND m_hRbEndAfterN   = nullptr;
    HWND m_hEditEndCount  = nullptr;
    HWND m_hRbEndOnDate   = nullptr;
    HWND m_hEndDate       = nullptr;

    HWND m_hChkPopup      = nullptr;
    HWND m_hChkSound      = nullptr;
    HWND m_hEditSoundFile = nullptr;
    HWND m_hBtnBrowse     = nullptr;
    HWND m_hEditSnooze    = nullptr;
    HWND m_hChkPaused     = nullptr;

    HWND m_hPreview       = nullptr;
    HWND m_hBtnOk         = nullptr;
    HWND m_hBtnCancel     = nullptr;
    HWND m_hBtnRemove     = nullptr;

    // Label controls (kept for destroy)
    std::vector<HWND> m_labels;

    static bool s_classRegistered;
};
