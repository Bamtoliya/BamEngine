#include "Engine_Includes.h"
#include "ClientRenderPipeline.h"

#include "DeferredProfileShaders.h"
#include "Material.h"
#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "RHIPipeline.h"
#include "SamplerManager.h"
#include "Shader.h"

EResult Client::ClientRenderPipeline::Initialize(const std::string& profileUri, uint32 width, uint32 height)
{
    auto& passes = RenderPassManager::Get();
    if (profileUri.empty())
    {
        m_ForwardPass = passes.RegisterRenderPass(L"Client Forward Pass", {}, L"",
            ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
            ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
            vec4(0.08f, 0.12f, 0.18f, 1.0f), 0, ERenderSortType::FrontToBack,
            ERenderPassType::Forward, EBlendMode::Forward | EBlendMode::Opaque | EBlendMode::Masked);
        if (m_ForwardPass != INVALID_PASS_ID)
            BAM_LOG(Info, "Client", "Rendering", "Using legacy Forward pipeline because RenderPipelineProfilePath is empty.");
        return m_ForwardPass == INVALID_PASS_ID ? EResult::Fail : EResult::Success;
    }

    RenderPipelineProfile profile;
    const auto fail = [&](std::string_view field, std::string_view message)
    {
        BAM_LOG(Error, "Client", "Rendering", "Render profile '{}', field '{}': {}", profileUri, field, message);
        Free();
        return EResult::Fail;
    };
    auto result = LoadRenderPipelineProfile(PlatformPath::ResolveUri(Utf8ToPath(profileUri).wstring()), profile);
    if (result)
        result = ValidateDeferredProfile(profile);
    if (!result)
        return fail(result.Field, result.Message);

    auto& resources = ResourceManager::Get();
    const auto dxilRoot = PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil";
    for (const auto& shader : GetDeferredProfileShaders(profile))
    {
        ShaderDesc desc{};
        desc.Key = shader.Key;
        desc.Path = (dxilRoot / shader.DxilFile).wstring();
        desc.shaderType = shader.Type;
        const auto handle = resources.LoadResource<Shader>(&desc);
        if (!handle.Get() || !handle->GetRHIShader() || handle->GetRHIShader()->GetRawShaderBytecode().empty())
            return fail(WStrToStr(desc.Key), "Could not register the required DXIL shader: " + WStrToStr(desc.Path));
    }

    result = m_Passes.Initialize(profile, L"Client", width, height);
    if (!result)
        return fail(result.Field, result.Message);
    if (!m_SceneRenderer.Initialize())
        return fail("DeferredLighting", "Could not create the lighting pipeline.");

    if (m_Passes.UsesScenePostProcess())
    {
        m_PostProcess.Initialize(L"Client");
        m_PostProcess.PrepareResources(width, height);
        if (IsFailure(m_PostProcess.Resize(width, height)) || !m_SceneSettings.CaptureDefaults(m_PostProcess))
            return fail("ScenePostProcess", "Could not prepare scene post-processing.");
    }

    MaterialDesc uiMaterial{};
    uiMaterial.Key = L"Resources/Material/UIMaterial.bammat.json";
    uiMaterial.Path = PlatformPath::ResolveUri(L"Engine://Material/UIMaterial.bammat.json").wstring();
    if (!resources.LoadResource<Material>(&uiMaterial).Get())
        return fail("UI", "Could not load the built-in UI material.");

    RHIPipelineDesc present{};
    present.vertexShader = resources.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
    present.pixelShader = resources.GetResourceHandle<Shader>(L"ClientPresentPS")->GetRHIShader();
    present.colorAttachmentCount = 1;
    present.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
    present.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
    present.depthStencilState.depthTestEnable = false;
    present.depthStencilState.depthWriteEnable = false;
    present.topology = ETopology::TriangleList;
    present.cullMode = ECullMode::None;
    m_PresentPipeline = PipelineManager::Get().GetOrCreatePipeline(present);
    if (!m_PresentPipeline || !m_PresentPipeline->GetNativeHandle())
        return fail("Output", "Could not create the back-buffer presentation pipeline.");

    RenderPassDesc ui{};
    ui.name = L"Client UI";
    ui.priority = 200;
    ui.passType = ERenderPassType::UI;
    ui.renderTargetNames = { m_Passes.GetColorTarget() };
    ui.sortType = ERenderSortType::FrontToBack;
    ui.acceptedBlendModes = EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend |
        EBlendMode::Additive | EBlendMode::NonPremultiplied;
    m_UIPass = passes.RegisterRenderPass(std::move(ui));

    RenderPassDesc output{};
    output.name = L"Client Present";
    output.priority = 300;
    output.loadOperation = ERenderPassLoadOperation::RPLO_Clear;
    output.acceptedBlendModes = EBlendMode::None;
    m_PresentPass = passes.RegisterRenderPass(std::move(output));
    if (m_UIPass == INVALID_PASS_ID || m_PresentPass == INVALID_PASS_ID)
        return fail("Output", "Could not register the Client UI or presentation pass.");

    BAM_LOG(Info, "Client", "Rendering", "Loaded render profile '{}' with {} passes", profileUri, profile.Passes.size());
    return EResult::Success;
}

EResult Client::ClientRenderPipeline::Resize(uint32 width, uint32 height)
{
    if (!UsesDeferredProfile())
        return EResult::Success;
    const auto result = m_Passes.Resize(width, height);
    if (IsFailure(result) || !m_Passes.UsesScenePostProcess())
        return result;
    return m_PostProcess.Resize(width, height);
}

void Client::ClientRenderPipeline::Submit(const CameraBuffer& camera, Engine::Scene& scene, f32 dt, bool pixelPerfect)
{
    auto& renderer = Renderer::Get();
    auto& passes = RenderPassManager::Get();
    if (!UsesDeferredProfile())
    {
        renderer.RegisterViewportCamera(camera, passes.GetRenderPassByID(m_ForwardPass));
        return;
    }

    const auto geometry = m_Passes.GetPass("GBuffer");
    passes.GetRenderPassByID(geometry)->SetPointSpriteSampling(pixelPerfect);
    m_SceneRenderer.Submit(camera, &scene, {
        m_Passes.GetPass("DirectionalShadow"), m_Passes.GetPass("PointShadow"), geometry,
        m_Passes.GetPass("DeferredLighting"), m_Passes.GetPass("ForwardOpaque"), m_Passes.GetPass("ForwardTransparent"),
        m_Passes.GetLightingInputs(), m_Passes.GetPointShadowInput()
    });

    auto color = m_Passes.GetColorTarget();
    if (m_Passes.UsesScenePostProcess() && m_SceneSettings.Apply(m_PostProcess, &scene))
        color = m_PostProcess.Execute(dt, color);

    auto* ui = passes.GetRenderPassByID(m_UIPass);
    ui->SetColorAttachments({ color });
    renderer.RegisterViewportCamera(ui);
    renderer.SubmitCustomCommand([pipeline = m_PresentPipeline, color = std::move(color)](f32, RenderPass*) -> EResult
    {
        auto* target = RenderTargetManager::Get().GetRenderTarget(color);
        auto* rhi = Renderer::Get().GetRHI();
        SamplerDesc samplerDesc{};
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = ESamplerAddressMode::Clamp;
        auto* sampler = SamplerManager::Get().GetOrCreateSampler(samplerDesc);
        if (!target || !target->GetTexture() || !sampler)
            return EResult::Fail;
        if (IsFailure(rhi->BindPipeline(pipeline)) ||
            IsFailure(rhi->BindTextureSampler(target->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS)))
            return EResult::Fail;
        return rhi->Draw(3);
    }, m_PresentPass);
}

void Client::ClientRenderPipeline::Free()
{
    if (RenderPassManager::IsCreated())
    {
        auto& passes = RenderPassManager::Get();
        for (const auto id : { m_PresentPass, m_UIPass, m_ForwardPass })
            passes.RemoveRenderPass(id);
    }
    m_PresentPass = m_UIPass = m_ForwardPass = INVALID_PASS_ID;
    m_PostProcess.Free();
    m_SceneSettings.Reset();
    m_Passes.Free();
    m_SceneRenderer.Free();
    m_PresentPipeline = nullptr;
}
