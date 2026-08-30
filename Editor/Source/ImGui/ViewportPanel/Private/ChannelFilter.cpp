#pragma once

#include "ChannelFilter.h"
#include "PipelineManager.h"
#include "ResourceManager.h"
#include "RenderTargetManager.h"


#pragma region Constructor & Destructor
void ChannelFilter::Initialize(const wstring& ownerPrefix)
{
	const wstring prefix = ownerPrefix.empty() ? L"Viewport" : ownerPrefix;
	m_ChannelPreviewRTName = prefix + L"_ChannelPreviewRT";

	ResourceManager& rm = ResourceManager::Get();
    auto createPipe = [&](const wstring& psName) -> Engine::RHIPipeline*
        {
            RHIPipelineDesc pd = {};
            pd.pipelineType = EPipelineType::Graphics;
            pd.vertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
            pd.pixelShader = rm.GetResourceHandle<Shader>(psName)->GetRHIShader();
            pd.colorAttachmentCount = 1;
            pd.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
            pd.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
            pd.depthStencilState.depthTestEnable = false;
            pd.depthStencilState.depthWriteEnable = false;
            pd.topology = ETopology::TriangleList;
            pd.cullMode = ECullMode::None;
            pd.blendState = Engine::BlendState{};
            return PipelineManager::Get().GetOrCreatePipeline(pd);
        };

    m_ChannelPreviewPipeline = createPipe(L"ViewportChannelPS");

    m_ChannelPreviewPassID = RenderPassManager::Get().RegisterRenderPass(
        prefix + L"_ChannelPreviewPass", {}, L"",
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.0f, 0.0f, 0.0f, 1.0f), 400, ERenderSortType::None, ERenderPassType::Custom);
}


void ChannelFilter::Free()
{
    m_ChannelPreviewPassID = INVALID_PASS_ID;
    m_ChannelPreviewPipeline = nullptr;
    m_ChannelPreviewRTName.clear();
    m_ChannelView = EViewportChannelView::RGBA;
}
#pragma endregion

void ChannelFilter::EnsureChannelPreviewRT(const wstring& sourceRTName)
{
    if (sourceRTName.empty() || m_ChannelPreviewRTName.empty())
        return;

    auto& rtMgr = RenderTargetManager::Get();
    RenderTarget* src = rtMgr.GetRenderTarget(sourceRTName);
    if (!src)
        return;

    RenderTarget* dst = rtMgr.GetRenderTarget(m_ChannelPreviewRTName);
    if (!dst)
    {
        RenderTargetDesc desc = {};
        desc.format = ETextureFormat::R8G8B8A8_UNORM;
        desc.usage = ETextureUsage::RenderTarget | ETextureUsage::Sampler;
        desc.bindFlag = ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource;
        desc.type = ERenderTargetType::Color;
        desc.dimension = ETextureDimension::Texture2D;
        desc.width = src->GetWidth();
        desc.height = src->GetHeight();
        desc.clearColor = vec4(0.f, 0.f, 0.f, 1.f);
        desc.name = m_ChannelPreviewRTName;
        rtMgr.CreateRenderTarget(&desc);
        return;
    }

    if (dst->GetWidth() != src->GetWidth() || dst->GetHeight() != src->GetHeight())
        dst->Resize(src->GetWidth(), src->GetHeight());
}

void ChannelFilter::Resize(const wstring& sourceRTName)
{
    EnsureChannelPreviewRT(sourceRTName);
}

void ChannelFilter::SubmitChannelPreviewPass(const wstring& sourceRTName, wstring& outRTName)
{
    outRTName = sourceRTName;

    if (sourceRTName.empty())
        return;
    if (m_ChannelView == EViewportChannelView::RGBA)
        return;
    if (!IsReady())
        return;

    EnsureChannelPreviewRT(sourceRTName);
    RenderTarget* dst = GetChannelPreviewRT();
    if (!dst)
        return;

    RenderPass* channelPass = RenderPassManager::Get().GetRenderPassByID(m_ChannelPreviewPassID);
    if (!channelPass)
        return;
    channelPass->SetColorAttachments({ m_ChannelPreviewRTName });

    const wstring capturedRead = sourceRTName;
    const uint32 capturedFlags = static_cast<uint32>(m_ChannelView);

    Engine::Renderer::Get().SubmitCustomCommand(
        [this, capturedRead, capturedFlags](f32 dt, Engine::RenderPass* pass) -> EResult
        {
            auto* src = Engine::RenderTargetManager::Get().GetRenderTarget(capturedRead);
            if (!src || !src->GetTexture())
                return EResult::Fail;

            auto* rhi = Engine::Renderer::Get().GetRHI();
            ChannelViewData channelData = { capturedFlags };

            rhi->BindConstantBuffer(&channelData, sizeof(ChannelViewData), 0, EShaderType::Pixel);
            rhi->BindTextureSampler(src->GetTexture(), Engine::SamplerManager::Get().GetDefaultSampler(), 0);
            rhi->BindPipeline(m_ChannelPreviewPipeline);
            return rhi->Draw(3);
        },
        m_ChannelPreviewPassID);

    outRTName = m_ChannelPreviewRTName;
}

RenderTarget* ChannelFilter::GetChannelPreviewRT() const
{
	return Engine::RenderTargetManager::Get().GetRenderTarget(m_ChannelPreviewRTName);
}