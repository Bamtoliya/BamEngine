#include "LocalizationManager.h"
#include "PlatformPath.h"
#include "Functions.h"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>

namespace
{
    bool ReadProjectCsvRow(std::string_view& text, std::vector<std::string>& row)
    {
        row.clear();
        for (;;)
        {
            std::string field;
            if (!text.empty() && text.front() == '"')
            {
                text.remove_prefix(1);
                for (;;)
                {
                    const auto quote = text.find('"');
                    if (quote == std::string_view::npos) return false;
                    field.append(text.substr(0, quote));
                    text.remove_prefix(quote + 1);
                    if (text.empty() || text.front() != '"') break;
                    field.push_back('"');
                    text.remove_prefix(1);
                }
            }
            else
            {
                const auto end = text.find_first_of(",\r\n");
                const auto value = text.substr(0, end);
                if (value.find('"') != std::string_view::npos) return false;
                field.assign(value);
                text.remove_prefix(value.size());
            }
            row.push_back(std::move(field));
            if (text.empty()) return true;

            const char separator = text.front();
            text.remove_prefix(1);
            if (separator == ',') continue;
            if (separator == '\n') return true;
            if (separator != '\r') return false;
            if (!text.empty() && text.front() == '\n') text.remove_prefix(1);
            return true;
        }
    }
}

Engine::EResult Engine::LocalizationManager::ReloadProjectData()
{
    const auto directory = PlatformPath::ResolveUri(ProjectCatalogDirectory);
    std::vector<std::filesystem::path> files;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(directory, error);
    if (error && error != std::errc::no_such_file_or_directory)
    {
        BAM_LOG(Error, "Engine", "Localization", "Could not inspect the project catalog directory: {}", error.message());
        return EResult::Fail;
    }
    if (std::filesystem::exists(status))
    {
        if (!std::filesystem::is_directory(status))
        {
            BAM_LOG(Error, "Engine", "Localization", "The project catalog path is not a directory.");
            return EResult::Fail;
        }
        for (auto entry = std::filesystem::directory_iterator(directory, error);
            !error && entry != std::filesystem::directory_iterator{}; entry.increment(error))
        {
            const auto entryStatus = entry->symlink_status(error);
            if (error) break;
            if (!std::filesystem::is_regular_file(entryStatus)) continue;
            const auto extension = ToLowerWStr(entry->path().extension().wstring());
            if (extension == L".csv" || extension == L".json") files.push_back(entry->path());
        }
        if (error)
        {
            BAM_LOG(Error, "Engine", "Localization", "Could not inspect project catalog files: {}", error.message());
            return EResult::Fail;
        }
    }
    std::ranges::sort(files);

    std::vector<TextMap> projectData(static_cast<std::size_t>(ELocalizationLanguage::Max));
    const auto add = [&](const std::filesystem::path& path, std::size_t language, const std::string& key, const std::string& value)
    {
        if (value.empty()) return true;
        if (!key.empty() && projectData[language].try_emplace(key, value).second) return true;
        BAM_LOG(Error, "Engine", "Localization", "Empty or duplicate project translation key '{}' in {}.", key, Engine::Unicode::WideToUtf8(path.wstring()));
        return false;
    };
    for (const auto& path : files)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            BAM_LOG(Error, "Engine", "Localization", "Could not read project localization file {}.", Engine::Unicode::WideToUtf8(path.wstring()));
            return EResult::Fail;
        }
        std::ostringstream bytes;
        bytes << file.rdbuf();
        if (file.bad() || bytes.bad())
        {
            BAM_LOG(Error, "Engine", "Localization", "Read failed for project localization file {}.", Engine::Unicode::WideToUtf8(path.wstring()));
            return EResult::Fail;
        }
        const auto content = bytes.str();
        std::string_view source(content);
        if (source.starts_with("\xEF\xBB\xBF")) source.remove_prefix(3);
        if (!glz::validate_utf8(source.data(), source.size()) || source.find('\0') != std::string_view::npos)
        {
            BAM_LOG(Error, "Engine", "Localization", "Project localization file {} must contain UTF-8 text.", Engine::Unicode::WideToUtf8(path.wstring()));
            return EResult::Fail;
        }

        if (ToLowerWStr(path.extension().wstring()) == L".csv")
        {
            std::vector<std::string> row;
            const auto malformed = [&](std::size_t record)
            {
                BAM_LOG(Error, "Engine", "Localization", "Malformed project localization CSV {} at record {}.",
                    Engine::Unicode::WideToUtf8(path.wstring()), record);
                return EResult::Fail;
            };
            if (!ReadProjectCsvRow(source, row) ||
                row != std::vector<std::string>{ "Group", "Key", "English", "Korean" })
                return malformed(1);

            std::string group;
            for (std::size_t record = 2; !source.empty(); ++record)
            {
                if (!ReadProjectCsvRow(source, row)) return malformed(record);
                if (row.size() == 1 && row.front().empty()) continue;
                if (row.size() != 4 || row[1].empty()) return malformed(record);
                if (!row[0].empty()) group = row[0];
                const auto key = group.empty() ? row[1] : group + "." + row[1];
                if (!add(path, 0, key, row[2]) || !add(path, 1, key, row[3])) return EResult::Fail;
            }
        }
        else
        {
            std::unordered_map<std::string, std::unordered_map<std::string, std::string>> catalog;
            const auto result = glz::read_json(catalog, source);
            if (result || catalog.empty())
            {
                BAM_LOG(Error, "Engine", "Localization", "Malformed project localization JSON {}.", Engine::Unicode::WideToUtf8(path.wstring()));
                return EResult::Fail;
            }
            constexpr std::array<std::string_view, 2> languages{ "English", "Korean" };
            for (const auto& [name, entries] : catalog)
            {
                const auto language = std::ranges::find(languages, name);
                if (language == languages.end())
                {
                    BAM_LOG(Error, "Engine", "Localization", "Unknown language '{}' in {}.", name, Engine::Unicode::WideToUtf8(path.wstring()));
                    return EResult::Fail;
                }
                for (const auto& [key, value] : entries)
                    if (!add(path, static_cast<std::size_t>(language - languages.begin()), key, value)) return EResult::Fail;
            }
        }
    }

    m_ProjectLocalizationData.swap(projectData);
    m_ProjectCatalogFiles.swap(files);
    m_ReverseMapDirty = true;
    return EResult::Success;
}
