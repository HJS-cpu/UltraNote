#pragma once

#include "Note.h"
#include <windows.h>
#include <optional>
#include <string>

// Pure scheduling utility: no state, no side effects.
// All SYSTEMTIMEs are treated as local wall-clock (no timezone).
// Internal arithmetic goes through FILETIME (naive, no DST conversion) —
// this keeps recurring times stable across DST boundaries: "daily at 08:00"
// stays 08:00 local year-round, even if DST shifts the UTC instant.
class AlarmScheduler {
public:
    // Compute the next time the alarm should fire at or after `now`.
    // nullopt = alarm has expired (end condition met) or is unfireable
    //          (e.g. Weekly with empty weekdayMask).
    // Respects:
    //   - paused: returns nullopt
    //   - snoozeUntil > nowSec: returns the snooze time
    //   - endKind AfterN: nullopt if firedCount >= endCount
    //   - endKind OnDate: nullopt if computed time > endDate
    static std::optional<SYSTEMTIME> ComputeNextFireTime(const AlarmConfig& alarm,
                                                         const SYSTEMTIME& now);

    // Compact human-readable description of the repetition pattern
    // (used in Notizlist "Intervall" column). Uses Ls() for localization.
    static std::wstring DescribeInterval(const AlarmConfig& alarm);

    // Compare SYSTEMTIMEs as wall-clock (ignores wDayOfWeek).
    // Returns < 0 / 0 / > 0.
    static int CompareSysTime(const SYSTEMTIME& a, const SYSTEMTIME& b);

    // Naive conversion: wall-clock <-> seconds since epoch (treats local time as UTC).
    // Used for snoozeUntil serialization and time arithmetic.
    static int64_t SysTimeToNaiveSec(const SYSTEMTIME& st);
    static SYSTEMTIME NaiveSecToSysTime(int64_t sec);

    // State transitions (mutating)
    // Called by Application after a fire event: increments firedCount, clears snooze.
    static void AdvanceAfterFire(AlarmConfig& alarm);
    // Called when user clicks "Snooze": sets snoozeUntil = now + snoozeMinutes*60.
    static void AdvanceAfterSnooze(AlarmConfig& alarm, const SYSTEMTIME& now);
};
