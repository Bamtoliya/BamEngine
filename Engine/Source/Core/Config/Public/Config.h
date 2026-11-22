#pragma once

#include <reflection/Annotations.h>
#include <cstdint>
#include <string>
#include <vector>

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
        std::string WindowTitle;

        PROPERTY(META("Optional", true))
        std::string AssetDirectory = "Assets";

        PROPERTY(META("Optional", true))
        std::string EngineResourceDirectory = "Engine/Resources";

        PROPERTY()
        std::string StartupManifestPath = "Config/Startup/StartupManifest.json";

        PROPERTY(META("Optional", true))
        std::string StartupScenePath = "Scenes/RaytracingScene.bamscene.json";

        PROPERTY(META("Optional", true))
        std::string RequiredGameplayActionsScene;

        PROPERTY(META("Optional", true))
        std::vector<std::string> RequiredGameplayActions;

        PROPERTY(META("Optional", true))
        std::string RenderPipelineProfilePath;

        // 레이트레이싱 그림자: 0 조명 설정 따름, 1 켜기, 2 끄기 (Engine::ERaytracedShadowMode)
        PROPERTY(META("Optional", true))
        std::int32_t RaytracedShadowMode = 0;

        // 레이트레이싱 반사: 0·2 끄기, 1 켜기 (Engine::ERaytracedShadowMode)
        PROPERTY(META("Optional", true))
        std::int32_t RaytracedReflectionMode = 0;

        // 스트리밍하는 메시와 텍스처의 GPU 메모리 한도(MB)
        PROPERTY(META("Optional", true))
        std::int32_t ResourceStreamingBudgetMB = 512;

        PROPERTY(META("Optional", true))
        std::string ShaderSourceDirectory = "Engine/Resources/Shader/hlsl";
    };
}
