#include "Localization.h"
#include "Utils.h"
#include <fstream>
#include <sstream>

namespace {
    std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return L"";
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) return L"";
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
        return w;
    }

    // Read the [info] name= value from a UTF-8 .lng file. GetPrivateProfileStringW
    // would read the file as CP_ACP and mojibake any non-ASCII display name.
    std::wstring ReadInfoName(const std::wstring& path, const std::wstring& fallback) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return fallback;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t st = (content.size() >= 3 &&
                     static_cast<unsigned char>(content[0]) == 0xEF &&
                     static_cast<unsigned char>(content[1]) == 0xBB &&
                     static_cast<unsigned char>(content[2]) == 0xBF) ? 3 : 0;
        std::istringstream ss(content.substr(st));
        std::string line;
        bool inInfo = false;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line[0] == '[') {
                auto e = line.find(']');
                if (e != std::string::npos) inInfo = (line.substr(1, e - 1) == "info");
                continue;
            }
            if (!inInfo) continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
            size_t ks = k.find_first_not_of(" \t");
            if (ks != std::string::npos && ks > 0) k = k.substr(ks);
            if (k == "name") {
                std::string v = line.substr(eq + 1);
                size_t vs = v.find_first_not_of(" \t");
                if (vs != std::string::npos) v = v.substr(vs);
                std::wstring w = Utf8ToWide(v);
                return w.empty() ? fallback : w;
            }
        }
        return fallback;
    }
}

Localization& Localization::Get() {
    static Localization instance;
    return instance;
}

void Localization::LoadDefaults() {
    m_strings.clear();

    // Built-in English defaults
    m_strings[L"menu.new_note"]       = L"&New Note";
    m_strings[L"menu.show_notes"]     = L"&Show Notes";
    m_strings[L"menu.hide_notes"]     = L"&Hide Notes";
    m_strings[L"menu.note_list"]      = L"Note &List...";
    m_strings[L"menu.settings"]       = L"&Settings";
    m_strings[L"menu.language"]       = L"&Language";
    m_strings[L"menu.exit"]           = L"E&xit";

    m_strings[L"note.edit"]           = L"&Edit\tEnter";
    m_strings[L"note.copy"]           = L"&Copy\tC";
    m_strings[L"note.delete"]         = L"&Delete\tDel";
    m_strings[L"note.hide"]           = L"&Hide";
    m_strings[L"note.always_on_top"]  = L"Always on &Top\tO";
    m_strings[L"note.new_note"]       = L"&New Note";

    m_strings[L"notelist.title"]      = L"UltraNote - Note List";
    m_strings[L"notelist.file"]       = L"&File";
    m_strings[L"notelist.close"]      = L"&Close";
    m_strings[L"notelist.note"]       = L"&Note";
    m_strings[L"notelist.new_note"]   = L"&New Note";
    m_strings[L"notelist.edit"]       = L"&Edit";
    m_strings[L"notelist.delete"]     = L"&Delete";
    m_strings[L"notelist.col_text"]   = L"Note";
    m_strings[L"notelist.col_created"] = L"Created";

    m_strings[L"confirm.delete_one"]  = L"Delete this note?";
    m_strings[L"confirm.delete_multi"] = L"Delete %d notes?";

    m_strings[L"app.already_running"] = L"UltraNote is already running.";
    m_strings[L"note.empty"]          = L"(empty)";
}

bool Localization::LoadLanguage(const std::wstring& langCode) {
    // Always start with English defaults as ultimate fallback
    LoadDefaults();
    m_langCode = L"en";

    // Always try to load en.lng first (overrides hardcoded defaults with full set)
    std::wstring langDir = GetExeDirectory() + L"\\lang";
    ParseLngFile(langDir + L"\\en.lng");

    if (langCode.empty() || langCode == L"en") {
        return true;
    }

    // Load the requested language on top
    std::wstring langFile = langDir + L"\\" + langCode + L".lng";

    if (ParseLngFile(langFile)) {
        m_langCode = langCode;
        return true;
    }

    // Fallback: English remains loaded
    return false;
}

bool Localization::ParseLngFile(const std::wstring& path) {
    // Open file as UTF-8
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Skip UTF-8 BOM if present
    size_t start = 0;
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        start = 3;
    }

    // Parse line by line
    bool inStringsSection = false;
    std::istringstream stream(content.substr(start));
    std::string line;

    while (std::getline(stream, line)) {
        // Remove carriage return
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        // Section header
        if (line[0] == '[') {
            auto end = line.find(']');
            if (end != std::string::npos) {
                std::string section = line.substr(1, end - 1);
                inStringsSection = (section == "strings");
            }
            continue;
        }

        // Key=Value pairs (only in [strings] section)
        if (!inStringsSection) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Trim whitespace from key
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();
        size_t keyStart = key.find_first_not_of(" \t");
        if (keyStart != std::string::npos && keyStart > 0)
            key = key.substr(keyStart);

        if (key.empty()) continue;

        // Convert key and value from UTF-8 to wide string
        auto toWide = [](const std::string& s) -> std::wstring {
            if (s.empty()) return L"";
            int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                          static_cast<int>(s.size()), nullptr, 0);
            if (len <= 0) return L"";
            std::wstring result(static_cast<size_t>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                static_cast<int>(s.size()), &result[0], len);
            return result;
        };

        m_strings[toWide(key)] = toWide(value);
    }

    return true;
}

const std::wstring& Localization::Str(const std::wstring& key) const {
    auto it = m_strings.find(key);
    if (it != m_strings.end()) {
        return it->second;
    }
    // Return key itself if not found
    return key;
}

std::vector<std::pair<std::wstring, std::wstring>> Localization::GetAvailableLanguages() const {
    std::vector<std::pair<std::wstring, std::wstring>> langs;

    // Always include built-in English
    langs.push_back({L"en", L"English"});

    // Scan lang/ directory for .lng files
    std::wstring langDir = GetExeDirectory() + L"\\lang\\*.lng";

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(langDir.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return langs;

    do {
        std::wstring filename = fd.cFileName;
        // Extract code from filename (e.g., "de.lng" -> "de")
        auto dot = filename.find(L'.');
        if (dot == std::wstring::npos) continue;
        std::wstring code = filename.substr(0, dot);
        if (code == L"en") continue; // Already added

        // Read the display name from the [info] section (UTF-8 aware)
        std::wstring filePath = GetExeDirectory() + L"\\lang\\" + filename;
        std::wstring name = ReadInfoName(filePath, code);

        langs.push_back({code, name});
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return langs;
}
