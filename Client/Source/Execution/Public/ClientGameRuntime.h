#pragma once

#include "ClientRenderPipeline.h"
#include "Config.h"
#include "Runtime.h"

#include <filesystem>
#include <functional>

namespace Client
{
    class ClientGameRuntime final
    {
    public:
        ClientGameRuntime() = default;
        ClientGameRuntime(const ClientGameRuntime&) = delete;
        ClientGameRuntime& operator=(const ClientGameRuntime&) = delete;
        ~ClientGameRuntime();

        Engine::EResult LoadProject(const std::filesystem::path& projectRoot);
        Engine::EResult Initialize(const Engine::RendererDesc& rendererDesc,
            const std::function<void()>& loadModules);
        Engine::EResult RunFrame(Engine::uint32 width, Engine::uint32 height, bool inputFocused, bool isMinimized);
        void Free();

        const Engine::ProjectSettings& GetProjectSettings() const { return m_ProjectSettings; }

    private:
        Engine::EResult LoadProjectSettings();
        Engine::EResult InitializeSystems();
        Engine::EResult LoadStartupScene();
        void SubmitRenderPipeline(Engine::f32 dt, Engine::uint32 width, Engine::uint32 height);
        bool ValidateStartupGraphs();

        Engine::Runtime* m_Runtime = nullptr;
        std::filesystem::path m_ProjectRoot;
        Engine::ProjectSettings m_ProjectSettings;
        ClientRenderPipeline m_RenderPipeline;
        bool m_ProjectLoaded = false;
    };
}
