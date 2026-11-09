#pragma once

#include "ChannelFilter.h"
#include "EditorUI.h"
#include <cmath>
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

EResult ChannelFilter::EnsureChannelPreviewRT(const wstring& sourceRTName)
{
    if (sourceRTName.empty() || m_ChannelPreviewRTName.empty())
        return EResult::InvalidArgument;

    auto& rtMgr = RenderTargetManager::Get();
    auto* source = rtMgr.GetRenderTarget(sourceRTName);

    if (!source || !source->GetTexture())
        return EResult::Fail;

    if (!CanSampleAsTexture2D(source->GetTexture()))
        return EResult::NotImplemented;

    auto* target = rtMgr.GetRenderTarget(m_ChannelPreviewRTName);

    if (!target)
    {
        RenderTargetDesc desc{};
        desc.textureDesc.format = ETextureFormat::R8G8B8A8_UNORM;
        desc.textureDesc.usage = ETextureUsage::RenderTarget | ETextureUsage::Sampler;
        desc.textureDesc.width = source->GetWidth();
        desc.textureDesc.height = source->GetHeight();
        desc.textureDesc.clearColor = vec4(0.f, 0.f, 0.f, 1.f);
        desc.name = m_ChannelPreviewRTName;

        return rtMgr.CreateRenderTarget(&desc) ? EResult::Success : EResult::Fail;
    }

    return target->Resize(source->GetWidth(), source->GetHeight());
}

EResult ChannelFilter::Resize(const wstring& sourceRTName)
{
    const auto result = EnsureChannelPreviewRT(sourceRTName);
    return result == EResult::NotImplemented ? EResult::Success : result;
}

void ChannelFilter::SubmitChannelPreviewPass(const wstring& sourceRTName, wstring& outRTName)
{
    outRTName = sourceRTName;

    if (sourceRTName.empty() || sourceRTName == m_ChannelPreviewRTName)
        return;

    if (m_ChannelView == EViewportChannelView::RGBA)
        return;
    if (!IsReady())
        return;

    if (EnsureChannelPreviewRT(sourceRTName) != EResult::Success)
        return;

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
            auto* texture = src ? src->GetTexture() : nullptr;
            if (!CanSampleAsTexture2D(texture))
                return EResult::Success;

            auto* rhi = Engine::Renderer::Get().GetRHI();
            ChannelViewData channelData = { capturedFlags };

            if (IsFailure(rhi->BindPipeline(m_ChannelPreviewPipeline)))
                return EResult::Fail;

            if (IsFailure(rhi->BindConstantBuffer(&channelData, sizeof(channelData), 2)))
                return EResult::Fail;

            auto* sampler = Engine::SamplerManager::Get().GetDefaultSampler();
            if (IsFailure(rhi->BindTextureSampler(texture, sampler, MAX_MATERIAL_TEXTURE_SLOTS)))
                return EResult::Fail;

            return rhi->Draw(3);
        },
        m_ChannelPreviewPassID);

    outRTName = m_ChannelPreviewRTName;
}

RenderTarget* ChannelFilter::GetChannelPreviewRT() const
{
	return Engine::RenderTargetManager::Get().GetRenderTarget(m_ChannelPreviewRTName);
}

bool ChannelFilter::CanSampleAsTexture2D(const Engine::RHITexture* texture)
{
    return texture &&
        texture->GetDimension() == Engine::ETextureDimension::Texture2D &&
        texture->GetArraySize() == 1 &&
        texture->GetSampleCount() == Engine::ETextureSampleCount::TextureSampleCount1 &&
        Engine::HasFlag(texture->GetUsage(), Engine::ETextureUsage::Sampler);
}

ImVec2 ChannelFilter::GetButtonSize()
{
    ImVec2 slot(0.0f, 0.0f);
    for (const char* label : { "R", "G", "B", "A" })
    {
        const auto size = EditorUI::GetIconButtonSize(label, true);
        slot.x = std::max(slot.x, size.x);
        slot.y = std::max(slot.y, size.y);
    }
    return ImVec2(std::ceil(slot.x), slot.y);
}

float ChannelFilter::GetButtonsWidth()
{
    return GetButtonSize().x * 4.0f;
}

void ChannelFilter::DrawButtons(EViewportChannelView& view)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    const auto slot = GetButtonSize();
    bool first = true;
    const auto drawButton = [&](const char* label, const char* key, EViewportChannelView flag, const ImVec4& activeColor)
    {
        const bool active = HasFlag(view, flag);
        EditorUI::IconButtonOptions options;
        options.Compact = true;
        options.Size = slot;
        options.SameLine = !first;
        first = false;
        ImGui::PushStyleColor(ImGuiCol_Button, active ? activeColor : ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
        if (EditorUI::IconButton(label, label, key, options))
            view = static_cast<EViewportChannelView>(static_cast<uint8>(view) ^ static_cast<uint8>(flag));
        ImGui::PopStyleColor();
    };
    drawButton("R", "Viewport.ChannelR", EViewportChannelView::R, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
    drawButton("G", "Viewport.ChannelG", EViewportChannelView::G, ImVec4(0.20f, 0.65f, 0.20f, 1.0f));
    drawButton("B", "Viewport.ChannelB", EViewportChannelView::B, ImVec4(0.20f, 0.35f, 0.80f, 1.0f));
    drawButton("A", "Viewport.ChannelA", EViewportChannelView::A, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
    ImGui::PopStyleVar(2);
}