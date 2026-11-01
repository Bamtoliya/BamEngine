// Engine/Source/Core/Config/Public/Config.h

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
        std::string AssetDirectory = "Resources";

        PROPERTY()
        std::string StartupManifestPath = "Resources/Config/Startup/StartupManifest.json";

        PROPERTY(META("Optional", true))
        std::string StartupScenePath;

        PROPERTY(META("Optional", true))
        std::string ShaderSourceDirectory = "Resources/Shader/hlsl";
    };
}