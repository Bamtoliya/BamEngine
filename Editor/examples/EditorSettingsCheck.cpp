#include "ConfigManager.h"
#include "BamEditorSettingsCheck.gen.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    namespace fs = std::filesystem;

    void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct ScratchDirectory
    {
        fs::path Path;

        explicit ScratchDirectory(const fs::path& base)
        {
            fs::create_directories(base);
            const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

            for (unsigned attempt = 0; attempt < 64; ++attempt)
            {
                auto candidate = base / ("run-" + stamp + "-" + std::to_string(attempt));

                if (fs::create_directory(candidate))
                {
                    Path = std::move(candidate);
                    return;
                }
            }

            throw std::runtime_error("Could not create an isolated test directory.");
        }

        ScratchDirectory(const ScratchDirectory&) = delete;
        ScratchDirectory& operator=(const ScratchDirectory&) = delete;

        ~ScratchDirectory()
        {
            if (!Path.empty())
            {
                std::error_code ignored;
                fs::remove_all(Path, ignored);
            }
        }
    };

    void WriteText(const fs::path& path, std::string_view text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        Check(static_cast<bool>(output), "Could not open a fixture for writing.");
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        Check(static_cast<bool>(output), "Could not finish writing a fixture.");
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        Check(static_cast<bool>(input), "Could not open a fixture for reading.");
        std::string text{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
        Check(!input.bad(), "Could not read fixture bytes.");
        return text;
    }

    bool SamePreferences(const Editor::EditorPreferences& left, const Editor::EditorPreferences& right)
    {
        return left.SchemaVersion == right.SchemaVersion && left.Language == right.Language &&
            left.UIScale == right.UIScale && left.TargetFPS == right.TargetFPS && left.VSync == right.VSync;
    }

    void CheckPreferences(const reflection::Registry& registry, const fs::path& root)
    {
        const auto path = root / "nested" / "preferences.json";
        Editor::EditorPreferences expected;
        expected.Language = "English";
        expected.UIScale = 1.25f;
        expected.TargetFPS = 144;
        expected.VSync = false;

        Check(static_cast<bool>(Engine::ConfigManager::Save(expected, path, registry)),
            "Preferences save failed.");

        Editor::EditorPreferences loaded;
        auto result = Engine::ConfigManager::Load(loaded, path, registry);

        Check(result.Status == Engine::EConfigLoadStatus::Loaded && SamePreferences(loaded, expected),
            "Preferences round trip changed values.");

        expected.UIScale = 1.5f;
        expected.TargetFPS = 60;
        expected.VSync = true;

        Check(static_cast<bool>(Engine::ConfigManager::Save(expected, path, registry)),
            "Replacing an existing settings file failed.");

        result = Engine::ConfigManager::Load(loaded, path, registry);

        Check(result.Status == Engine::EConfigLoadStatus::Loaded && SamePreferences(loaded, expected),
            "Reloading replaced preferences returned old values.");

        auto preserved = expected;
        result = Engine::ConfigManager::Load(preserved, root / "missing.json", registry);

        Check(result.Status == Engine::EConfigLoadStatus::NotFound && SamePreferences(preserved, expected),
            "A missing file changed the destination.");

        const std::array<std::string_view, 2> invalidDocuments = {
            "{",
            R"({"Settings":{"SchemaVersion":1,"Language":"Korean","UIScale":2,"TargetFPS":"invalid"}})"
        };

        for (const auto document : invalidDocuments)
        {
            const auto invalidPath = root / "invalid.json";
            WriteText(invalidPath, document);
            const auto before = ReadText(invalidPath);
            preserved = expected;

            result = Engine::ConfigManager::Load(preserved, invalidPath, registry);

            Check(result.Status == Engine::EConfigLoadStatus::Failed &&
                SamePreferences(preserved, expected) && ReadText(invalidPath) == before,
                "A rejected document changed the destination or source file.");
        }

        const auto legacyPath = root / "legacy.json";
        WriteText(legacyPath, R"({"Settings":{"SchemaVersion":1}})");
        Editor::EditorPreferences legacy;
        result = Engine::ConfigManager::Load(legacy, legacyPath, registry);

        Check(result.Status == Engine::EConfigLoadStatus::Loaded &&
            SamePreferences(legacy, Editor::EditorPreferences{}),
            "Missing optional fields did not retain defaults.");

        const auto futurePath = root / "future.json";
        WriteText(futurePath, R"({"Settings":{"SchemaVersion":999}})");
        Editor::EditorPreferences future;
        result = Engine::ConfigManager::Load(future, futurePath, registry);

        Check(result.Status == Engine::EConfigLoadStatus::Loaded && !future.HasValidValues(),
            "Schema validation did not reject a future preferences version.");
    }

    void CheckWorkspace(const reflection::Registry& registry, const fs::path& root)
    {
        Editor::EditorWorkspace expected;
        expected.WindowWidth = 1600;
        expected.WindowHeight = 900;
        expected.DisplaySettingsOpen = true;
        expected.PanelOpenStates = { { L"GraphicsSettings", true }, { L"Graph", false } };
        expected.ImGuiLayout = "[Window][Graphics###ToolBar.DisplaySettings]\nPos=20,30\n";
        expected.LastScenePath = "Resources/Scenes/검사.json";
        expected.ContentBrowserDirectory = "Resources/Textures";
        expected.ContentBrowserThumbnailSize = 96.0f;

        auto& scene = expected.ViewportStates[L"Scene View"];
        scene.ResolutionMode = 2;
        scene.AspectRatioMode = 1;
        scene.CustomWidth = 1600;
        scene.CustomHeight = 900;
        scene.HasCamera = true;
        scene.CameraPosition = { 1.0f, 2.0f, 3.0f };
        scene.CameraRotation = { 15.0f, 45.0f, 0.0f };
        scene.Perspective = false;
        scene.FOV = 75.0f;
        scene.OrthographicSize = 128.0f;
        scene.CameraSpeed = 200.0f;
        scene.RenderPassStates = { { L"Shadow", false }, { L"Geometry", true } };

        auto& game = expected.ViewportStates[L"Game View"];
        game.ResolutionMode = 1;
        game.PresetWidth = 1280;
        game.PresetHeight = 720;

        const auto path = root / "workspace.json";
        Check(static_cast<bool>(Engine::ConfigManager::Save(expected, path, registry)),
            "Workspace save failed.");

        Editor::EditorWorkspace loaded;
        const auto result = Engine::ConfigManager::Load(loaded, path, registry);

        Check(result.Status == Engine::EConfigLoadStatus::Loaded, "Workspace load failed.");
        Check(loaded.SchemaVersion == expected.SchemaVersion && loaded.HasValidWindowSize() &&
            loaded.WindowWidth == expected.WindowWidth && loaded.WindowHeight == expected.WindowHeight,
            "Workspace window settings changed.");

        Check(loaded.DisplaySettingsOpen && loaded.PanelOpenStates == expected.PanelOpenStates &&
            loaded.ImGuiLayout == expected.ImGuiLayout && loaded.LastScenePath == expected.LastScenePath &&
            loaded.ContentBrowserDirectory == expected.ContentBrowserDirectory &&
            loaded.ContentBrowserThumbnailSize == expected.ContentBrowserThumbnailSize,
            "Workspace panel, docking, or browser settings changed.");

        Check(loaded.ViewportStates.size() == 2, "Viewport entries were lost.");
        const auto& restored = loaded.ViewportStates.at(L"Scene View");

        Check(restored.ResolutionMode == scene.ResolutionMode &&
            restored.AspectRatioMode == scene.AspectRatioMode &&
            restored.CustomWidth == scene.CustomWidth && restored.CustomHeight == scene.CustomHeight,
            "Scene viewport resolution changed.");

        Check(restored.HasCamera && restored.CameraPosition == scene.CameraPosition &&
            restored.CameraRotation == scene.CameraRotation && restored.Perspective == scene.Perspective &&
            restored.FOV == scene.FOV && restored.OrthographicSize == scene.OrthographicSize &&
            restored.CameraSpeed == scene.CameraSpeed && restored.RenderPassStates == scene.RenderPassStates,
            "Scene viewport camera or render-pass settings changed.");

        const auto& restoredGame = loaded.ViewportStates.at(L"Game View");
        Check(restoredGame.ResolutionMode == game.ResolutionMode &&
            restoredGame.PresetWidth == game.PresetWidth && restoredGame.PresetHeight == game.PresetHeight,
            "Game viewport preset changed.");
    }

    void CheckSaveFailures(const reflection::Registry& registry, const fs::path& root)
    {
        const auto path = root / "nested" / "preferences.json";
        const auto before = ReadText(path);
        Editor::EditorPreferences preferences;

        auto invalid = preferences;
        invalid.UIScale = std::numeric_limits<float>::quiet_NaN();
        const auto invalidResult = Engine::ConfigManager::Save(invalid, path, registry);

        Check(!invalidResult && invalidResult.Error.Code == archive::ArchiveErrorCode::InvalidValue &&
            ReadText(path) == before, "Serialization failure replaced an existing file.");

        reflection::Registry emptyRegistry;
        const auto unsupported = Engine::ConfigManager::Save(preferences, path, emptyRegistry);

        Check(!unsupported && unsupported.Error.Code == archive::ArchiveErrorCode::UnsupportedType &&
            ReadText(path) == before, "Missing registration replaced an existing file.");

        const auto blockedParent = root / "blocked-parent";
        WriteText(blockedParent, "parent sentinel");
        const auto blocked = Engine::ConfigManager::Save(preferences, blockedParent / "settings.json", registry);

        Check(!blocked && blocked.FileError && ReadText(blockedParent) == "parent sentinel",
            "A parent-path failure damaged existing data.");

        const auto directoryTarget = root / "directory-target.json";
        fs::create_directory(directoryTarget);
        WriteText(directoryTarget / "marker.txt", "directory sentinel");
        const auto replacement = Engine::ConfigManager::Save(preferences, directoryTarget, registry);

        Check(!replacement && replacement.FileError && fs::is_directory(directoryTarget) &&
            ReadText(directoryTarget / "marker.txt") == "directory sentinel",
            "A failed final replacement damaged the destination.");

        Check(ReadText(path) == before, "Save-failure checks changed the original preferences.");

        for (const auto& entry : fs::recursive_directory_iterator(root))
        {
            Check(!entry.path().filename().string().starts_with(".bamconfig-save-"),
                "A settings save left a temporary directory behind.");
        }
    }
}

int main(int argc, char* argv[])
{
    try
    {
        Check(argc == 2, "Usage: EditorSettingsCheck <test-output-directory>");
        ScratchDirectory scratch{fs::path{argv[1]}};

        reflection::Registry registry;
        reflection::RegistrationResult registration;
        Check(reflection_generated::Register_BamEditorSettingsCheck(registry, registration),
            "Settings reflection registration failed.");

        CheckPreferences(registry, scratch.Path);
        CheckWorkspace(registry, scratch.Path);
        CheckSaveFailures(registry, scratch.Path);

        std::cout << "Editor settings persistence checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}