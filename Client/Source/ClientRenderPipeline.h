#pragma once

#include "DeferredProfilePasses.h"
#include "DeferredSceneRenderer.h"
#include "ScenePostProcessSettings.h"

namespace Client
{
    class ClientRenderPipeline
    {
    public:
        EResult Initialize(const std::string& profileUri, uint32 width, uint32 height);
        EResult Resize(uint32 width, uint32 height);
        void Submit(const CameraBuffer& camera, Engine::Scene& scene, f32 dt, bool pixelPerfect);
        void Free();
        // 프로젝트 설정의 레이트레이싱 그림자 모드. Client에는 그래픽·뷰포트 덮어쓰기가 없다.
        void SetRaytracedShadowMode(Engine::ERaytracedShadowMode mode)
        {
            m_SceneRenderer.SetRaytracedShadowOverrides({ mode });
        }
        void SetRaytracedReflectionMode(Engine::ERaytracedShadowMode mode)
        {
            m_SceneRenderer.SetRaytracedReflectionOverrides({ mode });
        }
        void SetTemporalAAMode(Engine::ERaytracedShadowMode mode)
        {
            m_SceneRenderer.SetTemporalAAOverrides({ mode });
        }
        bool UsesDeferredProfile() const { return m_PresentPass != Engine::INVALID_PASS_ID; }

    private:
        Engine::DeferredProfilePasses m_Passes;
        Engine::DeferredSceneRenderer m_SceneRenderer;
        Engine::PostProcessPipeline m_PostProcess;
        Engine::ScenePostProcessSettings m_SceneSettings;
        Engine::RHIPipeline* m_PresentPipeline = nullptr;
        Engine::RenderPassID m_ForwardPass = Engine::INVALID_PASS_ID;
        Engine::RenderPassID m_UIPass = Engine::INVALID_PASS_ID;
        Engine::RenderPassID m_PresentPass = Engine::INVALID_PASS_ID;
    };
}
