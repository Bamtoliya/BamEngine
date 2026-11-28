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
#include "SceneTransitionManager.h"
#include "Shader.h"

namespace
{
    struct ClientPresentParams
    {
        vec4 FadeColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        f32 FadeAlpha = 0.0f;
        f32 Pad[3] = {};
    };
    static_assert(sizeof(ClientPresentParams) == 32);
}

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
    const ERHIType rhiType = Renderer::IsCreated() ? Renderer::Get().GetRHIType() : ERHIType::DirectX12;
    const auto binRoot = PlatformPath::GetEngineResourceRoot() / "Shader" / "bin";
    for (const auto& shader : GetDeferredProfileShaders(profile))
    {
        if (shader.DirectX12Only && rhiType != ERHIType::DirectX12)
            continue;
        const std::wstring stemW = std::filesystem::path(shader.DxilFile).stem().wstring();
        ShaderDesc desc{};
        desc.Key = shader.Key;
        desc.dxilPath = (binRoot / "dxil" / shader.DxilFile).wstring();
        desc.spirvPath = (binRoot / "spirv" / (stemW + L".spv")).wstring();
        desc.wgslPath = (binRoot / "wgsl" / (stemW + L".wgsl")).wstring();
        desc.Path = (rhiType == ERHIType::DirectX12) ? desc.dxilPath :
            (rhiType == ERHIType::WebGPU ? desc.wgslPath : desc.spirvPath);
        desc.shaderType = shader.Type;
        const auto handle = resources.LoadResource<Shader>(&desc);
        if (!handle.Get() || !handle->GetRHIShader() || handle->GetRHIShader()->GetRawShaderBytecode().empty())
            return fail(Engine::Unicode::WideToUtf8(desc.Key), "Could not register the required shader: " + Engine::Unicode::WideToUtf8(desc.Path));
    }

    m_UseInternalResolution = profile.InternalWidth > 0 && profile.InternalHeight > 0;
    m_IntegerUpscale = profile.IntegerUpscale;
    m_PointUpscale = profile.PointUpscale;
    m_InternalWidth = profile.InternalWidth;
    m_InternalHeight = profile.InternalHeight;
    m_OutputWidth = width;
    m_OutputHeight = height;

    const uint32 sceneWidth = m_UseInternalResolution ? m_InternalWidth : width;
    const uint32 sceneHeight = m_UseInternalResolution ? m_InternalHeight : height;

    result = m_Passes.Initialize(profile, L"Client", width, height);
    if (!result)
        return fail(result.Field, result.Message);
    if (!m_SceneRenderer.Initialize())
        return fail("DeferredLighting", "Could not create the lighting pipeline.");

    if (m_Passes.UsesScenePostProcess())
    {
        m_PostProcess.Initialize(L"Client");
        m_PostProcess.PrepareResources(sceneWidth, sceneHeight);
        if (IsFailure(m_PostProcess.Resize(sceneWidth, sceneHeight)) || !m_SceneSettings.CaptureDefaults(m_PostProcess))
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
    ui.priority = m_UseInternalResolution ? 300u : 200u;
    ui.passType = ERenderPassType::UI;
    if (m_UseInternalResolution)
    {
        ui.loadOperation = ERenderPassLoadOperation::RPLO_Load;
        ui.storeOperation = ERenderPassStoreOperation::RPSO_Store;
    }
    else
    {
        ui.renderTargetNames = { m_Passes.GetColorTarget() };
    }
    ui.sortType = ERenderSortType::FrontToBack;
    ui.acceptedBlendModes = EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend |
        EBlendMode::Additive | EBlendMode::PureAdditive | EBlendMode::NonPremultiplied;
    m_UIPass = passes.RegisterRenderPass(std::move(ui));

    RenderPassDesc output{};
    output.name = L"Client Present";
    output.priority = m_UseInternalResolution ? 200u : 300u;
    output.loadOperation = ERenderPassLoadOperation::RPLO_Clear;
    if (m_UseInternalResolution)
        output.overrideClearColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
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
    m_OutputWidth = width;
    m_OutputHeight = height;
    const auto result = m_Passes.Resize(width, height);
    if (IsFailure(result) || !m_Passes.UsesScenePostProcess())
        return result;
    const uint32 sceneWidth = m_UseInternalResolution ? m_InternalWidth : width;
    const uint32 sceneHeight = m_UseInternalResolution ? m_InternalHeight : height;
    return m_PostProcess.Resize(sceneWidth, sceneHeight);
}

void Client::ClientRenderPipeline::Submit(const CameraBuffer& camera, Engine::Scene& scene, f32 dt, bool pixelPerfect,
    const std::optional<Engine::DepthOfFieldSettings>& depthOfField)
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
    {
        // 피사계 심도·아웃라인 등이 읽는 장면 입력(G-Buffer 위치·알베도·법선)
        const auto& inputs = m_Passes.GetLightingInputs();
        m_PostProcess.SetSceneInputs({ inputs[4], inputs[0], camera, depthOfField, inputs[1] });
        color = m_PostProcess.Execute(dt, color);
    }

    auto* ui = passes.GetRenderPassByID(m_UIPass);
    if (!m_UseInternalResolution)
        ui->SetColorAttachments({ color });
    renderer.RegisterViewportCamera(ui);
    ClientPresentParams presentParams{};
    if (Engine::SceneTransitionManager::IsCreated())
    {
        const auto& transition = Engine::SceneTransitionManager::Get();
        const f32 fadeAlpha = transition.GetFadeAlpha();
        if (fadeAlpha > 0.001f)
        {
            presentParams.FadeColor = transition.GetFadeColor();
            presentParams.FadeAlpha = fadeAlpha;
        }
    }

    renderer.SubmitCustomCommand([pipeline = m_PresentPipeline, color = std::move(color),
        useInternal = m_UseInternalResolution, integerUpscale = m_IntegerUpscale, pointUpscale = m_PointUpscale,
        internalW = m_InternalWidth, internalH = m_InternalHeight,
        outputW = m_OutputWidth, outputH = m_OutputHeight, presentParams](f32, RenderPass*) -> EResult
    {
        auto* target = RenderTargetManager::Get().GetRenderTarget(color);
        auto* rhi = Renderer::Get().GetRHI();
        SamplerDesc samplerDesc{};
        if (pointUpscale)
            samplerDesc.MinFilter = samplerDesc.MagFilter = samplerDesc.MipFilter = ESamplerFilter::Point;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = ESamplerAddressMode::Clamp;
        auto* sampler = SamplerManager::Get().GetOrCreateSampler(samplerDesc);
        if (!target || !target->GetTexture() || !sampler)
            return EResult::Fail;
        if (useInternal && internalW > 0 && internalH > 0)
        {
            const uint32 outW = rhi->GetSwapChainWidth() > 0 ? rhi->GetSwapChainWidth() : outputW;
            const uint32 outH = rhi->GetSwapChainHeight() > 0 ? rhi->GetSwapChainHeight() : outputH;
            if (outW > 0 && outH > 0)
            {
                uint32 dstW = outW;
                uint32 dstH = outH;
                const uint32 intScale = integerUpscale ? std::min(outW / internalW, outH / internalH) : 0u;
                if (intScale >= 1)
                {
                    dstW = internalW * intScale;
                    dstH = internalH * intScale;
                }
                else
                {
                    const float fitScale = std::min(static_cast<float>(outW) / static_cast<float>(internalW),
                        static_cast<float>(outH) / static_cast<float>(internalH));
                    dstW = std::max<uint32>(1, static_cast<uint32>(static_cast<float>(internalW) * fitScale));
                    dstH = std::max<uint32>(1, static_cast<uint32>(static_cast<float>(internalH) * fitScale));
                }
                const int32 vpX = static_cast<int32>((outW - dstW) / 2);
                const int32 vpY = static_cast<int32>((outH - dstH) / 2);
                if (IsFailure(rhi->SetViewport(vpX, vpY, dstW, dstH)))
                    return EResult::Fail;
            }
        }
        if (IsFailure(rhi->BindPipeline(pipeline)) ||
            IsFailure(rhi->BindConstantBuffer(&presentParams, sizeof(presentParams), 2)) ||
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
    m_UseInternalResolution = false;
    m_IntegerUpscale = false;
    m_PointUpscale = false;
    m_InternalWidth = 0;
    m_InternalHeight = 0;
    m_OutputWidth = 0;
    m_OutputHeight = 0;
}
