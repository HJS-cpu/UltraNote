#pragma once

#include <windows.h>
#include <string>
#include <cstdint>

enum class AlarmAction {
    Dismiss,
    Snooze,
    OpenNote
};

// Topmost modeless popup shown when an alarm fires.
// One instance per fired alarm (multiple concurrent alarms stack y+30 offset).
// Self-deletes on close via WM_NCDESTROY; owner (Application) holds the pointer
// and is notified via Application::OnAlarmPopupClosed(noteId, action).
class AlarmPopupWindow {
public:
    AlarmPopupWindow(HINSTANCE hInst, uint64_t noteId,
                     const std::wstring& title, const std::wstring& text,
                     bool playSound, const std::wstring& soundFile,
                     int snoozeMinutes, int stackIndex);
    ~AlarmPopupWindow();

    bool Create();
    HWND GetHwnd() const { return m_hwnd; }
    uint64_t GetNoteId() const { return m_noteId; }
    int GetStackIndex() const { return m_stackIndex; }

private:
    static bool EnsureClassRegistered(HINSTANCE hInst);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void CreateControls();
    void StartSound();
    void StopSound();
    void OnAction(AlarmAction action);

    HINSTANCE    m_hInst;
    uint64_t     m_noteId;
    std::wstring m_title;
    std::wstring m_text;
    bool         m_playSound;
    std::wstring m_soundFile;
    int          m_snoozeMinutes;
    int          m_stackIndex;

    HWND m_hwnd       = nullptr;
    HWND m_hLblTitle  = nullptr;
    HWND m_hLblText   = nullptr;
    HWND m_hBtnOpen   = nullptr;
    HWND m_hBtnSnooze = nullptr;
    HWND m_hBtnDismiss = nullptr;
    HFONT m_hFontTitle = nullptr;
    HFONT m_hFontText  = nullptr;

    bool m_soundActive = false;
    bool m_actionTaken = false;

    static bool s_classRegistered;
};
