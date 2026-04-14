#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include "Note.h"

// Shortcut action identifiers
enum ShortcutAction {
    SC_DELETE = 0,
    SC_ALWAYS_ON_TOP,
    SC_GLOBAL_NEWNOTE,    // Global hotkey: new note from anywhere
    SC_GLOBAL_NOTELIST,   // Global hotkey: toggle note list from anywhere
    SC_COUNT
};

// Packed shortcut: LOBYTE = VK code, HIBYTE = modifiers (HOTKEYF_CONTROL etc.)
struct ShortcutDef {
    ShortcutAction action;
    const wchar_t* settingsKey;
    const wchar_t* locKey;
    WORD defaultHotkey;   // MAKEWORD(vk, mods)
};

struct SettingsData {
    // Layout (Tab 1)
    COLORREF bgColor        = RGB(255, 255, 153);
    COLORREF textColor      = RGB(0, 0, 0);
    COLORREF borderColor    = RGB(200, 200, 80);
    std::wstring fontFace   = L"Arial";
    int      fontSize       = 10;
    bool     fontBold       = false;
    bool     fontItalic     = false;

    // Keyboard (Tab 2)
    WORD     shortcuts[SC_COUNT] = {};

    // General (Tab 3)
    int      autosaveInterval   = 30;
    bool     confirmDelete      = true;
    bool     previewEnabled     = false;
    int      previewDelay       = 400;
    bool     clickableLinks     = true;
    int      trayDoubleClick    = 0;   // 0=new note, 1=note list, 2=show all
    std::wstring language       = L"en";

    // Misc (Tab 4)
    int      newNoteX           = 100;
    int      newNoteY           = 100;
    int      cascadeStep        = 20;
    int      cascadeReset       = 500;
    std::wstring defaultFolder;
    std::wstring initialText;
};

class SettingsDialog {
public:
    // Show modal settings dialog; returns true if user clicked OK/Apply
    static bool Show(HWND hParent);

    // Load current settings from storage
    static SettingsData LoadFromStorage();

    // Save settings to storage
    static void SaveToStorage(const SettingsData& data);

    // Get default shortcut definitions
    static const ShortcutDef* GetShortcutDefs();

private:
    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Dialog setup
    void OnInitDialog(HWND hwnd);
    void CreateTabs(HWND hwnd);
    void CreateLayoutTab(HWND hwnd);
    void CreateKeyboardTab(HWND hwnd);
    void CreateGeneralTab(HWND hwnd);
    void CreateMiscTab(HWND hwnd);

    // Tab switching
    void ShowTab(int index);

    // Control interaction
    void OnChooseColor(HWND hwnd, COLORREF& color, int swatchId);
    void OnChooseFont(HWND hwnd);
    void UpdateFontDisplay();
    void UpdatePreview();
    void OnShortcutSelChange();
    void OnShortcutChange();
    void OnShortcutDefault();
    void PopulateLanguageCombo();
    void PopulateFolderCombo();
    void ShowInsertVariableMenu();

    // Read current values from controls
    void ReadFromControls();

    // Rebuild all controls after language change
    void RebuildControls();

    HWND m_hwnd = nullptr;
    HWND m_hTab = nullptr;
    int  m_currentTab = 0;

    // All child controls per tab (for show/hide)
    std::vector<HWND> m_tabControls[4];

    // Current working copy of settings
    SettingsData m_data;

    // Available languages cache
    std::vector<std::pair<std::wstring, std::wstring>> m_langs;

    // General tab: scrollable panel + bold font for group headers
    HWND m_hGeneralPanel = nullptr;
    HFONT m_hBoldFont = nullptr;
    std::vector<HWND> m_groupHeaders;

    // Custom colors for ChooseColor dialog
    static COLORREF s_customColors[16];
};
