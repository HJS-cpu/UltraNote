#include "Storage.h"
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <cwchar>

// ============================================================================
// File path
// ============================================================================

std::wstring Storage::GetNotesFilePath() {
    return GetExeDirectory() + L"\\notes.json";
}

// ============================================================================
// JSON Writing
// ============================================================================

std::wstring Storage::ColorToHex(COLORREF c) {
    wchar_t buf[8];
    swprintf_s(buf, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

// SYSTEMTIME <-> "YYYY-MM-DD HH:MM" (local wall-clock; seconds dropped)
static std::wstring SysTimeToString(const SYSTEMTIME& st) {
    wchar_t buf[32];
    swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

static bool StringToSysTime(const std::wstring& s, SYSTEMTIME& st) {
    ZeroMemory(&st, sizeof(st));
    if (s.size() < 10) return false;
    st.wYear   = static_cast<WORD>(wcstoul(s.substr(0, 4).c_str(), nullptr, 10));
    st.wMonth  = static_cast<WORD>(wcstoul(s.substr(5, 2).c_str(), nullptr, 10));
    st.wDay    = static_cast<WORD>(wcstoul(s.substr(8, 2).c_str(), nullptr, 10));
    if (s.size() >= 16) {
        st.wHour   = static_cast<WORD>(wcstoul(s.substr(11, 2).c_str(), nullptr, 10));
        st.wMinute = static_cast<WORD>(wcstoul(s.substr(14, 2).c_str(), nullptr, 10));
    }
    // Populate wDayOfWeek via round-trip through FILETIME
    FILETIME ft;
    SYSTEMTIME normalized = st;
    if (SystemTimeToFileTime(&normalized, &ft)) {
        FileTimeToSystemTime(&ft, &normalized);
        st.wDayOfWeek = normalized.wDayOfWeek;
    }
    return st.wYear != 0;
}

std::wstring Storage::EscapeJsonString(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t ch : s) {
        switch (ch) {
            case L'\"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n";  break;
            case L'\r': out += L"\\r";  break;
            case L'\t': out += L"\\t";  break;
            default:
                if (ch < 0x20) {
                    wchar_t buf[8];
                    swprintf_s(buf, L"\\u%04X", static_cast<unsigned>(ch));
                    out += buf;
                } else {
                    out += ch;
                }
                break;
        }
    }
    return out;
}

std::wstring Storage::SerializeNote(const NoteData& note) {
    std::wstring s;
    s += L"    {\n";
    s += L"      \"id\": " + std::to_wstring(note.id) + L",\n";
    s += L"      \"text\": \"" + EscapeJsonString(note.text) + L"\",\n";
    s += L"      \"title\": \"" + EscapeJsonString(note.title) + L"\",\n";
    s += L"      \"folder\": \"" + EscapeJsonString(note.folder) + L"\",\n";
    s += L"      \"x\": " + std::to_wstring(note.x) + L",\n";
    s += L"      \"y\": " + std::to_wstring(note.y) + L",\n";
    s += L"      \"width\": " + std::to_wstring(note.width) + L",\n";
    s += L"      \"height\": " + std::to_wstring(note.height) + L",\n";
    s += L"      \"minimized\": " + std::wstring(note.isMinimized ? L"true" : L"false") + L",\n";
    s += L"      \"hidden\": " + std::wstring(note.isHidden ? L"true" : L"false") + L",\n";
    s += L"      \"layout\": {\n";
    s += L"        \"bgColor\": \"" + ColorToHex(note.layout.backgroundColor) + L"\",\n";
    s += L"        \"textColor\": \"" + ColorToHex(note.layout.textColor) + L"\",\n";
    s += L"        \"borderColor\": \"" + ColorToHex(note.layout.borderColor) + L"\",\n";
    s += L"        \"fontFace\": \"" + EscapeJsonString(note.layout.fontFace) + L"\",\n";
    s += L"        \"fontSize\": " + std::to_wstring(note.layout.fontSizePts) + L",\n";
    s += L"        \"bold\": " + std::wstring(note.layout.fontBold ? L"true" : L"false") + L",\n";
    s += L"        \"italic\": " + std::wstring(note.layout.fontItalic ? L"true" : L"false") + L",\n";
    s += L"        \"alwaysOnTop\": " + std::wstring(note.layout.alwaysOnTop ? L"true" : L"false") + L"\n";
    s += L"      },\n";
    s += L"      \"createdAt\": " + std::to_wstring(note.createdAt) + L",\n";
    s += L"      \"modifiedAt\": " + std::to_wstring(note.modifiedAt) + L",\n";
    s += L"      \"showAttachments\": " + std::wstring(note.showAttachments ? L"true" : L"false") + L",\n";
    s += L"      \"attachments\": [";
    for (size_t i = 0; i < note.attachments.size(); ++i) {
        if (i > 0) s += L", ";
        s += L"\"" + EscapeJsonString(note.attachments[i]) + L"\"";
    }
    s += L"]";
    if (note.alarm.has_value()) {
        const auto& a = *note.alarm;
        s += L",\n      \"alarm\": {\n";
        s += L"        \"kind\": " + std::to_wstring(static_cast<int>(a.kind)) + L",\n";
        s += L"        \"startTime\": \"" + SysTimeToString(a.startTime) + L"\",\n";
        s += L"        \"intervalDays\": " + std::to_wstring(a.intervalDays) + L",\n";
        s += L"        \"weekdayMask\": " + std::to_wstring(a.weekdayMask) + L",\n";
        s += L"        \"monthDay\": " + std::to_wstring(a.monthDay) + L",\n";
        s += L"        \"nthWeek\": " + std::to_wstring(a.nthWeek) + L",\n";
        s += L"        \"nthWeekday\": " + std::to_wstring(a.nthWeekday) + L",\n";
        s += L"        \"quarterDay\": " + std::to_wstring(a.quarterDay) + L",\n";
        s += L"        \"endKind\": " + std::to_wstring(static_cast<int>(a.endKind)) + L",\n";
        s += L"        \"endCount\": " + std::to_wstring(a.endCount) + L",\n";
        s += L"        \"endDate\": \"" + SysTimeToString(a.endDate) + L"\",\n";
        s += L"        \"popup\": " + std::wstring(a.popup ? L"true" : L"false") + L",\n";
        s += L"        \"sound\": " + std::wstring(a.sound ? L"true" : L"false") + L",\n";
        s += L"        \"soundFile\": \"" + EscapeJsonString(a.soundFile) + L"\",\n";
        s += L"        \"snoozeMinutes\": " + std::to_wstring(a.snoozeMinutes) + L",\n";
        s += L"        \"firedCount\": " + std::to_wstring(a.firedCount) + L",\n";
        s += L"        \"snoozeUntil\": " + std::to_wstring(a.snoozeUntil) + L",\n";
        s += L"        \"paused\": " + std::wstring(a.paused ? L"true" : L"false") + L"\n";
        s += L"      }";
    }
    s += L"\n    }";
    return s;
}

std::wstring Storage::SerializeNotes(const std::vector<std::unique_ptr<NoteData>>& notes, uint64_t nextId,
                                      const std::vector<std::wstring>& folders) {
    std::wstring s;
    s += L"{\n";
    s += L"  \"version\": 1,\n";
    s += L"  \"nextId\": " + std::to_wstring(nextId) + L",\n";
    s += L"  \"folders\": [";
    for (size_t i = 0; i < folders.size(); ++i) {
        if (i > 0) s += L", ";
        s += L"\"" + EscapeJsonString(folders[i]) + L"\"";
    }
    s += L"],\n";
    s += L"  \"notes\": [\n";
    for (size_t i = 0; i < notes.size(); ++i) {
        s += SerializeNote(*notes[i]);
        if (i + 1 < notes.size()) s += L",";
        s += L"\n";
    }
    s += L"  ]\n";
    s += L"}\n";
    return s;
}

bool Storage::SaveNotes(const std::vector<std::unique_ptr<NoteData>>& notes, uint64_t nextId,
                         const std::vector<std::wstring>& folders) {
    std::wstring json = SerializeNotes(notes, nextId, folders);
    std::wstring filePath = GetNotesFilePath();
    std::wstring tmpPath = filePath + L".tmp";

    // Write to temp file
    std::wofstream ofs(tmpPath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;

    // Write BOM for UTF-16... actually let's use UTF-8
    ofs.close();

    // Write as UTF-8
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, tmpPath.c_str(), L"wb") != 0 || !fp)
        return false;

    // Convert wstring to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(),
                                       static_cast<int>(json.size()),
                                       nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) { fclose(fp); return false; }

    std::string utf8(static_cast<size_t>(utf8Len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(),
                        static_cast<int>(json.size()),
                        &utf8[0], utf8Len, nullptr, nullptr);

    size_t written = fwrite(utf8.data(), 1, utf8.size(), fp);
    fclose(fp);

    if (written != utf8.size()) return false;

    // Atomic rename
    if (!MoveFileExW(tmpPath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    return true;
}

// ============================================================================
// JSON Reading
// ============================================================================

COLORREF Storage::HexToColor(const std::wstring& hex) {
    if (hex.size() < 7 || hex[0] != L'#') return RGB(0, 0, 0);
    unsigned r = wcstoul(hex.substr(1, 2).c_str(), nullptr, 16);
    unsigned g = wcstoul(hex.substr(3, 2).c_str(), nullptr, 16);
    unsigned b = wcstoul(hex.substr(5, 2).c_str(), nullptr, 16);
    return RGB(r, g, b);
}

std::wstring Storage::UnescapeJsonString(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case L'"':  out += L'"'; break;
                case L'\\': out += L'\\'; break;
                case L'n':  out += L'\n'; break;
                case L'r':  out += L'\r'; break;
                case L't':  out += L'\t'; break;
                case L'u':
                    if (i + 4 < s.size()) {
                        unsigned val = wcstoul(s.substr(i + 1, 4).c_str(), nullptr, 16);
                        out += static_cast<wchar_t>(val);
                        i += 4;
                    }
                    break;
                default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Minimal recursive descent JSON parser for our fixed schema

void Storage::SkipWhitespace(const wchar_t*& p) {
    while (*p == L' ' || *p == L'\t' || *p == L'\n' || *p == L'\r') ++p;
}

bool Storage::Expect(const wchar_t*& p, wchar_t ch) {
    SkipWhitespace(p);
    if (*p != ch) return false;
    ++p;
    return true;
}

bool Storage::ParseString(const wchar_t*& p, std::wstring& out) {
    SkipWhitespace(p);
    if (*p != L'"') return false;
    ++p;
    std::wstring raw;
    while (*p && *p != L'"') {
        if (*p == L'\\') {
            raw += *p++;
            if (*p) raw += *p++;
        } else {
            raw += *p++;
        }
    }
    if (*p != L'"') return false;
    ++p;
    out = UnescapeJsonString(raw);
    return true;
}

bool Storage::ParseInt(const wchar_t*& p, int64_t& out) {
    SkipWhitespace(p);
    wchar_t* end = nullptr;
    out = wcstoll(p, &end, 10);
    if (end == p) return false;
    p = end;
    return true;
}

bool Storage::ParseBool(const wchar_t*& p, bool& out) {
    SkipWhitespace(p);
    if (wcsncmp(p, L"true", 4) == 0) {
        out = true;
        p += 4;
        return true;
    }
    if (wcsncmp(p, L"false", 5) == 0) {
        out = false;
        p += 5;
        return true;
    }
    return false;
}

bool Storage::SkipValue(const wchar_t*& p) {
    SkipWhitespace(p);
    if (*p == L'"') {
        std::wstring dummy;
        return ParseString(p, dummy);
    }
    if (*p == L'{') {
        ++p;
        int depth = 1;
        while (*p && depth > 0) {
            if (*p == L'{') ++depth;
            else if (*p == L'}') --depth;
            else if (*p == L'"') {
                std::wstring dummy;
                --p; // back up to re-parse the quote
                // Actually, just skip the string manually
                ++p; // skip opening quote
                while (*p && *p != L'"') {
                    if (*p == L'\\') ++p;
                    if (*p) ++p;
                }
                if (*p == L'"') ++p;
                continue;
            }
            if (depth > 0) ++p;
        }
        if (*p == L'}') ++p;
        return true;
    }
    if (*p == L'[') {
        ++p;
        int depth = 1;
        while (*p && depth > 0) {
            if (*p == L'[') ++depth;
            else if (*p == L']') --depth;
            if (depth > 0) ++p;
        }
        if (*p == L']') ++p;
        return true;
    }
    // Number or bool or null
    while (*p && *p != L',' && *p != L'}' && *p != L']' &&
           *p != L' ' && *p != L'\t' && *p != L'\n' && *p != L'\r') {
        ++p;
    }
    return true;
}

bool Storage::ParseLayoutObject(const wchar_t*& p, NoteLayout& layout) {
    if (!Expect(p, L'{')) return false;

    while (*p && *p != L'}') {
        SkipWhitespace(p);
        if (*p == L'}') break;

        std::wstring key;
        if (!ParseString(p, key)) return false;
        if (!Expect(p, L':')) return false;

        if (key == L"bgColor") {
            std::wstring val;
            if (!ParseString(p, val)) return false;
            layout.backgroundColor = HexToColor(val);
        } else if (key == L"textColor") {
            std::wstring val;
            if (!ParseString(p, val)) return false;
            layout.textColor = HexToColor(val);
        } else if (key == L"borderColor") {
            std::wstring val;
            if (!ParseString(p, val)) return false;
            layout.borderColor = HexToColor(val);
        } else if (key == L"fontFace") {
            if (!ParseString(p, layout.fontFace)) return false;
        } else if (key == L"fontSize") {
            int64_t val;
            if (!ParseInt(p, val)) return false;
            layout.fontSizePts = static_cast<int>(val);
        } else if (key == L"bold") {
            if (!ParseBool(p, layout.fontBold)) return false;
        } else if (key == L"italic") {
            if (!ParseBool(p, layout.fontItalic)) return false;
        } else if (key == L"alwaysOnTop") {
            if (!ParseBool(p, layout.alwaysOnTop)) return false;
        } else {
            if (!SkipValue(p)) return false;
        }

        SkipWhitespace(p);
        if (*p == L',') ++p;
    }

    return Expect(p, L'}');
}

bool Storage::ParseNoteObject(const wchar_t*& p, NoteData& note) {
    if (!Expect(p, L'{')) return false;

    while (*p && *p != L'}') {
        SkipWhitespace(p);
        if (*p == L'}') break;

        std::wstring key;
        if (!ParseString(p, key)) return false;
        if (!Expect(p, L':')) return false;

        if (key == L"id") {
            int64_t val;
            if (!ParseInt(p, val)) return false;
            note.id = static_cast<uint64_t>(val);
        } else if (key == L"text") {
            if (!ParseString(p, note.text)) return false;
        } else if (key == L"title") {
            if (!ParseString(p, note.title)) return false;
        } else if (key == L"folder") {
            if (!ParseString(p, note.folder)) return false;
        } else if (key == L"x") {
            int64_t val; if (!ParseInt(p, val)) return false;
            note.x = static_cast<int>(val);
        } else if (key == L"y") {
            int64_t val; if (!ParseInt(p, val)) return false;
            note.y = static_cast<int>(val);
        } else if (key == L"width") {
            int64_t val; if (!ParseInt(p, val)) return false;
            note.width = static_cast<int>(val);
        } else if (key == L"height") {
            int64_t val; if (!ParseInt(p, val)) return false;
            note.height = static_cast<int>(val);
        } else if (key == L"minimized") {
            if (!ParseBool(p, note.isMinimized)) return false;
        } else if (key == L"hidden") {
            if (!ParseBool(p, note.isHidden)) return false;
        } else if (key == L"layout") {
            if (!ParseLayoutObject(p, note.layout)) return false;
        } else if (key == L"createdAt") {
            if (!ParseInt(p, note.createdAt)) return false;
        } else if (key == L"modifiedAt") {
            if (!ParseInt(p, note.modifiedAt)) return false;
        } else if (key == L"showAttachments") {
            if (!ParseBool(p, note.showAttachments)) return false;
        } else if (key == L"attachments") {
            if (!ParseStringArray(p, note.attachments)) return false;
        } else if (key == L"alarm") {
            AlarmConfig alarm;
            if (!ParseAlarmObject(p, alarm)) return false;
            note.alarm = std::move(alarm);
        } else {
            if (!SkipValue(p)) return false;
        }

        SkipWhitespace(p);
        if (*p == L',') ++p;
    }

    return Expect(p, L'}');
}

bool Storage::ParseAlarmObject(const wchar_t*& p, AlarmConfig& alarm) {
    if (!Expect(p, L'{')) return false;

    while (*p && *p != L'}') {
        SkipWhitespace(p);
        if (*p == L'}') break;

        std::wstring key;
        if (!ParseString(p, key)) return false;
        if (!Expect(p, L':')) return false;

        if (key == L"kind") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.kind = static_cast<AlarmKind>(val);
        } else if (key == L"startTime") {
            std::wstring val; if (!ParseString(p, val)) return false;
            StringToSysTime(val, alarm.startTime);
        } else if (key == L"intervalDays") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.intervalDays = static_cast<int>(val);
        } else if (key == L"weekdayMask") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.weekdayMask = static_cast<uint8_t>(val);
        } else if (key == L"monthDay") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.monthDay = static_cast<int>(val);
        } else if (key == L"nthWeek") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.nthWeek = static_cast<int>(val);
        } else if (key == L"nthWeekday") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.nthWeekday = static_cast<int>(val);
        } else if (key == L"quarterDay") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.quarterDay = static_cast<int>(val);
        } else if (key == L"endKind") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.endKind = static_cast<AlarmEndKind>(val);
        } else if (key == L"endCount") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.endCount = static_cast<int>(val);
        } else if (key == L"endDate") {
            std::wstring val; if (!ParseString(p, val)) return false;
            StringToSysTime(val, alarm.endDate);
        } else if (key == L"popup") {
            if (!ParseBool(p, alarm.popup)) return false;
        } else if (key == L"sound") {
            if (!ParseBool(p, alarm.sound)) return false;
        } else if (key == L"soundFile") {
            if (!ParseString(p, alarm.soundFile)) return false;
        } else if (key == L"snoozeMinutes") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.snoozeMinutes = static_cast<int>(val);
        } else if (key == L"firedCount") {
            int64_t val; if (!ParseInt(p, val)) return false;
            alarm.firedCount = static_cast<int>(val);
        } else if (key == L"snoozeUntil") {
            if (!ParseInt(p, alarm.snoozeUntil)) return false;
        } else if (key == L"paused") {
            if (!ParseBool(p, alarm.paused)) return false;
        } else {
            if (!SkipValue(p)) return false;
        }

        SkipWhitespace(p);
        if (*p == L',') ++p;
    }

    return Expect(p, L'}');
}

bool Storage::ParseStringArray(const wchar_t*& p, std::vector<std::wstring>& out) {
    if (!Expect(p, L'[')) return false;
    SkipWhitespace(p);
    while (*p && *p != L']') {
        std::wstring val;
        if (!ParseString(p, val)) return false;
        out.push_back(std::move(val));
        SkipWhitespace(p);
        if (*p == L',') ++p;
        SkipWhitespace(p);
    }
    return Expect(p, L']');
}

bool Storage::ParseNotes(const std::wstring& json, std::vector<std::unique_ptr<NoteData>>& notes,
                          uint64_t& nextId, std::vector<std::wstring>& folders) {
    const wchar_t* p = json.c_str();

    if (!Expect(p, L'{')) return false;

    while (*p && *p != L'}') {
        SkipWhitespace(p);
        if (*p == L'}') break;

        std::wstring key;
        if (!ParseString(p, key)) return false;
        if (!Expect(p, L':')) return false;

        if (key == L"version") {
            int64_t val;
            if (!ParseInt(p, val)) return false;
            // Currently only version 1 supported
        } else if (key == L"nextId") {
            int64_t val;
            if (!ParseInt(p, val)) return false;
            nextId = static_cast<uint64_t>(val);
        } else if (key == L"folders") {
            if (!ParseStringArray(p, folders)) return false;
        } else if (key == L"notes") {
            if (!Expect(p, L'[')) return false;
            SkipWhitespace(p);
            while (*p && *p != L']') {
                if (*p == L']') break;
                auto note = std::make_unique<NoteData>();
                if (!ParseNoteObject(p, *note)) return false;
                notes.push_back(std::move(note));
                SkipWhitespace(p);
                if (*p == L',') ++p;
                SkipWhitespace(p);
            }
            if (!Expect(p, L']')) return false;
        } else {
            if (!SkipValue(p)) return false;
        }

        SkipWhitespace(p);
        if (*p == L',') ++p;
    }

    return true;
}

std::vector<std::unique_ptr<NoteData>> Storage::LoadNotes(uint64_t& outNextId,
                                                            std::vector<std::wstring>& outFolders) {
    std::vector<std::unique_ptr<NoteData>> notes;
    outNextId = 1;

    std::wstring filePath = GetNotesFilePath();

    // Read file as UTF-8
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, filePath.c_str(), L"rb") != 0 || !fp)
        return notes;

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize <= 0) { fclose(fp); return notes; }

    std::string utf8(static_cast<size_t>(fileSize), '\0');
    fread(&utf8[0], 1, static_cast<size_t>(fileSize), fp);
    fclose(fp);

    // Convert UTF-8 to wstring
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                       static_cast<int>(utf8.size()),
                                       nullptr, 0);
    if (wideLen <= 0) return notes;

    std::wstring json(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()),
                        &json[0], wideLen);

    ParseNotes(json, notes, outNextId, outFolders);
    return notes;
}

// ============================================================================
// Settings (mixed int + string key-value store)
// ============================================================================

static std::wstring ReadSettingsFile() {
    std::wstring filePath = GetExeDirectory() + L"\\settings.json";
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, filePath.c_str(), L"rb") != 0 || !fp)
        return {};

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fileSize <= 0) { fclose(fp); return {}; }

    std::string utf8(static_cast<size_t>(fileSize), '\0');
    fread(&utf8[0], 1, static_cast<size_t>(fileSize), fp);
    fclose(fp);

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                       static_cast<int>(utf8.size()), nullptr, 0);
    if (wideLen <= 0) return {};

    std::wstring json(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()), &json[0], wideLen);
    return json;
}

static void ParseSettingsJson(const std::wstring& json,
                               std::map<std::wstring, int>* intOut,
                               std::map<std::wstring, std::wstring>* strOut) {
    const wchar_t* p = json.c_str();
    Storage::SkipWhitespace(p);
    if (*p != L'{') return;
    ++p;

    while (*p && *p != L'}') {
        Storage::SkipWhitespace(p);
        if (*p == L'}') break;

        std::wstring key;
        if (!Storage::ParseString(p, key)) break;
        Storage::SkipWhitespace(p);
        if (*p != L':') break;
        ++p;
        Storage::SkipWhitespace(p);

        if (*p == L'"') {
            // String value
            std::wstring val;
            if (!Storage::ParseString(p, val)) break;
            if (strOut) (*strOut)[key] = val;
        } else {
            // Int value
            int64_t val;
            if (!Storage::ParseInt(p, val)) break;
            if (intOut) (*intOut)[key] = static_cast<int>(val);
        }

        Storage::SkipWhitespace(p);
        if (*p == L',') ++p;
    }
}

std::map<std::wstring, int> Storage::LoadSettings() {
    std::map<std::wstring, int> settings;
    std::wstring json = ReadSettingsFile();
    if (!json.empty())
        ParseSettingsJson(json, &settings, nullptr);
    return settings;
}

std::map<std::wstring, std::wstring> Storage::LoadSettingsStr() {
    std::map<std::wstring, std::wstring> settings;
    std::wstring json = ReadSettingsFile();
    if (!json.empty())
        ParseSettingsJson(json, nullptr, &settings);
    return settings;
}

static bool WriteSettingsFile(const std::wstring& json) {
    std::wstring filePath = GetExeDirectory() + L"\\settings.json";
    std::wstring tmpPath = filePath + L".tmp";

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(),
                                       static_cast<int>(json.size()),
                                       nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return false;

    std::string utf8(static_cast<size_t>(utf8Len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(),
                        static_cast<int>(json.size()),
                        &utf8[0], utf8Len, nullptr, nullptr);

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, tmpPath.c_str(), L"wb") != 0 || !fp)
        return false;

    size_t written = fwrite(utf8.data(), 1, utf8.size(), fp);
    fclose(fp);
    if (written != utf8.size()) return false;

    if (!MoveFileExW(tmpPath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }
    return true;
}

static std::wstring BuildSettingsJson(const std::map<std::wstring, int>& intSettings,
                                       const std::map<std::wstring, std::wstring>& strSettings) {
    std::wstring json = L"{\n";
    size_t total = intSettings.size() + strSettings.size();
    size_t i = 0;

    for (auto& [key, val] : intSettings) {
        json += L"  \"" + key + L"\": " + std::to_wstring(val);
        if (++i < total) json += L",";
        json += L"\n";
    }
    for (auto& [key, val] : strSettings) {
        json += L"  \"" + key + L"\": \"" + Storage::EscapeJsonString(val) + L"\"";
        if (++i < total) json += L",";
        json += L"\n";
    }
    json += L"}\n";
    return json;
}

bool Storage::SaveSettings(const std::map<std::wstring, int>& settings) {
    // Preserve existing string settings
    auto strSettings = LoadSettingsStr();
    return WriteSettingsFile(BuildSettingsJson(settings, strSettings));
}

bool Storage::SaveAllSettings(const std::map<std::wstring, int>& intSettings,
                               const std::map<std::wstring, std::wstring>& strSettings) {
    return WriteSettingsFile(BuildSettingsJson(intSettings, strSettings));
}
