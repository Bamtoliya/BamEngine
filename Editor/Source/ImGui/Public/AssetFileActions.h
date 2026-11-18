#pragma once

#include <filesystem>

namespace Editor::AssetFileActions
{
    bool IsSceneFile(const std::filesystem::path& path);
    bool IsPrefabFile(const std::filesystem::path& path);
    void Open(const std::filesystem::path& filePath);
    void RevealInContentBrowser(const std::filesystem::path& path);
}
