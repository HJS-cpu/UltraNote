#pragma once

#include <windows.h>
#include <string>
#include <cstdint>

// Note layout: all visual properties
struct NoteLayout {
    COLORREF backgroundColor = RGB(255, 255, 153);  // Classic sticky note yellow
    COLORREF textColor       = RGB(0, 0, 0);        // Black
    COLORREF borderColor     = RGB(200, 200, 80);   // Darker yellow
    std::wstring fontFace    = L"Arial";
    int      fontSizePts     = 10;
    bool     fontBold        = false;
    bool     fontItalic      = false;
    bool     alwaysOnTop     = false;
};

// Complete note data - everything needed to save/restore a note
struct NoteData {
    uint64_t     id          = 0;
    std::wstring text;
    std::wstring title;           // Empty = use first line of text as fallback
    std::wstring folder;          // Empty = no folder
    int          x           = 100;
    int          y           = 100;
    int          width       = 200;
    int          height      = 150;
    bool         isMinimized = false;
    bool         isHidden    = false;
    NoteLayout   layout;
    int64_t      createdAt   = 0;   // Unix timestamp
    int64_t      modifiedAt  = 0;   // Unix timestamp
};
