#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

class Localization {
public:
    static Localization& Get();

    // Load language file; falls back to built-in English
    bool LoadLanguage(const std::wstring& langCode);

    // Get translated string by key; returns key if not found
    const std::wstring& Str(const std::wstring& key) const;

    // List available languages: vector of {code, name}
    std::vector<std::pair<std::wstring, std::wstring>> GetAvailableLanguages() const;

    const std::wstring& GetCurrentLanguage() const { return m_langCode; }

private:
    Localization() = default;

    std::unordered_map<std::wstring, std::wstring> m_strings;
    std::wstring m_langCode = L"en";

    void LoadDefaults();
    bool ParseLngFile(const std::wstring& path);
};

// Short-hand for accessing localized strings
inline const std::wstring& Ls(const std::wstring& key) {
    return Localization::Get().Str(key);
}
