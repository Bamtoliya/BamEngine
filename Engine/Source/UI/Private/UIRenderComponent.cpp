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
#include "UICanvas.h"

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

    UICanvas* canvas = GetRootCanvas();
    bool isWorldSpace = canvas && canvas->GetRenderMode() == ECanvasRenderMode::WorldSpace;

    for (const auto& passInfo : activePasses)
    {
		if (!passInfo.renderPass) continue;
        const ERenderPassType passType = passInfo.renderPass->GetPassType();
        const RenderPassID passID = passInfo.renderPass->GetID();
        if (isWorldSpace)
        {
            if (passType == ERenderPassType::Forward || passType == ERenderPassType::ForwardTransparent)
            {
				Renderer::Get().SubmitUI(this, passID);
            }
        }
        else
        {
            if (passInfo.renderPass->GetPassType() == ERenderPassType::UI)
            {
                Renderer::Get().SubmitUI(this, passID);
            };
        }        
    }
}

EResult UIRenderComponent::Render(f32 dt, RenderPass* renderPass)
{
    return EResult::NotImplemented;
}

EResult UIRenderComponent::BindPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass)
{
    RHIPipelineDesc pipelineDesc = {};
    pipelineDesc.topology = mesh ? mesh->GetTopology() : ETopology::TriangleList;
    pipelineDesc.pipelineType = EPipelineType::Graphics;
    pipelineDesc.vertexShader = material->GetVertexShader()->GetRHIShader();
    pipelineDesc.pixelShader = material->GetPixelShader()->GetRHIShader();
    pipelineDesc.frontFace = material->GetFrontFace();
    pipelineDesc.blendState = material->GetBlendState();
    pipelineDesc.cullMode = material->GetCullMode();
    pipelineDesc.colorAttachmentCount = renderPass->GetRenderTargetCount();
    pipelineDesc.inputLayouts = mesh ? mesh->GetInputLayoutDescs() : std::vector<InputLayoutDesc>();

    for (uint32 i = 0; i < pipelineDesc.colorAttachmentCount; ++i)
    {
        pipelineDesc.colorAttachmentFormats[i] = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i))->GetFormat();
    }

    wstring depthStencilName = renderPass->GetDepthStencilName();
    pipelineDesc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
    if (!depthStencilName.empty())
        pipelineDesc.depthStencilAttachmentFormat = RenderTargetManager::Get().GetRenderTarget(depthStencilName)->GetFormat();
    pipelineDesc.depthStencilState.depthTestEnable = (!renderPass->GetDepthStencilName().empty()) && (material->GetDepthMode() != EDepthMode::None);
    pipelineDesc.depthStencilState.depthWriteEnable = pipelineDesc.depthStencilState.depthTestEnable && (material->GetDepthMode() == EDepthMode::ReadWrite);

    pipelineDesc.depthStencilState.depthCompareOp = material->GetDepthCompareOp();


    PipelineManager& pipelineManager = PipelineManager::Get();
    RHIPipeline* pipeline = pipelineManager.GetOrCreatePipeline(pipelineDesc);

    if (!pipeline)
    {
        return EResult::Fail;
    }
    return Renderer::Get().GetRHI()->BindPipeline(pipeline);
}

UICanvas* UIRenderComponent::GetRootCanvas() const
{
	// Traverse up the hierarchy to find the root canvas
	GameObject* currentObject = m_Owner;
	UICanvas* rootCanvas = nullptr;
    while (currentObject)
    {
		UICanvas* canvas = currentObject->GetComponent<UICanvas>();
		if (canvas) rootCanvas = canvas;
		currentObject = currentObject->GetParent();
    }
    return rootCanvas;
}
