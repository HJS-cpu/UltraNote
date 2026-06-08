#include "AlarmScheduler.h"
#include "Localization.h"
#include <algorithm>

namespace {

int DaysInMonth(int year, int month) {
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 30;
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

// Build a date at given Y/M/D using the time-of-day from timeTemplate.
// wDayOfWeek is populated via a FILETIME round-trip.
SYSTEMTIME MakeDate(int year, int month, int day, const SYSTEMTIME& timeTemplate) {
    SYSTEMTIME st = {};
    st.wYear   = static_cast<WORD>(year);
    st.wMonth  = static_cast<WORD>(month);
    st.wDay    = static_cast<WORD>(day);
    st.wHour   = timeTemplate.wHour;
    st.wMinute = timeTemplate.wMinute;
    st.wSecond = 0;
    FILETIME ft;
    if (SystemTimeToFileTime(&st, &ft)) {
        FileTimeToSystemTime(&ft, &st);
    }
    return st;
}

// Add `days` to a SYSTEMTIME (arithmetic via FILETIME), preserving exact time-of-day.
SYSTEMTIME AddDays(const SYSTEMTIME& st, int days) {
    FILETIME ft;
    SYSTEMTIME tmp = st;
    if (!SystemTimeToFileTime(&tmp, &ft)) return st;
    ULARGE_INTEGER u;
    u.LowPart  = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart += static_cast<uint64_t>(days) * 86400ULL * 10000000ULL;
    ft.dwLowDateTime  = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME out;
    FileTimeToSystemTime(&ft, &out);
    out.wHour   = st.wHour;
    out.wMinute = st.wMinute;
    out.wSecond = 0;
    return out;
}

// Find the Nth occurrence of weekday in given year/month (nth in 1..5, 5 = "5th or last").
int NthWeekdayDay(int year, int month, int nth, int weekday) {
    SYSTEMTIME first = {};
    first.wYear  = static_cast<WORD>(year);
    first.wMonth = static_cast<WORD>(month);
    first.wDay   = 1;
    FILETIME ft;
    if (!SystemTimeToFileTime(&first, &ft)) return 1;
    FileTimeToSystemTime(&ft, &first);
    int firstWd = first.wDayOfWeek;
    int offset = (weekday - firstWd + 7) % 7;
    int firstOccurrence = 1 + offset;
    int target = firstOccurrence + (nth - 1) * 7;
    int dim = DaysInMonth(year, month);
    if (target > dim) {
        // "5th or last" semantic: fall back to last occurrence of that weekday
        target = firstOccurrence + ((dim - firstOccurrence) / 7) * 7;
    }
    return target;
}

int QuarterStartMonth(int month) {
    return ((month - 1) / 3) * 3 + 1;
}

std::wstring FormatInt(const std::wstring& templateStr, int value) {
    // Replace the first %d with value (manual because swprintf with a wstring template is awkward)
    size_t pos = templateStr.find(L"%d");
    if (pos == std::wstring::npos) return templateStr;
    std::wstring result = templateStr;
    result.replace(pos, 2, std::to_wstring(value));
    return result;
}

std::wstring FormatStr(const std::wstring& templateStr, const std::wstring& value) {
    size_t pos = templateStr.find(L"%s");
    if (pos == std::wstring::npos) return templateStr;
    std::wstring result = templateStr;
    result.replace(pos, 2, value);
    return result;
}

std::wstring FormatIntStr(const std::wstring& templateStr, int intVal, const std::wstring& strVal) {
    std::wstring result = templateStr;
    size_t p1 = result.find(L"%d");
    if (p1 != std::wstring::npos) result.replace(p1, 2, std::to_wstring(intVal));
    size_t p2 = result.find(L"%s");
    if (p2 != std::wstring::npos) result.replace(p2, 2, strVal);
    return result;
}

// Weekday index (0=Sun..6=Sat) -> short name key
const wchar_t* WeekdayShortKey(int wd) {
    switch (wd) {
        case 0: return L"alarm.wd.sun";
        case 1: return L"alarm.wd.mon";
        case 2: return L"alarm.wd.tue";
        case 3: return L"alarm.wd.wed";
        case 4: return L"alarm.wd.thu";
        case 5: return L"alarm.wd.fri";
        case 6: return L"alarm.wd.sat";
    }
    return L"alarm.wd.sun";
}

std::wstring JoinWeekdays(uint8_t mask) {
    // Display order: Mon first (European convention)
    static const int order[] = { 1, 2, 3, 4, 5, 6, 0 };
    std::wstring result;
    for (int wd : order) {
        if (mask & (1u << wd)) {
            if (!result.empty()) result += L", ";
            result += Ls(WeekdayShortKey(wd));
        }
    }
    return result;
}

} // namespace

// ============================================================================
// Public API
// ============================================================================

int AlarmScheduler::CompareSysTime(const SYSTEMTIME& a, const SYSTEMTIME& b) {
    if (a.wYear   != b.wYear)   return a.wYear   < b.wYear   ? -1 : 1;
    if (a.wMonth  != b.wMonth)  return a.wMonth  < b.wMonth  ? -1 : 1;
    if (a.wDay    != b.wDay)    return a.wDay    < b.wDay    ? -1 : 1;
    if (a.wHour   != b.wHour)   return a.wHour   < b.wHour   ? -1 : 1;
    if (a.wMinute != b.wMinute) return a.wMinute < b.wMinute ? -1 : 1;
    if (a.wSecond != b.wSecond) return a.wSecond < b.wSecond ? -1 : 1;
    return 0;
}

int64_t AlarmScheduler::SysTimeToNaiveSec(const SYSTEMTIME& st) {
    FILETIME ft;
    SYSTEMTIME tmp = st;
    if (!SystemTimeToFileTime(&tmp, &ft)) return 0;
    ULARGE_INTEGER u;
    u.LowPart  = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    // FILETIME epoch is 1601-01-01 UTC (100ns units). Convert to seconds since 1970.
    // Diff 1601->1970 in seconds: 11644473600
    return static_cast<int64_t>(u.QuadPart / 10000000ULL) - 11644473600LL;
}

SYSTEMTIME AlarmScheduler::NaiveSecToSysTime(int64_t sec) {
    uint64_t ticks = (static_cast<uint64_t>(sec) + 11644473600ULL) * 10000000ULL;
    FILETIME ft;
    ft.dwLowDateTime  = static_cast<DWORD>(ticks & 0xFFFFFFFFULL);
    ft.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
    SYSTEMTIME st = {};
    FileTimeToSystemTime(&ft, &st);
    return st;
}

void AlarmScheduler::AdvanceAfterFire(AlarmConfig& alarm) {
    alarm.firedCount++;
    alarm.snoozeUntil = 0;
}

void AlarmScheduler::AdvanceAfterSnooze(AlarmConfig& alarm, const SYSTEMTIME& now) {
    int mins = (std::max)(1, alarm.snoozeMinutes);
    alarm.snoozeUntil = SysTimeToNaiveSec(now) + static_cast<int64_t>(mins) * 60;
}

std::optional<SYSTEMTIME> AlarmScheduler::ComputeNextFireTime(const AlarmConfig& alarm,
                                                              const SYSTEMTIME& now) {
    if (alarm.paused) return std::nullopt;

    // End condition: fired enough times (applies to all kinds including Once)
    if (alarm.endKind == AlarmEndKind::AfterN && alarm.firedCount >= alarm.endCount) {
        return std::nullopt;
    }

    // Snooze: if active and still in future, the snooze time IS the next fire
    if (alarm.snoozeUntil > 0) {
        int64_t nowSec = SysTimeToNaiveSec(now);
        if (alarm.snoozeUntil > nowSec) {
            return NaiveSecToSysTime(alarm.snoozeUntil);
        }
        // Snooze expired — fall through and compute normal next fire.
    }

    // Fire contract: return the earliest valid occurrence >= the current MINUTE
    // (now with seconds zeroed). Candidates already carry wSecond=0, so an
    // occurrence in the current minute is returned rather than skipped, letting
    // CheckDueAlarms (which fires on nextFire <= now) actually trigger it.
    SYSTEMTIME nowFloor = now;
    nowFloor.wSecond = 0;

    SYSTEMTIME candidate = {};

    switch (alarm.kind) {
    case AlarmKind::Once: {
        if (alarm.firedCount > 0) return std::nullopt;
        candidate = alarm.startTime;
        break;
    }

    case AlarmKind::Daily: {
        candidate = (CompareSysTime(alarm.startTime, nowFloor) > 0) ? alarm.startTime : nowFloor;
        candidate.wHour   = alarm.startTime.wHour;
        candidate.wMinute = alarm.startTime.wMinute;
        candidate.wSecond = 0;
        while (CompareSysTime(candidate, nowFloor) < 0 ||
               CompareSysTime(candidate, alarm.startTime) < 0) {
            candidate = AddDays(candidate, 1);
        }
        break;
    }

    case AlarmKind::EveryNDays: {
        int interval = (std::max)(1, alarm.intervalDays);
        // Direct jump instead of stepping one interval at a time: a far-past
        // startTime would otherwise loop hundreds/thousands of times per tick.
        SYSTEMTIME anchor = alarm.startTime;
        anchor.wSecond = 0;
        int64_t gapSec = SysTimeToNaiveSec(nowFloor) - SysTimeToNaiveSec(anchor);
        if (gapSec <= 0) {
            candidate = anchor;  // start is now or in the future
        } else {
            int64_t intervalSec = static_cast<int64_t>(interval) * 86400;
            int64_t steps = (gapSec + intervalSec - 1) / intervalSec;  // ceil
            candidate = AddDays(anchor, static_cast<int>(steps * interval));
        }
        break;
    }

    case AlarmKind::Weekly: {
        if (alarm.weekdayMask == 0) return std::nullopt;
        candidate = (CompareSysTime(alarm.startTime, nowFloor) > 0) ? alarm.startTime : nowFloor;
        candidate.wHour   = alarm.startTime.wHour;
        candidate.wMinute = alarm.startTime.wMinute;
        candidate.wSecond = 0;
        // If today's time is before the current minute, start search from tomorrow
        if (CompareSysTime(candidate, nowFloor) < 0) {
            candidate = AddDays(candidate, 1);
        }
        for (int i = 0; i < 7; ++i) {
            if (alarm.weekdayMask & (1u << candidate.wDayOfWeek)) {
                if (CompareSysTime(candidate, alarm.startTime) >= 0) break;
            }
            candidate = AddDays(candidate, 1);
        }
        if (!(alarm.weekdayMask & (1u << candidate.wDayOfWeek))) return std::nullopt;
        break;
    }

    case AlarmKind::MonthlyDay: {
        int day = (std::clamp)(alarm.monthDay, 1, 31);
        int y = now.wYear, m = now.wMonth;
        // Start from alarm.startTime's month if it's in the future
        if (CompareSysTime(alarm.startTime, now) > 0) {
            y = alarm.startTime.wYear;
            m = alarm.startTime.wMonth;
        }
        for (int i = 0; i < 24; ++i) {
            int effectiveDay = (std::min)(day, DaysInMonth(y, m));
            candidate = MakeDate(y, m, effectiveDay, alarm.startTime);
            if (CompareSysTime(candidate, nowFloor) >= 0 &&
                CompareSysTime(candidate, alarm.startTime) >= 0) {
                break;
            }
            if (++m > 12) { m = 1; ++y; }
        }
        break;
    }

    case AlarmKind::MonthlyNth: {
        int nth = (std::clamp)(alarm.nthWeek, 1, 5);
        int wd  = (std::clamp)(alarm.nthWeekday, 0, 6);
        int y = now.wYear, m = now.wMonth;
        if (CompareSysTime(alarm.startTime, now) > 0) {
            y = alarm.startTime.wYear;
            m = alarm.startTime.wMonth;
        }
        for (int i = 0; i < 24; ++i) {
            int day = NthWeekdayDay(y, m, nth, wd);
            candidate = MakeDate(y, m, day, alarm.startTime);
            if (CompareSysTime(candidate, nowFloor) >= 0 &&
                CompareSysTime(candidate, alarm.startTime) >= 0) {
                break;
            }
            if (++m > 12) { m = 1; ++y; }
        }
        break;
    }

    case AlarmKind::Quarterly: {
        int qd = (std::clamp)(alarm.quarterDay, 1, 90);
        int nowQStart = QuarterStartMonth(now.wMonth);
        int stQStart  = QuarterStartMonth(alarm.startTime.wMonth);
        // Global quarter index (year*4 + 0..3) for ordering across year boundaries.
        int stQIdx  = alarm.startTime.wYear * 4 + (stQStart  - 1) / 3;
        int nowQIdx = now.wYear             * 4 + (nowQStart - 1) / 3;
        int y;
        int qStart;
        if (stQIdx > nowQIdx) {
            // startTime is in a strictly future quarter — search from there.
            y = alarm.startTime.wYear;
            qStart = stQStart;
        } else {
            // startTime falls in the current (or a past) quarter — never fire
            // retroactively in the same quarter; skip to the quarter after now's.
            y = now.wYear;
            qStart = nowQStart + 3;
            if (qStart > 12) { qStart -= 12; ++y; }
        }
        for (int i = 0; i < 8; ++i) {
            SYSTEMTIME qFirst = MakeDate(y, qStart, 1, alarm.startTime);
            candidate = AddDays(qFirst, qd - 1);
            if (CompareSysTime(candidate, nowFloor) >= 0 &&
                CompareSysTime(candidate, alarm.startTime) >= 0) {
                break;
            }
            qStart += 3;
            if (qStart > 12) { qStart -= 12; ++y; }
        }
        break;
    }

    case AlarmKind::Yearly: {
        int y  = now.wYear;
        int mo = alarm.startTime.wMonth;
        int d  = alarm.startTime.wDay;
        if (CompareSysTime(alarm.startTime, now) > 0) {
            y = alarm.startTime.wYear;
        }
        for (int i = 0; i < 4; ++i) {
            int effectiveDay = (std::min)(d, DaysInMonth(y, mo));
            candidate = MakeDate(y, mo, effectiveDay, alarm.startTime);
            if (CompareSysTime(candidate, nowFloor) >= 0 &&
                CompareSysTime(candidate, alarm.startTime) >= 0) {
                break;
            }
            ++y;
        }
        break;
    }
    }

    // End condition: after specific date (compare whole calendar day)
    if (alarm.endKind == AlarmEndKind::OnDate) {
        SYSTEMTIME endLimit = alarm.endDate;
        endLimit.wHour = 23;
        endLimit.wMinute = 59;
        endLimit.wSecond = 59;
        if (CompareSysTime(candidate, endLimit) > 0) {
            return std::nullopt;
        }
    }

    return candidate;
}

std::wstring AlarmScheduler::DescribeInterval(const AlarmConfig& alarm) {
    switch (alarm.kind) {
    case AlarmKind::Once:
        return Ls(L"alarm.desc.once");

    case AlarmKind::Daily:
        return Ls(L"alarm.desc.daily");

    case AlarmKind::EveryNDays:
        return FormatInt(Ls(L"alarm.desc.every_n_days"), (std::max)(1, alarm.intervalDays));

    case AlarmKind::Weekly: {
        std::wstring days = JoinWeekdays(alarm.weekdayMask);
        if (days.empty()) return Ls(L"alarm.desc.weekly_none");
        return FormatStr(Ls(L"alarm.desc.weekly"), days);
    }

    case AlarmKind::MonthlyDay:
        return FormatInt(Ls(L"alarm.desc.monthly_day"), (std::clamp)(alarm.monthDay, 1, 31));

    case AlarmKind::MonthlyNth: {
        int nth = (std::clamp)(alarm.nthWeek, 1, 5);
        std::wstring wdName = Ls(WeekdayShortKey((std::clamp)(alarm.nthWeekday, 0, 6)));
        return FormatIntStr(Ls(L"alarm.desc.monthly_nth"), nth, wdName);
    }

    case AlarmKind::Quarterly:
        return FormatInt(Ls(L"alarm.desc.quarterly"), (std::clamp)(alarm.quarterDay, 1, 90));

    case AlarmKind::Yearly: {
        wchar_t buf[16];
        swprintf_s(buf, L"%02u.%02u.", alarm.startTime.wDay, alarm.startTime.wMonth);
        return FormatStr(Ls(L"alarm.desc.yearly"), buf);
    }
    }
    return L"";
}
