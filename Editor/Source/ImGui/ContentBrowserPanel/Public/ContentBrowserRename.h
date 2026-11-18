#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace Editor::ContentBrowser
{
    enum class ERenameInputResult { Editing, Submit, Cancel };

    std::string GetRenameBaseName(const std::filesystem::path& path, bool directory);
    bool TryBuildRenamePath(const std::filesystem::path& path, bool directory, std::string_view name,
        std::filesystem::path& target);
    ERenameInputResult DrawRenameInput(const char* id, char* buffer, std::size_t bufferSize, bool& requestFocus);
}