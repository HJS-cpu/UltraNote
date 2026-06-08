#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <optional>
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

static constexpr int MAX_ATTACHMENTS = 5;

// Alarm repetition type
enum class AlarmKind {
    Once       = 0,   // Once at startTime
    Daily      = 1,   // Every day at startTime time-of-day
    EveryNDays = 2,   // Every intervalDays days
    Weekly     = 3,   // On weekdayMask days of the week
    MonthlyDay = 4,   // On the monthDay-th calendar day
    MonthlyNth = 5,   // On the nthWeek-th nthWeekday of the month
    Quarterly  = 6,   // On the quarterDay-th day of each quarter
    Yearly     = 7    // Yearly on startTime's month/day
};

enum class AlarmEndKind {
    Never   = 0,
    AfterN  = 1,
    OnDate  = 2
};

// Weekday convention for weekdayMask and nthWeekday: bit/value 0 = Sunday .. 6 = Saturday
// (matches SYSTEMTIME::wDayOfWeek — no conversion needed at comparison time)
struct AlarmConfig {
    // Local wall-clock time. For recurring alarms, only time-of-day (+ month/day/year for anchoring) matter.
    SYSTEMTIME startTime = {};
    AlarmKind  kind = AlarmKind::Once;

    int        intervalDays = 1;    // EveryNDays: 1..365
    uint8_t    weekdayMask = 0;     // Weekly: bitmask
    int        monthDay = 1;        // MonthlyDay: 1..31 (clamped to last day if month is shorter)
    int        nthWeek = 1;         // MonthlyNth: 1..5 (5 = last occurrence in month)
    int        nthWeekday = 0;      // MonthlyNth: 0=Sun..6=Sat
    int        quarterDay = 1;      // Quarterly: 1..90

    AlarmEndKind endKind = AlarmEndKind::Never;
    int        endCount = 10;       // AfterN
    SYSTEMTIME endDate = {};        // OnDate (date portion only)

    bool       popup = true;
    bool       sound = true;
    std::wstring soundFile;         // Empty = MessageBeep fallback

    int        snoozeMinutes = 5;

    // Runtime state (persisted to survive app restart)
    int        firedCount = 0;
    int64_t    snoozeUntil = 0;     // 0 = not snoozed; else naive-local seconds since epoch
                                    // (local wall-clock treated as UTC; see AlarmScheduler::SysTimeToNaiveSec)
    bool       paused = false;
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
    std::vector<std::wstring> attachments;   // File paths (links, max MAX_ATTACHMENTS)
    bool         showAttachments = false;     // Attachment bar visible
    std::optional<AlarmConfig> alarm;         // No alarm set if nullopt
    int          cursorPos      = -1;        // Transient: cursor position from %%p marker (-1 = not set)
};
