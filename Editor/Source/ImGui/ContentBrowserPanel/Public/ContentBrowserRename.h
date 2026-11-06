#pragma once

#include <cstddef>
#include <filesystem>

namespace Editor::ContentBrowser
{
    void DrawRenameInput(const char* id, const std::filesystem::path& path, std::filesystem::path& renamingPath,
        char* renameBuffer, std::size_t renameBufferSize, std::filesystem::path& currentDirectory);
}