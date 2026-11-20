#include "PanelPluginGenerator.h"
#include "PluginDiscovery.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct TemporaryProject
    {
        std::filesystem::path Root;

        TemporaryProject()
        {
            Root = std::filesystem::temp_directory_path() /
                (L"BamPanelPluginCheck_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
            Check(std::filesystem::create_directory(Root), "temporary directory already exists");
        }

        ~TemporaryProject()
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }
    };

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        Check(file.is_open(), "cannot read generated file");
        return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }
}

int main()
{
    try
    {
        TemporaryProject project;
        const auto editor = project.Root / "Editor" / "Source" / "HotReload" / "Public";
        const auto engine = project.Root / "Engine" / "Source" / "Core" / "Module" / "Public";
        std::filesystem::create_directories(editor);
        std::filesystem::create_directories(engine);
        std::ofstream(editor / "EditorPanelPluginApi.h").put('\n');
        std::ofstream(engine / "ModuleApiExport.h").put('\n');

        Editor::PanelPluginOptions options{"Example", "My \"Panel\" 한", "1.0.0", 640, 480};
        std::filesystem::path created;
        auto invalid = options;
        invalid.Identifier = "../outside";
        Check(Editor::CreatePanelPlugin(project.Root, invalid, created) ==
            Editor::EPanelPluginCreateResult::InvalidIdentifier && created.empty(), "invalid identifier accepted");
        invalid = options;
        invalid.DisplayName = "hidden##name";
        Check(Editor::CreatePanelPlugin(project.Root, invalid, created) ==
            Editor::EPanelPluginCreateResult::InvalidName, "invalid panel name accepted");
        invalid = options;
        invalid.Width = 239;
        Check(Editor::CreatePanelPlugin(project.Root, invalid, created) ==
            Editor::EPanelPluginCreateResult::InvalidSize, "invalid panel size accepted");
        invalid = options;
        invalid.Version = "1.0/0";
        Check(Editor::CreatePanelPlugin(project.Root, invalid, created) ==
            Editor::EPanelPluginCreateResult::InvalidVersion, "invalid version accepted");

        Check(Editor::CreatePanelPlugin(project.Root, options, created) ==
            Editor::EPanelPluginCreateResult::Success, "plugin generation failed");
        Editor::PluginManifest manifest;
        Check(Editor::ReadPluginManifest(created / "plugin.json", manifest) ==
            Editor::EPluginManifestResult::Success, "generated manifest is invalid");
        Check(manifest.name == options.DisplayName && manifest.id == "user.panel.Example" &&
            manifest.modules.size() == 1 && manifest.modules[0].binary == "Binaries/Win64/UserPanel_Example.dll",
            "generated manifest metadata differs");
        const auto source = ReadFile(created / "Panel.cpp");
        Check(source.find("My \\\"Panel\\\"") != std::string::npos, "display name was not escaped");
        Check(source.find("\\xED\"\"\\x95\"\"\\x9C") != std::string::npos, "UTF-8 name was not preserved");
        Check(source.find("640.0f, 480.0f") != std::string::npos, "panel dimensions differ");
        Check(ReadFile(created / "CMakeLists.txt").find("UserPanel_Example") != std::string::npos,
            "build target is missing");
        Check(Editor::CreatePanelPlugin(project.Root, options, created) ==
            Editor::EPanelPluginCreateResult::DirectoryExists, "existing plugin was overwritten");
        Check(ReadFile(project.Root / "Plugins" / "UserPanel_Example" / "Panel.cpp") == source,
            "existing source changed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
