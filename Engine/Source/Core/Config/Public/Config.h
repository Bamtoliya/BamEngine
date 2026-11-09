#pragma once

#include <reflection/Annotations.h>
#include <cstdint>
#include <string>

namespace Engine
{
    STRUCT()
        struct ProjectSettings
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        PROPERTY()
        std::uint32_t SchemaVersion = CurrentSchemaVersion;

        PROPERTY(META("Optional", true))
        std::string ProjectName = "Untitled";

        PROPERTY(META("Optional", true))
        std::string AssetDirectory = "Assets";

        PROPERTY(META("Optional", true))
        std::string EngineResourceDirectory = "Engine/Resources";

        PROPERTY()
        std::string StartupManifestPath = "Config/Startup/StartupManifest.json";

        PROPERTY(META("Optional", true))
        std::string StartupScenePath = "Scenes/RaytracingScene.bamscene.json";

        PROPERTY(META("Optional", true))
        std::string ShaderSourceDirectory = "Engine/Resources/Shader/hlsl";
    };
}
