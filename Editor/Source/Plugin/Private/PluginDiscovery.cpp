#include "PluginDiscovery.h"
#include "Editor_Macros.h"
#include "Functions.h"
#include "Logger.h"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace Editor
{
    namespace
    {
        bool HasMetadataValue(std::string_view value)
        {
            return value.find_first_not_of(" \t\r\n") != std::string_view::npos &&
                value.find('\0') == std::string_view::npos;
        }
    }

    std::vector<PluginDevelopmentEntry> ReadPluginDevelopmentList(const std::filesystem::path& path)
    {
        std::vector<PluginDevelopmentEntry> entries;
        std::ifstream file(path);
        if (!file)
        {
            std::error_code error;
            if (std::filesystem::exists(path, error) || error)
                EDITOR_LOG_WARN("Cannot read editor plugin development list: {}", Engine::WStrToStr(path.wstring()));
            return entries;
        }

        std::unordered_set<std::string> targets;
        std::string line;
        while (std::getline(file, line))
        {
            if (line.ends_with('\r'))
                line.pop_back();
            if (line.empty())
                continue;

            const auto separator = line.find('|');
            if (separator == std::string::npos || separator == 0 || separator + 1 == line.size())
            {
                EDITOR_LOG_WARN("Invalid editor plugin development entry: {}", line);
                continue;
            }

            auto target = line.substr(0, separator);
            const std::string_view sourceText = std::string_view(line).substr(separator + 1);
            if (target.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") !=
                std::string::npos || !HasMetadataValue(sourceText))
            {
                EDITOR_LOG_WARN("Invalid editor plugin target or source directory: {}", line);
                continue;
            }

            try
            {
                auto source = Engine::Utf8ToPath(sourceText);
                const std::filesystem::path parentComponent("..");
                if (source.has_root_path() || source.lexically_normal() == "." ||
                    std::find(source.begin(), source.end(), parentComponent) != source.end())
                {
                    EDITOR_LOG_WARN("Invalid relative source directory for editor plugin {}", target);
                    continue;
                }

                if (!targets.insert(target).second)
                {
                    EDITOR_LOG_WARN("Duplicate editor plugin build target: {}", target);
                    continue;
                }

                entries.push_back({ std::move(target), std::move(source) });
            }
            catch (const std::system_error& error)
            {
                EDITOR_LOG_WARN("Invalid source path for editor plugin {}: {}", target, error.what());
            }
        }

        if (!file.eof())
        {
            EDITOR_LOG_WARN("Incomplete editor plugin development list: {}", Engine::WStrToStr(path.wstring()));
            return {};
        }

        return entries;
    }

    EPluginManifestResult ReadPluginManifest(const std::filesystem::path& path, PluginManifest& manifest)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return EPluginManifestResult::ReadFailed;

        std::string json(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
        if (json.starts_with("\xEF\xBB\xBF"))
            json.erase(0, 3);

        PluginManifest candidate;
        if (glz::read_json(candidate, json))
            return EPluginManifestResult::InvalidJson;
        if (candidate.manifestVersion != 1)
            return EPluginManifestResult::UnsupportedVersion;

        if (!HasMetadataValue(candidate.id) || !HasMetadataValue(candidate.name) ||
            !HasMetadataValue(candidate.version) || (candidate.modules.empty() && !HasMetadataValue(candidate.contentRoot)))
        {
            return EPluginManifestResult::InvalidMetadata;
        }

        std::unordered_set<std::string> moduleIds;
        for (const auto& module : candidate.modules)
        {
            if (!HasMetadataValue(module.id) || (module.environment != "Editor" && module.environment != "Runtime") ||
                module.apiVersion == 0 || !HasMetadataValue(module.sdkCompatibility) ||
                !HasMetadataValue(module.binary) || !moduleIds.insert(module.id).second)
            {
                return EPluginManifestResult::InvalidMetadata;
            }
        }

        manifest = std::move(candidate);
        return EPluginManifestResult::Success;
    }
}