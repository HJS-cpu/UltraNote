#include "SettingsDialog.h"
#include "Application.h"
#include "Storage.h"
#include "Localization.h"
#include "Utils.h"
#include "Resource.h"
#include <commctrl.h>

COLORREF SettingsDialog::s_customColors[16] = {};

// ----------------------------------------------------------------------------
// Autostart helpers (HKCU\Software\Microsoft\Windows\CurrentVersion\Run).
// This is the only Registry use in the whole project; see CLAUDE.md.
// ----------------------------------------------------------------------------
namespace {
    std::wstring GetSelfExePath() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        // n >= MAX_PATH means the path was truncated; fail closed so we never
        // write a broken path into the Run key (ApplyAutostartRegistry skips
        // the write on an empty return).
        if (n == 0 || n >= MAX_PATH) return std::wstring();
        return std::wstring(path, n);
    }

    bool ApplyAutostartRegistry(bool enable) {
        HKEY hKey = nullptr;
        LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) return false;

        bool ok = true;
        if (enable) {
            std::wstring exe = GetSelfExePath();
            if (exe.empty()) { RegCloseKey(hKey); return false; }
            std::wstring quoted = L"\"" + exe + L"\"";
            DWORD bytes = static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t));
            rc = RegSetValueExW(hKey, L"UltraNote", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(quoted.c_str()), bytes);
            ok = (rc == ERROR_SUCCESS);
        } else {
            rc = RegDeleteValueW(hKey, L"UltraNote");
            ok = (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
        }
        RegCloseKey(hKey);
        return ok;
    }
}

void SettingsDialog::SyncAutostart(bool enabled) {
    ApplyAutostartRegistry(enabled);
}

// Default shortcut definitions
static const ShortcutDef s_shortcutDefs[SC_COUNT] = {
    { SC_DELETE,         L"shortcut.delete",        L"settings.sc_delete",        MAKEWORD(VK_DELETE, 0) },
    { SC_ALWAYS_ON_TOP,  L"shortcut.ontop",         L"settings.sc_ontop",         MAKEWORD('O', HOTKEYF_ALT) },
    { SC_HIDE,           L"shortcut.hide",          L"settings.sc_hide",          MAKEWORD('H', HOTKEYF_ALT) },
    { SC_GLOBAL_NEWNOTE, L"shortcut.global_new",    L"settings.sc_global_new",    MAKEWORD('N', HOTKEYF_CONTROL | HOTKEYF_SHIFT) },
    { SC_GLOBAL_NOTELIST,L"shortcut.global_list",   L"settings.sc_global_list",   MAKEWORD('L', HOTKEYF_CONTROL | HOTKEYF_SHIFT) },
};

const ShortcutDef* SettingsDialog::GetShortcutDefs() {
    return s_shortcutDefs;
}

// Hotkey display formatting lives in Utils.h (FormatShortcut) so that menus and
// the settings list share a single source.

// ============================================================================
// Load / Save
// ============================================================================

SettingsData SettingsDialog::LoadFromStorage() {
    auto intS = Storage::LoadSettings();
    auto strS = Storage::LoadSettingsStr();
    SettingsData d;

    auto getInt = [&](const wchar_t* key, int def) -> int {
        auto it = intS.find(key);
        return (it != intS.end()) ? it->second : def;
    };
    auto getStr = [&](const wchar_t* key, const wchar_t* def) -> std::wstring {
        auto it = strS.find(key);
        return (it != strS.end()) ? it->second : def;
    };

    d.bgColor       = static_cast<COLORREF>(getInt(L"default.bgColor", static_cast<int>(RGB(255, 255, 153))));
    d.textColor     = static_cast<COLORREF>(getInt(L"default.textColor", static_cast<int>(RGB(0, 0, 0))));
    d.borderColor   = static_cast<COLORREF>(getInt(L"default.borderColor", static_cast<int>(RGB(200, 200, 80))));
    d.searchHlColor = static_cast<COLORREF>(getInt(L"display.searchHlColor", static_cast<int>(RGB(255, 165, 0))));
    d.fontFace      = getStr(L"default.fontFace", L"Arial");
    d.fontSize      = getInt(L"default.fontSize", 10);
    d.fontBold      = getInt(L"default.fontBold", 0) != 0;
    d.fontItalic    = getInt(L"default.fontItalic", 0) != 0;

    d.autosaveInterval = getInt(L"autosave.interval", 30);
    d.confirmDelete    = getInt(L"confirm.delete", 1) != 0;
    d.previewEnabled   = getInt(L"notelist.preview", 0) != 0;
    d.previewDelay     = getInt(L"preview.delay", 400);
    d.clickableLinks   = getInt(L"display.clickableLinks", 1) != 0;
    d.trayDoubleClick  = getInt(L"tray.doubleclick", 0);
    d.autostartEnabled = getInt(L"autostart.enabled", 0) != 0;
    d.language         = getStr(L"default.language", Localization::Get().GetCurrentLanguage().c_str());

    d.newNoteX       = getInt(L"newnote.x", 100);
    d.newNoteY       = getInt(L"newnote.y", 100);
    d.cascadeStep    = getInt(L"newnote.cascade", 20);
    d.cascadeReset   = getInt(L"newnote.cascade_reset", 500);
    d.defaultFolder  = getStr(L"newnote.folder", L"");
    d.initialText    = getStr(L"newnote.initialText", L"");
    d.newNoteAlwaysOnTop = getInt(L"newnote.alwaysOnTop", 0) != 0;

    d.dateFormat     = getInt(L"notelist.dateFormat", 0);
    d.zebraStriping  = getInt(L"notelist.zebra", 0) != 0;

    for (int i = 0; i < SC_COUNT; ++i) {
        d.shortcuts[i] = static_cast<WORD>(getInt(s_shortcutDefs[i].settingsKey,
                                                   s_shortcutDefs[i].defaultHotkey));
    }

    // Clamp values that drive timers / fonts to their UI ranges before they reach
    // SetTimer / CreateFont. A hand-edited or corrupt settings.json could otherwise
    // feed e.g. autosave.interval=0 (10 ms save-loop) or a negative font size.
    auto clampInt = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    d.autosaveInterval = clampInt(d.autosaveInterval, 10, 300);    // IDC_AUTOSAVE_SPIN range
    d.previewDelay     = clampInt(d.previewDelay, 100, 2000);      // IDC_PREVIEW_DELAY_SPIN range
    d.fontSize         = clampInt(d.fontSize, 6, 72);              // sane point-size bounds

    return d;
}

void SettingsDialog::SaveToStorage(const SettingsData& data) {
    auto intS = Storage::LoadSettings();
    auto strS = Storage::LoadSettingsStr();

    intS[L"default.bgColor"]       = static_cast<int>(data.bgColor);
    intS[L"default.textColor"]     = static_cast<int>(data.textColor);
    intS[L"default.borderColor"]   = static_cast<int>(data.borderColor);
    intS[L"display.searchHlColor"] = static_cast<int>(data.searchHlColor);
    strS[L"default.fontFace"]      = data.fontFace;
    intS[L"default.fontSize"]      = data.fontSize;
    intS[L"default.fontBold"]      = data.fontBold ? 1 : 0;
    intS[L"default.fontItalic"]    = data.fontItalic ? 1 : 0;

    intS[L"autosave.interval"]     = data.autosaveInterval;
    intS[L"confirm.delete"]        = data.confirmDelete ? 1 : 0;
    intS[L"notelist.preview"]      = data.previewEnabled ? 1 : 0;
    intS[L"preview.delay"]         = data.previewDelay;
    intS[L"display.clickableLinks"] = data.clickableLinks ? 1 : 0;
    intS[L"tray.doubleclick"]      = data.trayDoubleClick;
    intS[L"autostart.enabled"]     = data.autostartEnabled ? 1 : 0;
    strS[L"default.language"]      = data.language;

    intS[L"newnote.x"]             = data.newNoteX;
    intS[L"newnote.y"]             = data.newNoteY;
    intS[L"newnote.cascade"]       = data.cascadeStep;
    intS[L"newnote.cascade_reset"] = data.cascadeReset;
    strS[L"newnote.folder"]        = data.defaultFolder;
    strS[L"newnote.initialText"]  = data.initialText;
    intS[L"newnote.alwaysOnTop"]  = data.newNoteAlwaysOnTop ? 1 : 0;

    intS[L"notelist.dateFormat"]   = data.dateFormat;
    intS[L"notelist.zebra"]        = data.zebraStriping ? 1 : 0;

    for (int i = 0; i < SC_COUNT; ++i) {
        intS[s_shortcutDefs[i].settingsKey] = data.shortcuts[i];
    }

    Storage::SaveAllSettings(intS, strS);

    ApplyAutostartRegistry(data.autostartEnabled);
}

// ============================================================================
// Dialog show
// ============================================================================

bool SettingsDialog::Show(HWND hParent) {
    // Build in-memory dialog template
    struct {
        DLGTEMPLATE tmpl;
        WORD menu;
        WORD windowClass;
        WORD title;
    } dlg = {};

    dlg.tmpl.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.tmpl.cx = 240;  // DLU
    dlg.tmpl.cy = 245;  // DLU (Misc-Tab braucht Platz fuer Initial-Text-Vorschau)

    SettingsDialog self;
    self.m_data = LoadFromStorage();

    INT_PTR result = DialogBoxIndirectParamW(
        Application::Get().GetInstance(),
        &dlg.tmpl, hParent, DlgProc,
        reinterpret_cast<LPARAM>(&self));

    return result == IDOK;
}

INT_PTR CALLBACK SettingsDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* self = nullptr;

    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<SettingsDialog*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->OnInitDialog(hwnd);
        return FALSE;
    }

    self = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return FALSE;

    switch (msg) {
        case WM_COMMAND:
            // Live preview of the initial-text template
            if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_INITIAL_TEXT_EDIT) {
                self->UpdateInitialTextPreview();
                return TRUE;
            }
            switch (LOWORD(wParam)) {
                case IDC_SETTINGS_OK: {
                    self->ReadFromControls();
                    SaveToStorage(self->m_data);
                    Application::Get().ApplySettings();
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }

                case IDCANCEL:          // Esc / system close — DefDlgProc posts this
                case IDC_SETTINGS_CANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;

                case IDC_SETTINGS_APPLY: {
                    self->ReadFromControls();
                    std::wstring oldLang = Localization::Get().GetCurrentLanguage();
                    SaveToStorage(self->m_data);
                    Application::Get().ApplySettings();
                    // If language changed, rebuild all dialog controls
                    if (Localization::Get().GetCurrentLanguage() != oldLang) {
                        self->RebuildControls();
                    }
                    return TRUE;
                }

                case IDC_BG_COLOR_BTN:
                    self->OnChooseColor(hwnd, self->m_data.bgColor, IDC_BG_COLOR_SWATCH);
                    self->UpdatePreview();
                    return TRUE;

                case IDC_TEXT_COLOR_BTN:
                    self->OnChooseColor(hwnd, self->m_data.textColor, IDC_TEXT_COLOR_SWATCH);
                    self->UpdatePreview();
                    return TRUE;

                case IDC_BORDER_COLOR_BTN:
                    self->OnChooseColor(hwnd, self->m_data.borderColor, IDC_BORDER_COLOR_SWATCH);
                    self->UpdatePreview();
                    return TRUE;

                case IDC_SEARCH_HL_BTN:
                    self->OnChooseColor(hwnd, self->m_data.searchHlColor, IDC_SEARCH_HL_SWATCH);
                    return TRUE;

                case IDC_FONT_BTN:
                    self->OnChooseFont(hwnd);
                    self->UpdatePreview();
                    return TRUE;

                case IDC_SHORTCUT_CHANGE:
                    self->OnShortcutChange();
                    return TRUE;

                case IDC_SHORTCUT_DEFAULT:
                    self->OnShortcutDefault();
                    return TRUE;

                case IDC_INITIAL_TEXT_INSERT:
                    self->ShowInsertVariableMenu();
                    return TRUE;
            }

            // Handle insert variable menu selection
            if (LOWORD(wParam) >= ID_INITTEXT_BASE && LOWORD(wParam) <= ID_INITTEXT_MAX) {
                // Must match order in ShowInsertVariableMenu()
                static const wchar_t* s_varCodes[] = {
                    L"%c", L"%x", L"%X",
                    L"%#c", L"%#x",
                    L"%d", L"%m", L"%y", L"%Y",
                    L"%H", L"%I", L"%M", L"%S", L"%p",
                    L"%a", L"%A", L"%b", L"%B",
                    L"%%", L"%%p"
                };
                int idx = LOWORD(wParam) - ID_INITTEXT_BASE;
                if (idx >= 0 && idx < _countof(s_varCodes)) {
                    HWND hEdit = GetDlgItem(hwnd, IDC_INITIAL_TEXT_EDIT);
                    if (hEdit) {
                        SetFocus(hEdit);
                        SendMessageW(hEdit, EM_REPLACESEL, TRUE,
                            reinterpret_cast<LPARAM>(s_varCodes[idx]));
                    }
                }
                return TRUE;
            }
            break;

        case WM_NOTIFY: {
            auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == IDC_SETTINGS_TAB && nmhdr->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(self->m_hTab);
                self->ShowTab(sel);
                return TRUE;
            }
            if (nmhdr->idFrom == IDC_SHORTCUT_LIST && nmhdr->code == LVN_ITEMCHANGED) {
                self->OnShortcutSelChange();
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            COLORREF color = RGB(0, 0, 0);
            if (dis->CtlID == IDC_BG_COLOR_SWATCH) color = self->m_data.bgColor;
            else if (dis->CtlID == IDC_TEXT_COLOR_SWATCH) color = self->m_data.textColor;
            else if (dis->CtlID == IDC_BORDER_COLOR_SWATCH) color = self->m_data.borderColor;
            else if (dis->CtlID == IDC_SEARCH_HL_SWATCH) color = self->m_data.searchHlColor;
            else if (dis->CtlID == IDC_LAYOUT_PREVIEW) {
                // Draw preview box
                HBRUSH bgBrush = CreateSolidBrush(self->m_data.bgColor);
                FillRect(dis->hDC, &dis->rcItem, bgBrush);
                DeleteObject(bgBrush);

                HPEN borderPen = CreatePen(PS_SOLID, 1, self->m_data.borderColor);
                HGDIOBJ oldPen = SelectObject(dis->hDC, borderPen);
                HGDIOBJ oldBr = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top,
                          dis->rcItem.right, dis->rcItem.bottom);
                SelectObject(dis->hDC, oldBr);
                SelectObject(dis->hDC, oldPen);
                DeleteObject(borderPen);

                HFONT hFont = CreateFontFromParams(self->m_data.fontFace,
                                                    self->m_data.fontSize,
                                                    self->m_data.fontBold,
                                                    self->m_data.fontItalic);
                HGDIOBJ oldFont = SelectObject(dis->hDC, hFont);
                SetTextColor(dis->hDC, self->m_data.textColor);
                SetBkMode(dis->hDC, TRANSPARENT);
                RECT textRc = dis->rcItem;
                InflateRect(&textRc, -4, -4);
                DrawTextW(dis->hDC, Ls(L"settings.preview_text").c_str(), -1,
                          &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
                SelectObject(dis->hDC, oldFont);
                DeleteObject(hFont);
                return TRUE;
            }
            else break;

            // Color swatch
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dis->hDC, &dis->rcItem, brush);
            DeleteObject(brush);
            FrameRect(dis->hDC, &dis->rcItem,
                      static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            return TRUE;
        }

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;

        case WM_DESTROY:
            // Free the GDI/USER handles created during the dialog's life.
            // Create sites already destroy the previous handle on RebuildControls
            // re-entry, so the members hold only the final live instances here.
            if (self->m_hBoldFont)  { DeleteObject(self->m_hBoldFont); self->m_hBoldFont = nullptr; }
            if (self->m_hTitleIcon) { DestroyIcon(self->m_hTitleIcon); self->m_hTitleIcon = nullptr; }
            if (self->m_hGroupIcon) { DestroyIcon(self->m_hGroupIcon); self->m_hGroupIcon = nullptr; }
            return FALSE;
    }

    return FALSE;
}

// ============================================================================
// Dialog init
// ============================================================================

void SettingsDialog::OnInitDialog(HWND hwnd) {
    m_hwnd = hwnd;
    SetWindowTextW(hwnd, Ls(L"settings.title").c_str());

    // Set dialog icon (shown in title bar). Stored as a member and destroyed
    // both on re-entry (RebuildControls re-runs OnInitDialog) and at WM_DESTROY,
    // since LoadImageW without LR_SHARED hands us an owned icon.
    if (m_hTitleIcon) { DestroyIcon(m_hTitleIcon); m_hTitleIcon = nullptr; }
    m_hTitleIcon = static_cast<HICON>(LoadImageW(
        Application::Get().GetInstance(), MAKEINTRESOURCE(IDI_SETTINGS),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    if (m_hTitleIcon) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_hTitleIcon));

    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    // OK / Cancel / Apply buttons
    RECT rc;
    GetClientRect(hwnd, &rc);
    int btnW = 75, btnH = 23;
    int btnY = rc.bottom - btnH - 8;

    HWND hOk = CreateWindowExW(0, L"BUTTON", Ls(L"settings.ok").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        rc.right - 3 * (btnW + 6) - 4, btnY, btnW, btnH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_OK)),
        nullptr, nullptr);

    HWND hCancel = CreateWindowExW(0, L"BUTTON", Ls(L"settings.cancel").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        rc.right - 2 * (btnW + 6) - 4, btnY, btnW, btnH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_CANCEL)),
        nullptr, nullptr);

    HWND hApply = CreateWindowExW(0, L"BUTTON", Ls(L"settings.apply").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        rc.right - (btnW + 6) - 4, btnY, btnW, btnH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_APPLY)),
        nullptr, nullptr);

    SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    SendMessageW(hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    SendMessageW(hApply, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    CreateTabs(hwnd);
    CreateLayoutTab(hwnd);
    CreateKeyboardTab(hwnd);
    CreateGeneralTab(hwnd);
    CreateMiscTab(hwnd);
    CreateNoteListTab(hwnd);

    // Apply font to all tab controls
    for (int t = 0; t < 5; ++t) {
        for (HWND h : m_tabControls[t]) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }
    }

    // Re-apply bold font to group headers (overwritten by loop above)
    if (m_hBoldFont) {
        for (HWND h : m_groupHeaders) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(m_hBoldFont), TRUE);
        }
    }

    ShowTab(0);
}

void SettingsDialog::CreateTabs(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    m_hTab = CreateWindowExW(0, WC_TABCONTROL, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        4, 4, rc.right - 8, rc.bottom - 40,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_TAB)),
        nullptr, nullptr);

    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(m_hTab, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    const wchar_t* tabKeys[] = {
        L"settings.tab_layout", L"settings.tab_keyboard",
        L"settings.tab_general", L"settings.tab_misc",
        L"settings.tab_notelist"
    };
    for (int i = 0; i < 5; ++i) {
        TCITEMW tie = {};
        tie.mask = TCIF_TEXT;
        tie.pszText = const_cast<wchar_t*>(Ls(tabKeys[i]).c_str());
        TabCtrl_InsertItem(m_hTab, i, &tie);
    }
}

// Helper to get the display area inside the tab control
static RECT GetTabDisplayRect(HWND hTab) {
    RECT rc;
    GetClientRect(hTab, &rc);
    TabCtrl_AdjustRect(hTab, FALSE, &rc);

    // Convert to parent coords
    POINT pt = { rc.left, rc.top };
    ClientToScreen(hTab, &pt);
    ScreenToClient(GetParent(hTab), &pt);
    rc.right = pt.x + (rc.right - rc.left);
    rc.bottom = pt.y + (rc.bottom - rc.top);
    rc.left = pt.x;
    rc.top = pt.y;
    return rc;
}

// ============================================================================
// Tab 1: Layout
// ============================================================================

void SettingsDialog::CreateLayoutTab(HWND hwnd) {
    RECT tabRc = GetTabDisplayRect(m_hTab);
    int x = tabRc.left + 10;
    int y = tabRc.top + 8;
    int labelW = 100, swatchW = 24, swatchH = 20, btnW = 70, btnH = 23;
    int rowH = 28;

    auto addColorRow = [&](const wchar_t* labelKey, int swatchId, int btnId) {
        HWND hLabel = CreateWindowExW(0, L"STATIC", Ls(labelKey).c_str(),
            WS_CHILD | SS_LEFT, x, y + 2, labelW, 18, hwnd, nullptr, nullptr, nullptr);
        m_tabControls[0].push_back(hLabel);

        HWND hSwatch = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | SS_OWNERDRAW, x + labelW, y, swatchW, swatchH,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(swatchId)),
            nullptr, nullptr);
        m_tabControls[0].push_back(hSwatch);

        HWND hBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.change").c_str(),
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
            x + labelW + swatchW + 8, y, btnW, btnH,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(btnId)),
            nullptr, nullptr);
        m_tabControls[0].push_back(hBtn);

        y += rowH;
    };

    addColorRow(L"settings.bg_color", IDC_BG_COLOR_SWATCH, IDC_BG_COLOR_BTN);
    addColorRow(L"settings.text_color", IDC_TEXT_COLOR_SWATCH, IDC_TEXT_COLOR_BTN);
    addColorRow(L"settings.border_color", IDC_BORDER_COLOR_SWATCH, IDC_BORDER_COLOR_BTN);
    addColorRow(L"settings.search_hl_color", IDC_SEARCH_HL_SWATCH, IDC_SEARCH_HL_BTN);

    // Font row
    HWND hFontLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.font").c_str(),
        WS_CHILD | SS_LEFT, x, y + 2, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[0].push_back(hFontLabel);

    HWND hFontDisplay = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
        WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
        x + labelW, y, 150, swatchH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FONT_DISPLAY)),
        nullptr, nullptr);
    m_tabControls[0].push_back(hFontDisplay);

    HWND hFontBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.change").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        x + labelW + 158, y, btnW, btnH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FONT_BTN)),
        nullptr, nullptr);
    m_tabControls[0].push_back(hFontBtn);
    y += rowH + 4;

    UpdateFontDisplay();

    // Preview
    HWND hPreviewLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.preview_label").c_str(),
        WS_CHILD | SS_LEFT, x, y, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[0].push_back(hPreviewLabel);
    y += 20;

    HWND hPreview = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | SS_OWNERDRAW,
        x, y, tabRc.right - tabRc.left - 20, 80,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LAYOUT_PREVIEW)),
        nullptr, nullptr);
    m_tabControls[0].push_back(hPreview);
}

// ============================================================================
// Tab 2: Keyboard
// ============================================================================

void SettingsDialog::CreateKeyboardTab(HWND hwnd) {
    RECT tabRc = GetTabDisplayRect(m_hTab);
    int x = tabRc.left + 10;
    int y = tabRc.top + 8;
    int listW = tabRc.right - tabRc.left - 20;
    int listH = 130;

    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        x, y, listW, listH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_LIST)),
        nullptr, nullptr);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_tabControls[1].push_back(hList);

    // Columns
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = listW / 2;
    lvc.pszText = const_cast<wchar_t*>(Ls(L"settings.sc_action").c_str());
    ListView_InsertColumn(hList, 0, &lvc);
    lvc.pszText = const_cast<wchar_t*>(Ls(L"settings.sc_key").c_str());
    ListView_InsertColumn(hList, 1, &lvc);

    // Populate
    for (int i = 0; i < SC_COUNT; ++i) {
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = const_cast<wchar_t*>(Ls(s_shortcutDefs[i].locKey).c_str());
        ListView_InsertItem(hList, &lvi);

        std::wstring keyStr = FormatShortcut(m_data.shortcuts[i]);
        ListView_SetItemText(hList, i, 1, const_cast<wchar_t*>(keyStr.c_str()));
    }

    y += listH + 8;

    // Hotkey control + buttons
    HWND hHkLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.sc_newkey").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, 100, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[1].push_back(hHkLabel);

    HWND hHotkey = CreateWindowExW(0, HOTKEY_CLASS, L"",
        WS_CHILD | WS_TABSTOP | WS_BORDER,
        x + 100, y, 130, 23,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_HOTKEY)),
        nullptr, nullptr);
    // Accept any modifier combination — including Alt-only — so HKM_GETHOTKEY
    // returns the user's input verbatim. Without explicit rules some shells
    // strip the Alt modifier from displayed/returned hotkeys.
    SendMessageW(hHotkey, HKM_SETRULES, 0, 0);
    m_tabControls[1].push_back(hHotkey);

    HWND hChangeBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.sc_apply").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        x + 238, y, 70, 23,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_CHANGE)),
        nullptr, nullptr);
    m_tabControls[1].push_back(hChangeBtn);

    HWND hDefBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.sc_default").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        x + 314, y, 70, 23,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_DEFAULT)),
        nullptr, nullptr);
    m_tabControls[1].push_back(hDefBtn);
}

// ============================================================================
// Scrollable options panel (used by General tab)
// ============================================================================

static LRESULT CALLBACK ScrollPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_VSCROLL: {
            SCROLLINFO si = {};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int oldPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP:     si.nPos -= 20; break;
                case SB_LINEDOWN:   si.nPos += 20; break;
                case SB_PAGEUP:     si.nPos -= si.nPage; break;
                case SB_PAGEDOWN:   si.nPos += si.nPage; break;
                case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
            }
            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            GetScrollInfo(hwnd, SB_VERT, &si);
            if (si.nPos != oldPos) {
                ScrollWindowEx(hwnd, 0, oldPos - si.nPos,
                    nullptr, nullptr, nullptr, nullptr,
                    SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            SendMessageW(hwnd, WM_VSCROLL,
                delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }

        case WM_COMMAND:
            return SendMessageW(GetParent(hwnd), msg, wParam, lParam);

        case WM_ERASEBKGND: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
            return TRUE;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void RegisterScrollPanelClass() {
    static bool registered = false;
    if (registered) return;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ScrollPanelProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"UltraNoteScrollPanel";
    RegisterClassExW(&wc);
    registered = true;
}

// ============================================================================
// Tab 3: General
// ============================================================================

void SettingsDialog::CreateGeneralTab(HWND hwnd) {
    RECT tabRc = GetTabDisplayRect(m_hTab);

    // Create bold font for group headers
    if (m_hBoldFont) {
        DeleteObject(m_hBoldFont);
        m_hBoldFont = nullptr;
    }
    m_groupHeaders.clear();

    LOGFONTW lf = {};
    HFONT hDefaultFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    GetObjectW(hDefaultFont, sizeof(lf), &lf);
    lf.lfWeight = FW_BOLD;
    m_hBoldFont = CreateFontIndirectW(&lf);

    // Create scrollable panel filling the tab area
    RegisterScrollPanelClass();
    int panelX = tabRc.left + 4;
    int panelY = tabRc.top + 4;
    int panelW = tabRc.right - tabRc.left - 8;
    int panelH = tabRc.bottom - tabRc.top - 8;

    // WS_EX_CONTROLPARENT: the dialog manager's Tab navigation only recurses into
    // child containers that carry this flag — without it the General-tab controls
    // (which live inside this scroll panel, not directly under the dialog) would be
    // unreachable by keyboard.
    m_hGeneralPanel = CreateWindowExW(WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT,
        L"UltraNoteScrollPanel", L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL,
        panelX, panelY, panelW, panelH,
        hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(m_hGeneralPanel);

    // All controls are children of the panel
    HWND panel = m_hGeneralPanel;
    int x = 6, y = 4;
    int indent = 20;
    int subIndent = 16;
    int editW = 50;
    int headerH = 18;
    int rowH = 22;
    int groupGap = 6;
    int contentW = panelW - 30;  // account for scrollbar + margins

    // Load group header icon (small, 16x16)
    int iconCx = 16;
    int iconCy = 16;
    if (m_hGroupIcon) { DestroyIcon(m_hGroupIcon); m_hGroupIcon = nullptr; }
    m_hGroupIcon = static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_GROUP),
        IMAGE_ICON, iconCx, iconCy, LR_DEFAULTCOLOR));

    auto addGroupHeader = [&](const wchar_t* labelKey) {
        // Icon before the text
        HWND hIcon = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | SS_ICON,
            x, y + (headerH - iconCy) / 2 - 1, iconCx, iconCy,
            panel, nullptr, nullptr, nullptr);
        SendMessageW(hIcon, STM_SETICON, reinterpret_cast<WPARAM>(m_hGroupIcon), 0);

        // Label text after the icon
        int textX = x + iconCx + 4;
        HWND hLabel = CreateWindowExW(0, L"STATIC", Ls(labelKey).c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            textX, y, contentW - iconCx - 4, headerH,
            panel, nullptr, nullptr, nullptr);
        SendMessageW(hLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hBoldFont), TRUE);
        m_groupHeaders.push_back(hLabel);
        y += headerH + 2;
    };

    int ix = x + indent;

    // --- Group: Display ---
    addGroupHeader(L"settings.group_display");

    HWND hPreviewCheck = CreateWindowExW(0, L"BUTTON", Ls(L"settings.preview_enabled").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        ix, y, contentW - indent, 18,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW_ENABLED)),
        nullptr, nullptr);
    SendMessageW(hPreviewCheck, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    if (m_data.previewEnabled)
        SendMessageW(hPreviewCheck, BM_SETCHECK, BST_CHECKED, 0);
    y += rowH;

    HWND hLinksCheck = CreateWindowExW(0, L"BUTTON", Ls(L"settings.clickable_links").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        ix, y, contentW - indent, 18,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CLICKABLE_LINKS)),
        nullptr, nullptr);
    SendMessageW(hLinksCheck, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    if (m_data.clickableLinks)
        SendMessageW(hLinksCheck, BM_SETCHECK, BST_CHECKED, 0);
    y += rowH;

    HWND hNewNoteTopCheck = CreateWindowExW(0, L"BUTTON", Ls(L"settings.newnote_alwaysontop").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        ix, y, contentW - indent, 18,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NEWNOTE_ALWAYSONTOP)),
        nullptr, nullptr);
    SendMessageW(hNewNoteTopCheck, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    if (m_data.newNoteAlwaysOnTop)
        SendMessageW(hNewNoteTopCheck, BM_SETCHECK, BST_CHECKED, 0);
    y += rowH;

    HWND hDelayLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.preview_delay").c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        ix + subIndent, y + 2, 130, 18, panel, nullptr, nullptr, nullptr);
    SendMessageW(hDelayLabel, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);

    HWND hEdit2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
        ix + subIndent + 130, y, editW, 20,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW_DELAY_EDIT)),
        nullptr, nullptr);
    SendMessageW(hEdit2, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);

    HWND hSpin2 = CreateWindowExW(0, UPDOWN_CLASS, L"",
        WS_CHILD | WS_VISIBLE | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
        0, 0, 0, 0,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW_DELAY_SPIN)),
        nullptr, nullptr);
    SendMessageW(hSpin2, UDM_SETRANGE32, 100, 2000);
    SendMessageW(hSpin2, UDM_SETPOS32, 0, m_data.previewDelay);

    HWND hMs = CreateWindowExW(0, L"STATIC", L"ms",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        ix + subIndent + 130 + editW + 8, y + 2, 30, 18,
        panel, nullptr, nullptr, nullptr);
    SendMessageW(hMs, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    y += rowH + groupGap;

    // --- Group: Delete ---
    addGroupHeader(L"settings.group_delete");

    HWND hCheck = CreateWindowExW(0, L"BUTTON", Ls(L"settings.confirm_delete").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        ix, y, contentW - indent, 18,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CONFIRM_DELETE)),
        nullptr, nullptr);
    SendMessageW(hCheck, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    if (m_data.confirmDelete)
        SendMessageW(hCheck, BM_SETCHECK, BST_CHECKED, 0);
    y += rowH + groupGap;

    // --- Group: Save ---
    addGroupHeader(L"settings.group_save");

    HWND hAutoLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.autosave").c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        ix, y + 2, 110, 18, panel, nullptr, nullptr, nullptr);
    SendMessageW(hAutoLabel, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);

    HWND hEdit1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
        ix + 110, y, editW, 20,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSAVE_EDIT)),
        nullptr, nullptr);
    SendMessageW(hEdit1, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);

    HWND hSpin1 = CreateWindowExW(0, UPDOWN_CLASS, L"",
        WS_CHILD | WS_VISIBLE | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
        0, 0, 0, 0,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSAVE_SPIN)),
        nullptr, nullptr);
    SendMessageW(hSpin1, UDM_SETRANGE32, 10, 300);
    SendMessageW(hSpin1, UDM_SETPOS32, 0, m_data.autosaveInterval);

    HWND hSec1 = CreateWindowExW(0, L"STATIC", Ls(L"settings.seconds").c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        ix + 110 + editW + 8, y + 2, 40, 18,
        panel, nullptr, nullptr, nullptr);
    SendMessageW(hSec1, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    y += rowH + groupGap;

    // --- Group: System Tray ---
    addGroupHeader(L"settings.group_tray");

    HWND hTrayLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.tray_dblclick").c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        ix, y + 3, 110, 18, panel, nullptr, nullptr, nullptr);
    SendMessageW(hTrayLabel, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);

    HWND hCombo1 = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        ix + 110, y, 160, 120,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TRAY_DBLCLICK)),
        nullptr, nullptr);
    SendMessageW(hCombo1, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    SendMessageW(hCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Ls(L"settings.tray_newnote").c_str()));
    SendMessageW(hCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Ls(L"settings.tray_notelist").c_str()));
    SendMessageW(hCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Ls(L"settings.tray_showall").c_str()));
    SendMessageW(hCombo1, CB_SETCURSEL, m_data.trayDoubleClick, 0);
    y += rowH + groupGap;

    // --- Group: Startup ---
    addGroupHeader(L"settings.group_startup");

    HWND hAutostartCheck = CreateWindowExW(0, L"BUTTON",
        Ls(L"settings.autostart_enabled").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        ix, y, contentW - indent, 18,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSTART_ENABLED)),
        nullptr, nullptr);
    SendMessageW(hAutostartCheck, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    if (m_data.autostartEnabled)
        SendMessageW(hAutostartCheck, BM_SETCHECK, BST_CHECKED, 0);
    y += rowH + groupGap;

    // --- Group: Language ---
    addGroupHeader(L"settings.language");

    HWND hLangCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        ix, y, 160, 120,
        panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LANGUAGE)),
        nullptr, nullptr);
    SendMessageW(hLangCombo, WM_SETFONT, reinterpret_cast<WPARAM>(hDefaultFont), TRUE);
    y += rowH;

    // Set scroll range based on content height
    int totalContentH = y + 4;
    RECT panelRc;
    GetClientRect(panel, &panelRc);
    int visibleH = panelRc.bottom;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = totalContentH;
    si.nPage = visibleH;
    SetScrollInfo(panel, SB_VERT, &si, TRUE);

    ShowScrollBar(panel, SB_VERT, totalContentH > visibleH);

    PopulateLanguageCombo();
}

// ============================================================================
// Tab 4: Misc
// ============================================================================

void SettingsDialog::CreateMiscTab(HWND hwnd) {
    RECT tabRc = GetTabDisplayRect(m_hTab);
    int x = tabRc.left + 10;
    int y = tabRc.top + 8;
    int labelW = 170, editW = 60, rowH = 28;

    auto addSpinRow = [&](const wchar_t* labelKey, int editId, int spinId,
                          int minVal, int maxVal, int curVal, const wchar_t* unitKey = nullptr) {
        HWND hLabel = CreateWindowExW(0, L"STATIC", Ls(labelKey).c_str(),
            WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
        m_tabControls[3].push_back(hLabel);

        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
            x + labelW, y, editW, 22,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(editId)),
            nullptr, nullptr);
        m_tabControls[3].push_back(hEdit);

        HWND hSpin = CreateWindowExW(0, UPDOWN_CLASS, L"",
            WS_CHILD | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(spinId)),
            nullptr, nullptr);
        SendMessageW(hSpin, UDM_SETRANGE32, minVal, maxVal);
        SendMessageW(hSpin, UDM_SETPOS32, 0, curVal);
        m_tabControls[3].push_back(hSpin);

        if (unitKey) {
            HWND hUnit = CreateWindowExW(0, L"STATIC", Ls(unitKey).c_str(),
                WS_CHILD | SS_LEFT, x + labelW + editW + 20, y + 3, 30, 18,
                hwnd, nullptr, nullptr, nullptr);
            m_tabControls[3].push_back(hUnit);
        }

        y += rowH;
    };

    addSpinRow(L"settings.newnote_x", IDC_NEWNOTE_X_EDIT, IDC_NEWNOTE_X_SPIN,
               0, 3000, m_data.newNoteX, L"settings.px");
    addSpinRow(L"settings.newnote_y", IDC_NEWNOTE_Y_EDIT, IDC_NEWNOTE_Y_SPIN,
               0, 3000, m_data.newNoteY, L"settings.px");
    addSpinRow(L"settings.cascade_step", IDC_CASCADE_STEP_EDIT, IDC_CASCADE_STEP_SPIN,
               10, 50, m_data.cascadeStep, L"settings.px");
    addSpinRow(L"settings.cascade_reset", IDC_CASCADE_RESET_EDIT, IDC_CASCADE_RESET_SPIN,
               200, 1000, m_data.cascadeReset, L"settings.px");

    // Default folder
    HWND hLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.default_folder").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[3].push_back(hLabel);

    HWND hCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        x + labelW, y, 160, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEFAULT_FOLDER)),
        nullptr, nullptr);
    m_tabControls[3].push_back(hCombo);

    PopulateFolderCombo();

    y += rowH;

    // Initial text label
    HWND hInitLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.initial_text").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[3].push_back(hInitLabel);

    // Insert button (right-aligned next to label)
    HWND hInsertBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.insert_var").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        x + labelW, y, 100, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INITIAL_TEXT_INSERT)),
        nullptr, nullptr);
    m_tabControls[3].push_back(hInsertBtn);
    y += rowH;

    // Multiline edit for initial text
    int initEditW = labelW + 100;
    int initEditH = 80;
    HWND hInitEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_data.initialText.c_str(),
        WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL,
        x, y, initEditW, initEditH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INITIAL_TEXT_EDIT)),
        nullptr, nullptr);
    m_tabControls[3].push_back(hInitEdit);
    y += initEditH + 6;

    // Preview label + read-only multiline edit showing the resolved text
    HWND hPreviewLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.preview_label").c_str(),
        WS_CHILD | SS_LEFT, x, y, labelW, 16, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[3].push_back(hPreviewLabel);
    y += 18;

    int cursorPos = -1;
    std::wstring resolved = ExpandInitialText(m_data.initialText, cursorPos);
    HWND hPreview = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", resolved.c_str(),
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        x, y, initEditW, 56,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INITIAL_TEXT_PREVIEW)),
        nullptr, nullptr);
    m_tabControls[3].push_back(hPreview);
}

void SettingsDialog::UpdateInitialTextPreview() {
    HWND hEdit = GetDlgItem(m_hwnd, IDC_INITIAL_TEXT_EDIT);
    HWND hPreview = GetDlgItem(m_hwnd, IDC_INITIAL_TEXT_PREVIEW);
    if (!hEdit || !hPreview) return;

    std::wstring tmpl;
    int len = GetWindowTextLengthW(hEdit);
    if (len > 0) {
        tmpl.resize(len);
        GetWindowTextW(hEdit, &tmpl[0], len + 1);
    }
    int cursorPos = -1;
    std::wstring resolved = ExpandInitialText(tmpl, cursorPos);
    SetWindowTextW(hPreview, resolved.c_str());
}

// ============================================================================
// Tab 5: Note List
// ============================================================================

void SettingsDialog::CreateNoteListTab(HWND hwnd) {
    // GetTabDisplayRect already returns parent-client coords; an extra
    // MapWindowPoints(m_hTab, hwnd, ...) added the tab offset a second time and
    // pushed every control down-right. Use the rect as-is, with the same +10/+8
    // insets as the other tabs.
    RECT dr = GetTabDisplayRect(m_hTab);

    int x = dr.left + 10;
    int y = dr.top + 8;
    int labelW = 110;
    int comboW = 120;
    int rowH = 26;

    // Date format label
    HWND hLabel = CreateWindowExW(0, L"STATIC",
        Ls(L"settings.date_format").c_str(),
        WS_CHILD | SS_LEFT,
        x, y + 2, labelW, 18,
        hwnd, nullptr, nullptr, nullptr);
    m_tabControls[4].push_back(hLabel);

    // Date format combo
    HWND hCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        x + labelW + 8, y, comboW, 200,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DATE_FORMAT)),
        nullptr, nullptr);
    SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2026-04-15 09:15"));
    SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"15.04.2026 09:15"));
    SendMessageW(hCombo, CB_SETCURSEL, m_data.dateFormat, 0);
    m_tabControls[4].push_back(hCombo);
    y += rowH + 8;

    // Zebra striping checkbox
    HWND hCheck = CreateWindowExW(0, L"BUTTON",
        Ls(L"settings.zebra_striping").c_str(),
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
        x, y, 300, 18,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ZEBRA_STRIPING)),
        nullptr, nullptr);
    SendMessageW(hCheck, BM_SETCHECK, m_data.zebraStriping ? BST_CHECKED : BST_UNCHECKED, 0);
    m_tabControls[4].push_back(hCheck);
}

// ============================================================================
// Tab switching
// ============================================================================

void SettingsDialog::ShowTab(int index) {
    for (int t = 0; t < 5; ++t) {
        int show = (t == index) ? SW_SHOW : SW_HIDE;
        for (HWND h : m_tabControls[t]) {
            ShowWindow(h, show);
        }
    }
    m_currentTab = index;

    // Move focus onto the first interactive control of the page now shown so the
    // dialog is keyboard-operable from the moment a tab opens (Tab/Space/arrows).
    // The General tab nests its controls inside the scroll panel, so descend into
    // it. Static labels/swatches carry no WS_TABSTOP and are skipped.
    HWND first = nullptr;
    for (HWND h : m_tabControls[index]) {
        // For the General tab the only direct child is the scroll panel; pick its
        // first tabstop child instead.
        if (h == m_hGeneralPanel) {
            for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
                if ((GetWindowLongW(c, GWL_STYLE) & WS_TABSTOP) && IsWindowEnabled(c)) {
                    first = c;
                    break;
                }
            }
        } else if ((GetWindowLongW(h, GWL_STYLE) & WS_TABSTOP) && IsWindowEnabled(h)) {
            first = h;
        }
        if (first) break;
    }
    if (first) SetFocus(first);
}

// ============================================================================
// Color / Font chooser
// ============================================================================

void SettingsDialog::OnChooseColor(HWND hwnd, COLORREF& color, int swatchId) {
    CHOOSECOLORW cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = hwnd;
    cc.rgbResult    = color;
    cc.lpCustColors = s_customColors;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        color = cc.rgbResult;
        HWND hSwatch = GetDlgItem(hwnd, swatchId);
        if (hSwatch) InvalidateRect(hSwatch, nullptr, TRUE);
    }
}

void SettingsDialog::OnChooseFont(HWND hwnd) {
    LOGFONTW lf = {};
    wcsncpy_s(lf.lfFaceName, m_data.fontFace.c_str(), _TRUNCATE);

    HDC hdc = GetDC(hwnd);
    lf.lfHeight = -MulDiv(m_data.fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);

    lf.lfWeight = m_data.fontBold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = m_data.fontItalic ? TRUE : FALSE;

    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = hwnd;
    cf.lpLogFont   = &lf;
    cf.Flags       = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    cf.rgbColors   = m_data.textColor;

    if (ChooseFontW(&cf)) {
        m_data.fontFace   = lf.lfFaceName;
        m_data.fontSize   = cf.iPointSize / 10;
        m_data.fontBold   = (lf.lfWeight >= FW_BOLD);
        m_data.fontItalic = (lf.lfItalic != FALSE);
        m_data.textColor  = cf.rgbColors;
        UpdateFontDisplay();

        // Also update text color swatch
        HWND hSwatch = GetDlgItem(m_hwnd, IDC_TEXT_COLOR_SWATCH);
        if (hSwatch) InvalidateRect(hSwatch, nullptr, TRUE);
    }
}

void SettingsDialog::UpdateFontDisplay() {
    HWND hDisp = GetDlgItem(m_hwnd, IDC_FONT_DISPLAY);
    if (!hDisp) return;

    std::wstring display = m_data.fontFace + L", " + std::to_wstring(m_data.fontSize) + L"pt";
    if (m_data.fontBold) display += L" Bold";
    if (m_data.fontItalic) display += L" Italic";
    SetWindowTextW(hDisp, display.c_str());
}

void SettingsDialog::UpdatePreview() {
    HWND hPreview = GetDlgItem(m_hwnd, IDC_LAYOUT_PREVIEW);
    if (hPreview) InvalidateRect(hPreview, nullptr, TRUE);
}

// ============================================================================
// Shortcut handling
// ============================================================================

void SettingsDialog::OnShortcutSelChange() {
    HWND hList = GetDlgItem(m_hwnd, IDC_SHORTCUT_LIST);
    HWND hHotkey = GetDlgItem(m_hwnd, IDC_SHORTCUT_HOTKEY);
    if (!hList || !hHotkey) return;

    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel >= 0 && sel < SC_COUNT) {
        WORD hk = m_data.shortcuts[sel];
        SendMessageW(hHotkey, HKM_SETHOTKEY, hk, 0);
    }
}

void SettingsDialog::OnShortcutChange() {
    HWND hList = GetDlgItem(m_hwnd, IDC_SHORTCUT_LIST);
    HWND hHotkey = GetDlgItem(m_hwnd, IDC_SHORTCUT_HOTKEY);
    if (!hList || !hHotkey) return;

    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= SC_COUNT) return;

    WORD hk = static_cast<WORD>(SendMessageW(hHotkey, HKM_GETHOTKEY, 0, 0));
    if (hk == 0) return;

    // Global hotkeys are registered system-wide via RegisterHotKey. A binding
    // without Ctrl or Alt (e.g. a bare "N", or Shift+N) would swallow that key
    // for every application and persist across restarts — refuse it. Per-note
    // slots are unaffected (Del with no modifier is a valid per-note default).
    if (sel == SC_GLOBAL_NEWNOTE || sel == SC_GLOBAL_NOTELIST) {
        BYTE mods = HIBYTE(hk);
        if (!(mods & (HOTKEYF_CONTROL | HOTKEYF_ALT))) {
            MessageBoxW(m_hwnd, Ls(L"settings.sc_global_needmod").c_str(),
                        Ls(L"settings.title").c_str(), MB_OK | MB_ICONWARNING);
            return;
        }
    }

    m_data.shortcuts[sel] = hk;

    std::wstring keyStr = FormatShortcut(hk);
    ListView_SetItemText(hList, sel, 1, const_cast<wchar_t*>(keyStr.c_str()));
}

void SettingsDialog::OnShortcutDefault() {
    HWND hList = GetDlgItem(m_hwnd, IDC_SHORTCUT_LIST);
    HWND hHotkey = GetDlgItem(m_hwnd, IDC_SHORTCUT_HOTKEY);
    if (!hList || !hHotkey) return;

    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= SC_COUNT) return;

    WORD defHk = s_shortcutDefs[sel].defaultHotkey;
    m_data.shortcuts[sel] = defHk;
    SendMessageW(hHotkey, HKM_SETHOTKEY, defHk, 0);

    std::wstring keyStr = FormatShortcut(defHk);
    ListView_SetItemText(hList, sel, 1, const_cast<wchar_t*>(keyStr.c_str()));
}

// ============================================================================
// Combo population
// ============================================================================

void SettingsDialog::PopulateLanguageCombo() {
    HWND hCombo = GetDlgItem(m_hGeneralPanel ? m_hGeneralPanel : m_hwnd, IDC_LANGUAGE);
    if (!hCombo) return;

    m_langs = Localization::Get().GetAvailableLanguages();
    int selIdx = 0;
    for (size_t i = 0; i < m_langs.size(); ++i) {
        SendMessageW(hCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(m_langs[i].second.c_str()));
        if (m_langs[i].first == m_data.language)
            selIdx = static_cast<int>(i);
    }
    SendMessageW(hCombo, CB_SETCURSEL, selIdx, 0);
}

void SettingsDialog::PopulateFolderCombo() {
    HWND hCombo = GetDlgItem(m_hwnd, IDC_DEFAULT_FOLDER);
    if (!hCombo) return;

    SendMessageW(hCombo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(Ls(L"note.no_folder").c_str()));

    const auto& folders = Application::Get().GetFolders();
    int selIdx = 0;
    for (size_t i = 0; i < folders.size(); ++i) {
        SendMessageW(hCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(folders[i].c_str()));
        if (folders[i] == m_data.defaultFolder)
            selIdx = static_cast<int>(i) + 1;
    }
    SendMessageW(hCombo, CB_SETCURSEL, selIdx, 0);
}

// ============================================================================
// Read values from controls
// ============================================================================

void SettingsDialog::ReadFromControls() {
    // Tab 3: General (controls are children of the scroll panel)
    HWND gp = m_hGeneralPanel ? m_hGeneralPanel : m_hwnd;
    HWND hSpin;

    hSpin = GetDlgItem(gp, IDC_AUTOSAVE_SPIN);
    if (hSpin) m_data.autosaveInterval = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    HWND hCheck = GetDlgItem(gp, IDC_CONFIRM_DELETE);
    if (hCheck) m_data.confirmDelete = (SendMessageW(hCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    HWND hPreviewCheck = GetDlgItem(gp, IDC_PREVIEW_ENABLED);
    if (hPreviewCheck) m_data.previewEnabled = (SendMessageW(hPreviewCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    HWND hLinksCheck = GetDlgItem(gp, IDC_CLICKABLE_LINKS);
    if (hLinksCheck) m_data.clickableLinks = (SendMessageW(hLinksCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    HWND hNewNoteTopCheck = GetDlgItem(gp, IDC_NEWNOTE_ALWAYSONTOP);
    if (hNewNoteTopCheck) m_data.newNoteAlwaysOnTop = (SendMessageW(hNewNoteTopCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    hSpin = GetDlgItem(gp, IDC_PREVIEW_DELAY_SPIN);
    if (hSpin) m_data.previewDelay = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    HWND hCombo = GetDlgItem(gp, IDC_TRAY_DBLCLICK);
    if (hCombo) m_data.trayDoubleClick = static_cast<int>(SendMessageW(hCombo, CB_GETCURSEL, 0, 0));

    HWND hAutostart = GetDlgItem(gp, IDC_AUTOSTART_ENABLED);
    if (hAutostart) m_data.autostartEnabled = (SendMessageW(hAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);

    HWND hLang = GetDlgItem(gp, IDC_LANGUAGE);
    if (hLang) {
        int idx = static_cast<int>(SendMessageW(hLang, CB_GETCURSEL, 0, 0));
        if (idx >= 0 && idx < static_cast<int>(m_langs.size()))
            m_data.language = m_langs[idx].first;
    }

    // Tab 4: Misc
    hSpin = GetDlgItem(m_hwnd, IDC_NEWNOTE_X_SPIN);
    if (hSpin) m_data.newNoteX = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    hSpin = GetDlgItem(m_hwnd, IDC_NEWNOTE_Y_SPIN);
    if (hSpin) m_data.newNoteY = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    hSpin = GetDlgItem(m_hwnd, IDC_CASCADE_STEP_SPIN);
    if (hSpin) m_data.cascadeStep = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    hSpin = GetDlgItem(m_hwnd, IDC_CASCADE_RESET_SPIN);
    if (hSpin) m_data.cascadeReset = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    HWND hFolder = GetDlgItem(m_hwnd, IDC_DEFAULT_FOLDER);
    if (hFolder) {
        int idx = static_cast<int>(SendMessageW(hFolder, CB_GETCURSEL, 0, 0));
        if (idx <= 0) {
            m_data.defaultFolder.clear();   // index 0 is the "(no folder)" entry
        } else {
            // Read the folder NAME straight from the combo, not folders[idx-1]:
            // a folder created while the dialog was open shifts GetFolders() and
            // would map the same index onto a different name.
            int len = static_cast<int>(SendMessageW(hFolder, CB_GETLBTEXTLEN, idx, 0));
            if (len > 0) {
                std::wstring name(len, L'\0');
                SendMessageW(hFolder, CB_GETLBTEXT, idx, reinterpret_cast<LPARAM>(&name[0]));
                m_data.defaultFolder = name;
            }
        }
    }

    // Initial text
    HWND hInitText = GetDlgItem(m_hwnd, IDC_INITIAL_TEXT_EDIT);
    if (hInitText) {
        int len = GetWindowTextLengthW(hInitText);
        if (len > 0) {
            std::wstring buf(len + 1, L'\0');
            GetWindowTextW(hInitText, &buf[0], len + 1);
            buf.resize(len);
            m_data.initialText = buf;
        } else {
            m_data.initialText.clear();
        }
    }

    // Tab 5: Note List
    HWND hDateFmt = GetDlgItem(m_hwnd, IDC_DATE_FORMAT);
    if (hDateFmt) m_data.dateFormat = static_cast<int>(SendMessageW(hDateFmt, CB_GETCURSEL, 0, 0));

    HWND hZebra = GetDlgItem(m_hwnd, IDC_ZEBRA_STRIPING);
    if (hZebra) m_data.zebraStriping = (SendMessageW(hZebra, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

// ============================================================================
// Insert variable popup menu
// ============================================================================

void SettingsDialog::ShowInsertVariableMenu() {
    struct VarEntry {
        const wchar_t* code;       // Display code (left prefix)
        const wchar_t* format;     // strftime format for preview (nullptr = literal)
        const wchar_t* literal;    // Literal preview text (when format is nullptr)
        const wchar_t* locKey;     // Localization key for description
        bool sepBefore;            // Separator before this item
    };

    static const VarEntry s_vars[] = {
        { L"%c",   L"%c",   nullptr, L"settings.var_datetime_short", false },
        { L"%x",   L"%x",   nullptr, L"settings.var_date_short",     false },
        { L"%X",   L"%X",   nullptr, L"settings.var_time",           false },
        { L"%#c",  L"%#c",  nullptr, L"settings.var_datetime_long",  true  },
        { L"%#x",  L"%#x",  nullptr, L"settings.var_date_long",      false },
        { L"%d",   L"%d",   nullptr, L"settings.var_day",            true  },
        { L"%m",   L"%m",   nullptr, L"settings.var_month",          false },
        { L"%y",   L"%y",   nullptr, L"settings.var_year_short",     false },
        { L"%Y",   L"%Y",   nullptr, L"settings.var_year_long",      false },
        { L"%H",   L"%H",   nullptr, L"settings.var_hour24",         true  },
        { L"%I",   L"%I",   nullptr, L"settings.var_hour12",         false },
        { L"%M",   L"%M",   nullptr, L"settings.var_minute",         false },
        { L"%S",   L"%S",   nullptr, L"settings.var_second",         false },
        { L"%p",   L"%p",   nullptr, L"settings.var_ampm",           false },
        { L"%a",   L"%a",   nullptr, L"settings.var_weekday_short",  true  },
        { L"%A",   L"%A",   nullptr, L"settings.var_weekday_long",   false },
        { L"%b",   L"%b",   nullptr, L"settings.var_month_short",    false },
        { L"%B",   L"%B",   nullptr, L"settings.var_month_long",     false },
        { L"%%",   nullptr, L"%",    L"settings.var_percent",        true  },
        { L"%%p",  nullptr, L"%%p",  L"settings.var_cursor",         false },
    };

    // Get current time for live preview
    std::time_t now = std::time(nullptr);
    struct tm localTime;
    localtime_s(&localTime, &now);

    HMENU hMenu = CreatePopupMenu();
    for (int i = 0; i < _countof(s_vars); ++i) {
        if (s_vars[i].sepBefore)
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        // Build preview string
        std::wstring preview;
        if (s_vars[i].format) {
            wchar_t buf[128];
            size_t len = wcsftime(buf, 128, s_vars[i].format, &localTime);
            if (len > 0) preview.assign(buf, len);
        } else {
            preview = s_vars[i].literal;
        }

        // Build menu text: "%c  Datum/Zeit kurz:\tPreview"
        std::wstring text = s_vars[i].code;
        text += L"  ";
        text += Ls(s_vars[i].locKey);
        text += L"\t";
        text += preview;

        AppendMenuW(hMenu, MF_STRING, ID_INITTEXT_BASE + i, text.c_str());
    }

    // Position menu below the insert button
    HWND hBtn = GetDlgItem(m_hwnd, IDC_INITIAL_TEXT_INSERT);
    RECT rc;
    GetWindowRect(hBtn, &rc);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

// ============================================================================
// Rebuild all controls after language change
// ============================================================================

void SettingsDialog::RebuildControls() {
    // Defensive: with the A4 deferral the dialog is no longer destroyed mid-apply,
    // but never operate on a dead HWND — that would leak the rebuilt icons/fonts.
    if (!IsWindow(m_hwnd)) return;

    // Remember the active tab before OnInitDialog's trailing ShowTab(0) clobbers
    // m_currentTab — otherwise a language change always snaps the user back to the
    // first (Layout) tab even if they triggered it from another page.
    int savedTab = m_currentTab;

    // Destroy all tab-page controls
    for (int t = 0; t < 5; ++t) {
        for (HWND h : m_tabControls[t]) {
            DestroyWindow(h);
        }
        m_tabControls[t].clear();
    }

    // Destroy tab control
    if (m_hTab) {
        DestroyWindow(m_hTab);
        m_hTab = nullptr;
    }

    // Destroy OK/Cancel/Apply buttons
    HWND hOk = GetDlgItem(m_hwnd, IDC_SETTINGS_OK);
    HWND hCancel = GetDlgItem(m_hwnd, IDC_SETTINGS_CANCEL);
    HWND hApply = GetDlgItem(m_hwnd, IDC_SETTINGS_APPLY);
    if (hOk) DestroyWindow(hOk);
    if (hCancel) DestroyWindow(hCancel);
    if (hApply) DestroyWindow(hApply);

    // Rebuild everything with new language strings
    OnInitDialog(m_hwnd);

    // Restore the tab the user was on (savedTab, captured before OnInitDialog)
    TabCtrl_SetCurSel(m_hTab, savedTab);
    ShowTab(savedTab);
}
