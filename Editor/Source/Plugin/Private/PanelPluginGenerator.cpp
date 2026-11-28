#include "PanelPluginGenerator.h"
#include "EditorPanelPluginApi.h"
#include "PluginDiscovery.h"
#include "Editor_Macros.h"
#include "Functions.h"
#include "Logger.h"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Editor
{
    namespace
    {
        bool IsIdentifier(std::string_view value)
        {
            if (value.empty() || value.size() > 48 || !((value[0] >= 'A' && value[0] <= 'Z') ||
                (value[0] >= 'a' && value[0] <= 'z')))
                return false;

            return std::all_of(value.begin(), value.end(), [](char c)
            {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '_';
            });
        }

        bool IsDisplayName(std::string_view value)
        {
            return !value.empty() && value.find_first_not_of(' ') != std::string_view::npos &&
                value.find("##") == std::string_view::npos &&
                std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; });
        }

        bool IsVersion(std::string_view value)
        {
            return !value.empty() && value.size() <= 32 && std::all_of(value.begin(), value.end(), [](char c)
            {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';
            });
        }

        std::string CppString(std::string_view value)
        {
            constexpr char hex[] = "0123456789ABCDEF";
            std::string output = "\"";
            for (const unsigned char c : value)
            {
                if (c == '"' || c == '\\')
                {
                    output += '\\';
                    output += static_cast<char>(c);
                }
                else if (c >= 0x20 && c < 0x7f)
                    output += static_cast<char>(c);
                else
                {
                    output += "\\x";
                    output += hex[c >> 4];
                    output += hex[c & 0x0f];
                    output += "\"\"";
                }
            }
            return output + "\"";
        }

        bool WriteNewFile(const std::filesystem::path& path, const std::string& content)
        {
#if defined(_WIN32)
            const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                EDITOR_LOG_ERROR("Could not create plugin file {}: Win32 error {}", Engine::Unicode::WideToUtf8(path.wstring()),
                    GetLastError());
                return false;
            }
            DWORD written = 0;
            const bool attempted = content.size() <= MAXDWORD;
            const bool writeOk = attempted && WriteFile(file, content.data(), static_cast<DWORD>(content.size()),
                &written, nullptr) != 0;
            const bool success = writeOk && written == content.size();
            const DWORD writeError = success ? ERROR_SUCCESS : (writeOk || !attempted ? ERROR_WRITE_FAULT : GetLastError());
            const bool closed = CloseHandle(file) != 0;
            const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
            if (!success || !closed)
                EDITOR_LOG_ERROR("Could not write plugin file {}: Win32 error {}", Engine::Unicode::WideToUtf8(path.wstring()),
                    closed ? writeError : closeError);
            return success && closed;
#else
            return false;
#endif
        }
    }

    EPanelPluginCreateResult CreatePanelPlugin(const std::filesystem::path& projectRoot,
        const PanelPluginOptions& options, std::filesystem::path& createdDirectory)
    {
        createdDirectory.clear();
        if (!IsIdentifier(options.Identifier))
            return EPanelPluginCreateResult::InvalidIdentifier;
        if (!IsDisplayName(options.DisplayName))
            return EPanelPluginCreateResult::InvalidName;
        if (!IsVersion(options.Version))
            return EPanelPluginCreateResult::InvalidVersion;
        if (options.Width < 240 || options.Width > 4096 || options.Height < 160 || options.Height > 4096)
            return EPanelPluginCreateResult::InvalidSize;

        std::error_code error;
        const auto root = std::filesystem::canonical(projectRoot, error);
        if (error || *PluginSdkCompatibility == '\0' ||
            !std::filesystem::is_regular_file(root / "Editor" / "Source" / "HotReload" / "Public" /
                "EditorPanelPluginApi.h", error) || error ||
            !std::filesystem::is_regular_file(root / "Engine" / "Source" / "Core" / "Module" / "Public" /
                "ModuleApiExport.h", error) || error)
            return EPanelPluginCreateResult::UnsupportedEnvironment;

        const auto target = "UserPanel_" + options.Identifier;
        PluginManifest manifest;
        manifest.manifestVersion = 1;
        manifest.id = "user.panel." + options.Identifier;
        manifest.name = options.DisplayName;
        manifest.version = options.Version;
        manifest.modules.push_back({ target, "Editor", 1, PluginSdkCompatibility,
            "Binaries/Win64/" + target + ".dll" });
        std::string json;
        if (glz::write_json(manifest, json))
        {
            EDITOR_LOG_ERROR("Could not serialize panel plugin manifest");
            return EPanelPluginCreateResult::FileSystemError;
        }

        const auto name = CppString(options.DisplayName);
        const auto panelId = CppString("Editor.User." + options.Identifier);
        const auto sdk = CppString(PluginSdkCompatibility);
        const std::string source =
            "#include \"EditorPanelPluginApi.h\"\n"
            "#include \"ModuleApiExport.h\"\n"
            "#include <imgui.h>\n"
            "#include <string_view>\n\n"
            "static_assert(std::string_view(Editor::PluginSdkCompatibility) == " + sdk +
            ", \"Build this plugin with the same compiler, architecture and configuration as the Editor\");\n\n"
            "namespace\n{\n"
            "    using namespace Editor;\n"
            "    struct Panel {};\n"
            "    void* Create(const void*, std::uint64_t) { return new Panel(); }\n"
            "    void Destroy(void* instance) { delete static_cast<Panel*>(instance); }\n"
            "    std::uint64_t StateSize(const void*) { return 0; }\n"
            "    bool CaptureState(const void*, void*, std::uint64_t size) { return size == 0; }\n"
            "    void Update(void*, float) {}\n"
            "    void Draw(void*, const PanelDrawContextV2& context)\n    {\n"
            "        ImGui::SetAllocatorFunctions(context.Allocate, context.Free, context.AllocatorData);\n"
            "        ImGui::SetCurrentContext(context.ImGui);\n"
            "        ImGui::TextUnformatted(" + name + ");\n"
            "    }\n"
            "    void SaveWorkspace(const void*, void*, SetWorkspaceFloatV2) {}\n"
            "    void RestoreWorkspace(void*, const void*, GetWorkspaceFloatV2) {}\n\n"
            "    const PanelRegistrationV2 Panels[] = {{\n"
            "        L" + panelId + ", " + name + ", " + std::to_string(options.Width) + ".0f, " +
            std::to_string(options.Height) + ".0f, 0, 1,\n"
            "        &Create, &Destroy, &StateSize, &CaptureState, &Update, &Draw, &SaveWorkspace, &RestoreWorkspace\n"
            "    }};\n"
            "    const PluginApiV2 Api{sizeof(PluginApiV2), sizeof(PanelRegistrationV2), sizeof(PanelDrawContextV2),\n"
            "        sizeof(PanelHostServicesV2), 1, Panels};\n"
            "}\n\n"
            "EXPORT_PLUGIN_API_ENTRY(BamGetPluginV2, Editor::PluginApiV2, Api)\n";
        const std::string cmake =
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(" + target + " LANGUAGES CXX)\n"
            "find_package(imgui CONFIG REQUIRED)\n"
            "add_library(" + target + " MODULE Panel.cpp)\n"
            "target_compile_features(" + target + " PRIVATE cxx_std_20)\n"
            "target_include_directories(" + target + " PRIVATE\n"
            "    \"${CMAKE_CURRENT_SOURCE_DIR}/../../Editor/Source/HotReload/Public\"\n"
            "    \"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Module/Public\")\n"
            "target_link_libraries(" + target + " PRIVATE imgui::imgui)\n"
            "if(MSVC)\n"
            "    target_compile_options(" + target + " PRIVATE /utf-8 /Zc:preprocessor)\n"
            "    target_link_options(" + target + " PRIVATE \"$<$<CONFIG:Debug>:/PDBALTPATH:%_PDB%>\")\n"
            "endif()\n"
            "set_target_properties(" + target + " PROPERTIES PREFIX \"\"\n"
            "    RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64/$<0:>\"\n"
            "    LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64/$<0:>\")\n";

        const auto plugins = root / "Plugins";
        std::filesystem::create_directories(plugins, error);
        if (error)
        {
            EDITOR_LOG_ERROR("Could not create plugin root {}: {}", Engine::Unicode::WideToUtf8(plugins.wstring()), error.message());
            return EPanelPluginCreateResult::FileSystemError;
        }
        const auto resolvedPlugins = std::filesystem::canonical(plugins, error);
        if (error || !std::filesystem::equivalent(resolvedPlugins.parent_path(), root, error) || error)
        {
            EDITOR_LOG_ERROR("Plugin root is inaccessible or outside the project: {}", Engine::Unicode::WideToUtf8(plugins.wstring()));
            return EPanelPluginCreateResult::FileSystemError;
        }

        const auto directory = resolvedPlugins / target;
        if (!std::filesystem::create_directory(directory, error))
        {
            if (error)
                EDITOR_LOG_ERROR("Could not create plugin directory {}: {}", Engine::Unicode::WideToUtf8(directory.wstring()), error.message());
            return error ? EPanelPluginCreateResult::FileSystemError : EPanelPluginCreateResult::DirectoryExists;
        }

        createdDirectory = directory;
        if (!WriteNewFile(directory / "Panel.cpp", source) ||
            !WriteNewFile(directory / "CMakeLists.txt", cmake) ||
            !WriteNewFile(directory / "plugin.json", json))
            return EPanelPluginCreateResult::FileSystemError;
        return EPanelPluginCreateResult::Success;
    }
}
