#pragma once

#include "Note.h"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

class Storage {
public:
    // Load all notes from notes.json; returns empty vector if file missing
    static std::vector<NoteData> LoadNotes(uint64_t& outNextId,
                                            std::vector<std::wstring>& outFolders);

    // Save all notes to notes.json (atomic: write .tmp then rename)
    static bool SaveNotes(const std::vector<NoteData>& notes, uint64_t nextId,
                          const std::vector<std::wstring>& folders);

    // Get the path to notes.json (in EXE directory)
    static std::wstring GetNotesFilePath();

    // Key-value settings (settings.json)
    // Supports int and string values in the same file
    static std::map<std::wstring, int> LoadSettings();
    static std::map<std::wstring, std::wstring> LoadSettingsStr();
    static bool SaveSettings(const std::map<std::wstring, int>& settings);
    static bool SaveAllSettings(const std::map<std::wstring, int>& intSettings,
                                const std::map<std::wstring, std::wstring>& strSettings);

    // JSON primitives (used by settings helpers)
    static void SkipWhitespace(const wchar_t*& p);
    static bool ParseString(const wchar_t*& p, std::wstring& out);
    static bool ParseInt(const wchar_t*& p, int64_t& out);
    static std::wstring EscapeJsonString(const std::wstring& s);

private:
    // JSON writing helpers
    static std::wstring SerializeNotes(const std::vector<NoteData>& notes, uint64_t nextId,
                                        const std::vector<std::wstring>& folders);
    static std::wstring SerializeNote(const NoteData& note);
    static std::wstring ColorToHex(COLORREF c);

    // JSON reading helpers
    static bool ParseNotes(const std::wstring& json, std::vector<NoteData>& notes,
                           uint64_t& nextId, std::vector<std::wstring>& folders);
    static bool ParseStringArray(const wchar_t*& p, std::vector<std::wstring>& out);
    static std::wstring UnescapeJsonString(const std::wstring& s);
    static COLORREF HexToColor(const std::wstring& hex);

    // Minimal JSON pull-parser helpers
    static bool Expect(const wchar_t*& p, wchar_t ch);
    static bool ParseBool(const wchar_t*& p, bool& out);
    static bool SkipValue(const wchar_t*& p);
    static bool ParseNoteObject(const wchar_t*& p, NoteData& note);
    static bool ParseLayoutObject(const wchar_t*& p, NoteLayout& layout);
};
