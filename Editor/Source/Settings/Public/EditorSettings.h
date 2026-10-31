#pragma once

#include <reflection/Annotations.h>

#include <cstdint>
#include <string>
#include <map>
#include <cmath>
#include <array>

namespace Editor
{
    struct EditorApplicationState
    {
        bool HasWindow = false;
        bool HasWindowSize = false;
        std::int32_t WindowWidth = 0;
        std::int32_t WindowHeight = 0;
        bool WindowMinimized = false;
        bool WindowMaximized = false;
        bool WindowFullscreen = false;

        bool HasTargetFPS = false;
        std::uint32_t TargetFPS = 0;
    };

    STRUCT()
    struct EditorPreferences
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;
        static constexpr float MinUIScale = 0.5f;
        static constexpr float MaxUIScale = 3.0f;
        static constexpr std::int32_t MaxTargetFPS = 1000;

        PROPERTY()
        std::uint32_t SchemaVersion = CurrentSchemaVersion;

        PROPERTY(META("Optional", true))
        std::string Language = "Korean";

        PROPERTY(META("Optional", true))
        float UIScale = 1.0f;

        PROPERTY(META("Optional", true))
        std::int32_t TargetFPS = 0;

        PROPERTY(META("Optional", true))
        bool VSync = true;

        [[nodiscard]] bool HasValidValues() const
        {
            return SchemaVersion == CurrentSchemaVersion && (Language == "English" || Language == "Korean") &&
                std::isfinite(UIScale) && UIScale >= MinUIScale && UIScale <= MaxUIScale &&
                TargetFPS >= 0 && TargetFPS <= MaxTargetFPS;
        }
    };

    STRUCT()
    struct ViewportWorkspace
    {
        PROPERTY(META("Optional", true))
        std::int32_t ResolutionMode = 0;

        PROPERTY(META("Optional", true))
        std::int32_t AspectRatioMode = 0;

        PROPERTY(META("Optional", true))
        std::int32_t PresetWidth = 1920;

        PROPERTY(META("Optional", true))
        std::int32_t PresetHeight = 1080;

        PROPERTY(META("Optional", true))
        std::int32_t CustomWidth = 1920;

        PROPERTY(META("Optional", true))
        std::int32_t CustomHeight = 1080;

        PROPERTY(META("Optional", true))
        bool HasCamera = false;

        PROPERTY(META("Optional", true))
        std::array<float, 3> CameraPosition{};

        PROPERTY(META("Optional", true))
        std::array<float, 3> CameraRotation{};

        PROPERTY(META("Optional", true))
        bool Perspective = true;

        PROPERTY(META("Optional", true))
        float FOV = 60.0f;

        PROPERTY(META("Optional", true))
        float OrthographicSize = 720.0f;

        PROPERTY(META("Optional", true))
        float CameraSpeed = 100.0f;

        PROPERTY(META("Optional", true))
        std::map<std::wstring, bool> RenderPassStates;
    };

    STRUCT()
    struct EditorWorkspace
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        PROPERTY()
        std::uint32_t SchemaVersion = CurrentSchemaVersion;

        PROPERTY(META("Optional", true))
        std::int32_t WindowWidth = 1280;

        PROPERTY(META("Optional", true))
        std::int32_t WindowHeight = 720;

        PROPERTY(META("Optional", true))
        bool WindowMaximized = false;

        PROPERTY(META("Optional", true))
        bool WindowFullscreen = false;

        PROPERTY(META("Optional", true))
        std::string LastScenePath;

        [[nodiscard]] static constexpr bool HasValidWindowSize(std::int32_t width, std::int32_t height)
        {
            return width >= 320 && width <= 16384 && height >= 240 && height <= 16384;
        }

        [[nodiscard]] bool HasValidWindowSize() const
        {
            return HasValidWindowSize(WindowWidth, WindowHeight);
        }

        PROPERTY(META("Optional", true))
        std::map<std::wstring, bool> PanelOpenStates;

        PROPERTY(META("Optional", true))
        bool DisplaySettingsOpen = false;

        PROPERTY(META("Optional", true))
        std::string ImGuiLayout;

        PROPERTY(META("Optional", true))
        std::string ContentBrowserDirectory;

        PROPERTY(META("Optional", true))
        float ContentBrowserThumbnailSize = 64.0f;

        PROPERTY(META("Optional", true))
        std::map<std::wstring, ViewportWorkspace> ViewportStates;
    };

    struct EditorWindowRequest
    {
        std::int32_t Width = 1280;
        std::int32_t Height = 720;
        bool Fullscreen = false;
        bool Maximized = false;

        [[nodiscard]] bool HasValidValues() const
        {
            return Fullscreen || EditorWorkspace::HasValidWindowSize(Width, Height);
        }
    };
}