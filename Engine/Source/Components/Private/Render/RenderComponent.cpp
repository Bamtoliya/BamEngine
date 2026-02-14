#pragma once

#include "RenderComponent.h"
#include "Renderer.h"
#include "RHIPipeline.h"
#include "PipelineManager.h"
#include "RHI.h"
#include "RenderTargetManager.h"

void RenderComponent::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
	if (m_Active && m_Owner->IsActive() && m_Owner->IsVisible())
	{
		const auto& activePasses = Renderer::Get().GetActiveViewportCameras();
		for (const auto& passInfo : activePasses)
		{
			Renderer::Get().Submit(this, passInfo.PassID);
		}
	}
}

EResult RenderComponent::BindPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass)
{
	tagRHIPipelineDesc pipelineDesc = {};
	pipelineDesc.Topology = mesh->GetTopology();
	pipelineDesc.PipelineType = EPipelineType::Graphics;
	pipelineDesc.VertexShader = material->GetVertexShader()->GetRHIShader();
	pipelineDesc.PixelShader = material->GetPixelShader()->GetRHIShader();
	pipelineDesc.BlendMode = material->GetBlendMode();
	pipelineDesc.CullMode = material->GetCullMode();
	pipelineDesc.ColorAttachmentCount = renderPass->GetRenderTargetCount();

	
	for (uint32 i = 0; i < pipelineDesc.ColorAttachmentCount; ++i)
	{
		pipelineDesc.ColorAttachmentFormats[i] = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i))->GetFormat();
	}

	wstring depthStencilName = renderPass->GetDepthStencilName();
	if(!depthStencilName.empty())
		pipelineDesc.DepthStencilAttachmentFormat = RenderTargetManager::Get().GetRenderTarget(depthStencilName)->GetFormat();
	pipelineDesc.DepthStencilState.DepthTestEnable = (renderPass->GetDepthStencilName() != L"") && (material->GetDepthMode() != EDepthMode::None);
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