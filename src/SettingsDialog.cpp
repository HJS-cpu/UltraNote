#include "SettingsDialog.h"
#include "Application.h"
#include "Storage.h"
#include "Localization.h"
#include "Utils.h"
#include "Resource.h"
#include <commctrl.h>

COLORREF SettingsDialog::s_customColors[16] = {};

// Default shortcut definitions
static const ShortcutDef s_shortcutDefs[SC_COUNT] = {
    { SC_EDIT,           L"shortcut.edit",          L"settings.sc_edit",          MAKEWORD(VK_RETURN, 0) },
    { SC_DELETE,         L"shortcut.delete",        L"settings.sc_delete",        MAKEWORD(VK_DELETE, 0) },
    { SC_RENAME,         L"shortcut.rename",        L"settings.sc_rename",        MAKEWORD(VK_F2, 0) },
    { SC_ALWAYS_ON_TOP,  L"shortcut.ontop",         L"settings.sc_ontop",         MAKEWORD('O', 0) },
    { SC_HIDE,           L"shortcut.hide",          L"settings.sc_hide",          MAKEWORD('H', 0) },
    { SC_COPY,           L"shortcut.copy",          L"settings.sc_copy",          MAKEWORD('C', 0) },
    { SC_NEW_NOTE,       L"shortcut.newnote",       L"settings.sc_newnote",       MAKEWORD('N', 0) },
    { SC_GLOBAL_NEWNOTE, L"shortcut.global_new",    L"settings.sc_global_new",    MAKEWORD('N', HOTKEYF_CONTROL | HOTKEYF_SHIFT) },
    { SC_GLOBAL_NOTELIST,L"shortcut.global_list",   L"settings.sc_global_list",   MAKEWORD('L', HOTKEYF_CONTROL | HOTKEYF_SHIFT) },
};

const ShortcutDef* SettingsDialog::GetShortcutDefs() {
    return s_shortcutDefs;
}

// Helper to format a hotkey as display string
static std::wstring FormatHotkey(WORD hotkey) {
    BYTE vk = LOBYTE(hotkey);
    BYTE mods = HIBYTE(hotkey);
    std::wstring result;

    if (mods & HOTKEYF_CONTROL) result += L"Ctrl+";
    if (mods & HOTKEYF_SHIFT)   result += L"Shift+";
    if (mods & HOTKEYF_ALT)     result += L"Alt+";

    // Named keys
    switch (vk) {
        case VK_RETURN: result += L"Enter"; break;
        case VK_DELETE: result += L"Del"; break;
        case VK_ESCAPE: result += L"Esc"; break;
        case VK_SPACE:  result += L"Space"; break;
        case VK_TAB:    result += L"Tab"; break;
        case VK_F1: case VK_F2: case VK_F3: case VK_F4:
        case VK_F5: case VK_F6: case VK_F7: case VK_F8:
        case VK_F9: case VK_F10: case VK_F11: case VK_F12:
            result += L"F" + std::to_wstring(vk - VK_F1 + 1);
            break;
        default:
            if (vk >= 'A' && vk <= 'Z') {
                result += static_cast<wchar_t>(vk);
            } else if (vk >= '0' && vk <= '9') {
                result += static_cast<wchar_t>(vk);
            } else {
                result += L"?";
            }
            break;
    }
    return result;
}

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
    d.fontFace      = getStr(L"default.fontFace", L"Arial");
    d.fontSize      = getInt(L"default.fontSize", 10);
    d.fontBold      = getInt(L"default.fontBold", 0) != 0;
    d.fontItalic    = getInt(L"default.fontItalic", 0) != 0;

    d.autosaveInterval = getInt(L"autosave.interval", 30);
    d.confirmDelete    = getInt(L"confirm.delete", 1) != 0;
    d.previewEnabled   = getInt(L"notelist.preview", 0) != 0;
    d.previewDelay     = getInt(L"preview.delay", 400);
    d.trayDoubleClick  = getInt(L"tray.doubleclick", 0);
    d.language         = getStr(L"default.language", Localization::Get().GetCurrentLanguage().c_str());

    d.newNoteX       = getInt(L"newnote.x", 100);
    d.newNoteY       = getInt(L"newnote.y", 100);
    d.cascadeStep    = getInt(L"newnote.cascade", 20);
    d.cascadeReset   = getInt(L"newnote.cascade_reset", 500);
    d.defaultFolder  = getStr(L"newnote.folder", L"");

    for (int i = 0; i < SC_COUNT; ++i) {
        d.shortcuts[i] = static_cast<WORD>(getInt(s_shortcutDefs[i].settingsKey,
                                                   s_shortcutDefs[i].defaultHotkey));
    }

    return d;
}

void SettingsDialog::SaveToStorage(const SettingsData& data) {
    auto intS = Storage::LoadSettings();
    auto strS = Storage::LoadSettingsStr();

    intS[L"default.bgColor"]       = static_cast<int>(data.bgColor);
    intS[L"default.textColor"]     = static_cast<int>(data.textColor);
    intS[L"default.borderColor"]   = static_cast<int>(data.borderColor);
    strS[L"default.fontFace"]      = data.fontFace;
    intS[L"default.fontSize"]      = data.fontSize;
    intS[L"default.fontBold"]      = data.fontBold ? 1 : 0;
    intS[L"default.fontItalic"]    = data.fontItalic ? 1 : 0;

    intS[L"autosave.interval"]     = data.autosaveInterval;
    intS[L"confirm.delete"]        = data.confirmDelete ? 1 : 0;
    intS[L"notelist.preview"]      = data.previewEnabled ? 1 : 0;
    intS[L"preview.delay"]         = data.previewDelay;
    intS[L"tray.doubleclick"]      = data.trayDoubleClick;
    strS[L"default.language"]      = data.language;

    intS[L"newnote.x"]             = data.newNoteX;
    intS[L"newnote.y"]             = data.newNoteY;
    intS[L"newnote.cascade"]       = data.cascadeStep;
    intS[L"newnote.cascade_reset"] = data.cascadeReset;
    strS[L"newnote.folder"]        = data.defaultFolder;

    for (int i = 0; i < SC_COUNT; ++i) {
        intS[s_shortcutDefs[i].settingsKey] = data.shortcuts[i];
    }

    Storage::SaveAllSettings(intS, strS);
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
    dlg.tmpl.cy = 210;  // DLU

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
            switch (LOWORD(wParam)) {
                case IDC_SETTINGS_OK: {
                    self->ReadFromControls();
                    SaveToStorage(self->m_data);
                    Application::Get().ApplySettings();
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }

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
    }

    return FALSE;
}

// ============================================================================
// Dialog init
// ============================================================================

void SettingsDialog::OnInitDialog(HWND hwnd) {
    m_hwnd = hwnd;
    SetWindowTextW(hwnd, Ls(L"settings.title").c_str());

    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    // OK / Cancel / Apply buttons
    RECT rc;
    GetClientRect(hwnd, &rc);
    int btnW = 75, btnH = 23;
    int btnY = rc.bottom - btnH - 8;

    HWND hOk = CreateWindowExW(0, L"BUTTON", Ls(L"settings.ok").c_str(),
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        rc.right - 3 * (btnW + 6) - 4, btnY, btnW, btnH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_OK)),
        nullptr, nullptr);

    HWND hCancel = CreateWindowExW(0, L"BUTTON", Ls(L"settings.cancel").c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        rc.right - 2 * (btnW + 6) - 4, btnY, btnW, btnH,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_CANCEL)),
        nullptr, nullptr);

    HWND hApply = CreateWindowExW(0, L"BUTTON", Ls(L"settings.apply").c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
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

    // Apply font to all tab controls
    for (int t = 0; t < 4; ++t) {
        for (HWND h : m_tabControls[t]) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }
    }

    ShowTab(0);
}

void SettingsDialog::CreateTabs(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    m_hTab = CreateWindowExW(0, WC_TABCONTROL, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        4, 4, rc.right - 8, rc.bottom - 40,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SETTINGS_TAB)),
        nullptr, nullptr);

    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(m_hTab, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    const wchar_t* tabKeys[] = {
        L"settings.tab_layout", L"settings.tab_keyboard",
        L"settings.tab_general", L"settings.tab_misc"
    };
    for (int i = 0; i < 4; ++i) {
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
            WS_CHILD | BS_PUSHBUTTON,
            x + labelW + swatchW + 8, y, btnW, btnH,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(btnId)),
            nullptr, nullptr);
        m_tabControls[0].push_back(hBtn);

        y += rowH;
    };

    addColorRow(L"settings.bg_color", IDC_BG_COLOR_SWATCH, IDC_BG_COLOR_BTN);
    addColorRow(L"settings.text_color", IDC_TEXT_COLOR_SWATCH, IDC_TEXT_COLOR_BTN);
    addColorRow(L"settings.border_color", IDC_BORDER_COLOR_SWATCH, IDC_BORDER_COLOR_BTN);

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
        WS_CHILD | BS_PUSHBUTTON,
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
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
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

        std::wstring keyStr = FormatHotkey(m_data.shortcuts[i]);
        ListView_SetItemText(hList, i, 1, const_cast<wchar_t*>(keyStr.c_str()));
    }

    y += listH + 8;

    // Hotkey control + buttons
    HWND hHkLabel = CreateWindowExW(0, L"STATIC", Ls(L"settings.sc_newkey").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, 100, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[1].push_back(hHkLabel);

    HWND hHotkey = CreateWindowExW(0, HOTKEY_CLASS, L"",
        WS_CHILD | WS_BORDER,
        x + 100, y, 130, 23,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_HOTKEY)),
        nullptr, nullptr);
    m_tabControls[1].push_back(hHotkey);

    HWND hChangeBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.sc_apply").c_str(),
        WS_CHILD | BS_PUSHBUTTON,
        x + 238, y, 70, 23,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_CHANGE)),
        nullptr, nullptr);
    m_tabControls[1].push_back(hChangeBtn);

    HWND hDefBtn = CreateWindowExW(0, L"BUTTON", Ls(L"settings.sc_default").c_str(),
        WS_CHILD | BS_PUSHBUTTON,
        x + 314, y, 70, 23,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SHORTCUT_DEFAULT)),
        nullptr, nullptr);
    m_tabControls[1].push_back(hDefBtn);
}

// ============================================================================
// Tab 3: General
// ============================================================================

void SettingsDialog::CreateGeneralTab(HWND hwnd) {
    RECT tabRc = GetTabDisplayRect(m_hTab);
    int x = tabRc.left + 10;
    int y = tabRc.top + 8;
    int labelW = 160, editW = 60, rowH = 28;

    // Autosave interval
    HWND hLabel1 = CreateWindowExW(0, L"STATIC", Ls(L"settings.autosave").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(hLabel1);

    HWND hEdit1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_NUMBER | ES_RIGHT,
        x + labelW, y, editW, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSAVE_EDIT)),
        nullptr, nullptr);
    m_tabControls[2].push_back(hEdit1);

    HWND hSpin1 = CreateWindowExW(0, UPDOWN_CLASS, L"",
        WS_CHILD | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
        0, 0, 0, 0,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSAVE_SPIN)),
        nullptr, nullptr);
    SendMessageW(hSpin1, UDM_SETRANGE32, 10, 300);
    SendMessageW(hSpin1, UDM_SETPOS32, 0, m_data.autosaveInterval);
    m_tabControls[2].push_back(hSpin1);

    HWND hSec1 = CreateWindowExW(0, L"STATIC", Ls(L"settings.seconds").c_str(),
        WS_CHILD | SS_LEFT, x + labelW + editW + 20, y + 3, 40, 18,
        hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(hSec1);
    y += rowH;

    // Confirm delete
    HWND hCheck = CreateWindowExW(0, L"BUTTON", Ls(L"settings.confirm_delete").c_str(),
        WS_CHILD | BS_AUTOCHECKBOX,
        x, y, 300, 20,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CONFIRM_DELETE)),
        nullptr, nullptr);
    if (m_data.confirmDelete)
        SendMessageW(hCheck, BM_SETCHECK, BST_CHECKED, 0);
    m_tabControls[2].push_back(hCheck);
    y += rowH;

    // Preview enabled
    HWND hPreviewCheck = CreateWindowExW(0, L"BUTTON", Ls(L"settings.preview_enabled").c_str(),
        WS_CHILD | BS_AUTOCHECKBOX,
        x, y, 300, 20,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW_ENABLED)),
        nullptr, nullptr);
    if (m_data.previewEnabled)
        SendMessageW(hPreviewCheck, BM_SETCHECK, BST_CHECKED, 0);
    m_tabControls[2].push_back(hPreviewCheck);
    y += rowH;

    // Preview delay
    HWND hLabel2 = CreateWindowExW(0, L"STATIC", Ls(L"settings.preview_delay").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(hLabel2);

    HWND hEdit2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_NUMBER | ES_RIGHT,
        x + labelW, y, editW, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW_DELAY_EDIT)),
        nullptr, nullptr);
    m_tabControls[2].push_back(hEdit2);

    HWND hSpin2 = CreateWindowExW(0, UPDOWN_CLASS, L"",
        WS_CHILD | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
        0, 0, 0, 0,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW_DELAY_SPIN)),
        nullptr, nullptr);
    SendMessageW(hSpin2, UDM_SETRANGE32, 100, 2000);
    SendMessageW(hSpin2, UDM_SETPOS32, 0, m_data.previewDelay);
    m_tabControls[2].push_back(hSpin2);

    HWND hMs = CreateWindowExW(0, L"STATIC", L"ms",
        WS_CHILD | SS_LEFT, x + labelW + editW + 20, y + 3, 30, 18,
        hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(hMs);
    y += rowH;

    // Tray double-click action
    HWND hLabel3 = CreateWindowExW(0, L"STATIC", Ls(L"settings.tray_dblclick").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(hLabel3);

    HWND hCombo1 = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST,
        x + labelW, y, 160, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TRAY_DBLCLICK)),
        nullptr, nullptr);
    SendMessageW(hCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Ls(L"settings.tray_newnote").c_str()));
    SendMessageW(hCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Ls(L"settings.tray_notelist").c_str()));
    SendMessageW(hCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Ls(L"settings.tray_showall").c_str()));
    SendMessageW(hCombo1, CB_SETCURSEL, m_data.trayDoubleClick, 0);
    m_tabControls[2].push_back(hCombo1);
    y += rowH;

    // Language
    HWND hLabel4 = CreateWindowExW(0, L"STATIC", Ls(L"settings.language").c_str(),
        WS_CHILD | SS_LEFT, x, y + 3, labelW, 18, hwnd, nullptr, nullptr, nullptr);
    m_tabControls[2].push_back(hLabel4);

    HWND hLangCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST,
        x + labelW, y, 160, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LANGUAGE)),
        nullptr, nullptr);
    m_tabControls[2].push_back(hLangCombo);

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
            WS_CHILD | ES_NUMBER | ES_RIGHT,
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
        WS_CHILD | CBS_DROPDOWNLIST,
        x + labelW, y, 160, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEFAULT_FOLDER)),
        nullptr, nullptr);
    m_tabControls[3].push_back(hCombo);

    PopulateFolderCombo();
}

// ============================================================================
// Tab switching
// ============================================================================

void SettingsDialog::ShowTab(int index) {
    for (int t = 0; t < 4; ++t) {
        int show = (t == index) ? SW_SHOW : SW_HIDE;
        for (HWND h : m_tabControls[t]) {
            ShowWindow(h, show);
        }
    }
    m_currentTab = index;
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
    wcscpy_s(lf.lfFaceName, m_data.fontFace.c_str());

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

    m_data.shortcuts[sel] = hk;

    std::wstring keyStr = FormatHotkey(hk);
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

    std::wstring keyStr = FormatHotkey(defHk);
    ListView_SetItemText(hList, sel, 1, const_cast<wchar_t*>(keyStr.c_str()));
}

// ============================================================================
// Combo population
// ============================================================================

void SettingsDialog::PopulateLanguageCombo() {
    HWND hCombo = GetDlgItem(m_hwnd, IDC_LANGUAGE);
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
    // Tab 3: General
    HWND hSpin;

    hSpin = GetDlgItem(m_hwnd, IDC_AUTOSAVE_SPIN);
    if (hSpin) m_data.autosaveInterval = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    HWND hCheck = GetDlgItem(m_hwnd, IDC_CONFIRM_DELETE);
    if (hCheck) m_data.confirmDelete = (SendMessageW(hCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    HWND hPreviewCheck = GetDlgItem(m_hwnd, IDC_PREVIEW_ENABLED);
    if (hPreviewCheck) m_data.previewEnabled = (SendMessageW(hPreviewCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    hSpin = GetDlgItem(m_hwnd, IDC_PREVIEW_DELAY_SPIN);
    if (hSpin) m_data.previewDelay = static_cast<int>(SendMessageW(hSpin, UDM_GETPOS32, 0, 0));

    HWND hCombo = GetDlgItem(m_hwnd, IDC_TRAY_DBLCLICK);
    if (hCombo) m_data.trayDoubleClick = static_cast<int>(SendMessageW(hCombo, CB_GETCURSEL, 0, 0));

    HWND hLang = GetDlgItem(m_hwnd, IDC_LANGUAGE);
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
            m_data.defaultFolder.clear();
        } else {
            const auto& folders = Application::Get().GetFolders();
            if (idx - 1 < static_cast<int>(folders.size()))
                m_data.defaultFolder = folders[idx - 1];
        }
    }
}

// ============================================================================
// Rebuild all controls after language change
// ============================================================================

void SettingsDialog::RebuildControls() {
    // Destroy all tab-page controls
    for (int t = 0; t < 4; ++t) {
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

    // Restore current tab
    TabCtrl_SetCurSel(m_hTab, m_currentTab);
    ShowTab(m_currentTab);
}
