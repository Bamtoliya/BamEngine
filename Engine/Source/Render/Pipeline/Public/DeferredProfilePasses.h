#pragma once

#include "RenderPassTypes.h"
#include "RenderPipelineProfile.h"
#include <array>
#include <string_view>
#include <utility>

namespace Engine
{
    [[nodiscard]] ENGINE_API RenderPipelineProfileResult ValidateDeferredProfile(const RenderPipelineProfile& profile);

    // Owns the render passes and attachments of one deferred profile instance.
    class ENGINE_API DeferredProfilePasses
    {
    public:
        DeferredProfilePasses() = default;
        ~DeferredProfilePasses();
        DeferredProfilePasses(const DeferredProfilePasses&) = delete;
        DeferredProfilePasses& operator=(const DeferredProfilePasses&) = delete;

        [[nodiscard]] RenderPipelineProfileResult Initialize(const RenderPipelineProfile& profile,
            const std::wstring& prefix, uint32 width, uint32 height);
        void Free();
        EResult Resize(uint32 width, uint32 height);

        [[nodiscard]] RenderPassID GetPass(std::string_view type) const;
        [[nodiscard]] const std::vector<std::wstring>& GetTargets() const { return m_Targets; }
        [[nodiscard]] const std::wstring& GetColorTarget() const { return m_ColorTarget; }
        [[nodiscard]] const std::wstring& GetSceneColorCopyTarget() const { return m_SceneColorCopyTarget; }
        [[nodiscard]] const std::array<std::wstring, 6>& GetLightingInputs() const { return m_LightingInputs; }
        [[nodiscard]] const std::wstring& GetPointShadowInput() const { return m_PointShadowInput; }
        [[nodiscard]] bool UsesScenePostProcess() const { return m_UsesScenePostProcess; }
        [[nodiscard]] uint32 GetInternalWidth() const { return m_InternalWidth; }
        [[nodiscard]] uint32 GetInternalHeight() const { return m_InternalHeight; }
        [[nodiscard]] bool HasInternalResolution() const { return m_InternalWidth > 0 && m_InternalHeight > 0; }

    private:
        std::vector<std::wstring> m_Targets;
        std::vector<std::pair<std::string, RenderPassID>> m_Passes;
        std::array<std::wstring, 6> m_LightingInputs;
        std::wstring m_PointShadowInput;
        std::wstring m_ColorTarget;
        std::wstring m_SceneColorCopyTarget;
        uint32 m_InternalWidth = 0;
        uint32 m_InternalHeight = 0;
        bool m_UsesScenePostProcess = false;
    };
}
