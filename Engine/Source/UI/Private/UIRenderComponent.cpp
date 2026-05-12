#pragma once
#include "UIRenderComponent.h"
#include "Renderer.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "PipelineManager.h"
#include "RenderTargetManager.h"
#include "RenderPass.h"
#include "ComponentRegistry.h"
#include "Camera.h"

REGISTER_COMPONENT(UIRenderComponent)

EResult UIRenderComponent::Initialize(void* arg)
{
    return EResult();
}

UIRenderComponent* UIRenderComponent::Create(void* arg)
{
    return nullptr;
}

Component* UIRenderComponent::Clone(GameObject* owner, void* arg)
{
    return nullptr;
}

void UIRenderComponent::Free()
{
    __super::Free();
}

void UIRenderComponent::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
    if (!(m_Active && m_Owner && m_Owner->IsActive() && m_Owner->IsVisible())) return;

    const auto& activePasses = Renderer::Get().GetActiveViewportCameras();
    for (const auto& passInfo : activePasses)
    {
		if (!passInfo.Camera || !passInfo.RenderPass) continue;
        const RenderPassID passID = passInfo.RenderPass->GetID();
        if (passInfo.RenderPass->GetPassType() != ERenderPassType::UI) continue;
		if ((passInfo.Camera->GetCullingMask() & m_VisibilityChannel) == 0) continue;
        Renderer::Get().SubmitUI(this, passID);
    }
}

EResult UIRenderComponent::Render(f32 dt, RenderPass* renderPass)
{
    return EResult::NotImplemented;
}

EResult UIRenderComponent::BindPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass)
{
    tagRHIPipelineDesc pipelineDesc = {};
    pipelineDesc.Topology = mesh ? mesh->GetTopology() : ETopology::TriangleList;
    pipelineDesc.PipelineType = EPipelineType::Graphics;
    pipelineDesc.VertexShader = material->GetVertexShader()->GetRHIShader();
    pipelineDesc.PixelShader = material->GetPixelShader()->GetRHIShader();
    pipelineDesc.FrontFace = material->GetFrontFace();
    pipelineDesc.BlendState = material->GetBlendState();
    pipelineDesc.CullMode = material->GetCullMode();
    pipelineDesc.ColorAttachmentCount = renderPass->GetRenderTargetCount();
    pipelineDesc.InputLayouts = mesh ? mesh->GetInputLayoutDescs() : std::vector<tagInputLayoutDesc>();

    for (uint32 i = 0; i < pipelineDesc.ColorAttachmentCount; ++i)
    {
        pipelineDesc.ColorAttachmentFormats[i] = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i))->GetFormat();
    }

    wstring depthStencilName = renderPass->GetDepthStencilName();
    pipelineDesc.DepthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
    if (!depthStencilName.empty())
        pipelineDesc.DepthStencilAttachmentFormat = RenderTargetManager::Get().GetRenderTarget(depthStencilName)->GetFormat();
    pipelineDesc.DepthStencilState.DepthTestEnable = (!renderPass->GetDepthStencilName().empty()) && (material->GetDepthMode() != EDepthMode::None);
    pipelineDesc.DepthStencilState.DepthWriteEnable = pipelineDesc.DepthStencilState.DepthTestEnable && (material->GetDepthMode() == EDepthMode::ReadWrite);

    pipelineDesc.DepthStencilState.DepthCompareOp = material->GetDepthCompareOp();


    PipelineManager& pipelineManager = PipelineManager::Get();
    RHIPipeline* pipeline = pipelineManager.GetOrCreatePipeline(pipelineDesc);

    if (!pipeline)
    {
        return EResult::Fail;
    }
    return Renderer::Get().GetRHI()->BindPipeline(pipeline);
}
