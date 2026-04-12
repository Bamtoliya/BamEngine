#pragma once

#include "RenderComponent.h"

#include "Mesh.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "RenderPass.h"

#include "ResourceManager.h"
#include "Renderer.h"
#include "RHIPipeline.h"
#include "PipelineManager.h"
#include "RHI.h"
#include "RenderTargetManager.h"
#include "GameObject.h"


#pragma region Constructor&Destructor
EResult RenderComponent::Initialize(void* arg)
{
	return __super::Initialize(arg);
}
void RenderComponent::Free()
{
	Component::Free();
	m_RenderPassID = { INVALID_PASS_ID };
	m_MaterialInstances.clear();
}
#pragma endregion



#pragma region Loop
void RenderComponent::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
	if (m_Active && m_Owner && m_Owner->IsActive() && m_Owner->IsVisible())
	{
		const auto& activePasses = Renderer::Get().GetActiveViewportCameras();
		for (const auto& passInfo : activePasses)
		{
			Renderer::Get().Submit(this, passInfo.PassID);
		}
	}
}
#pragma endregion

#pragma region Material Management
void RenderComponent::SetMaterial(const ResourceHandle<Material>& material, uint32 index)
{
	if (!material) return;
	if (index >= m_MaterialInstances.size())
		m_MaterialInstances.resize(index + 1);

	if (m_MaterialInstances[index])
	{
		m_MaterialInstances[index]->SetBaseMaterial(material);
	}
	else
	{
		TODO("Render Component에서 MaterialInstance 생성 방식 결정 필요 (현재는 매번 새로 생성)");
		m_MaterialInstances[index] = ResourceManager::Get().AddResource<MaterialInstance>(material.Get()->GetKey() + L"_Instance", MaterialInstance::Create(material));
	}
}

Material* RenderComponent::GetSharedMaterial(uint32 index) const
{
	if (index >= m_MaterialInstances.size() || !m_MaterialInstances[index])
		return nullptr;
	return m_MaterialInstances[index]->GetBaseMaterial();
}

MaterialInstance* RenderComponent::GetMaterialInstance(uint32 index) const
{
	if (index >= m_MaterialInstances.size())
		return nullptr;
	return m_MaterialInstances[index].Get();
}
#pragma endregion



#pragma region Bind
EResult RenderComponent::BindPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass)
{
	if (!mesh || !material || !renderPass) return EResult::InvalidArgument;
	tagRHIPipelineDesc pipelineDesc = {};
	pipelineDesc.Topology = mesh->GetTopology();
	pipelineDesc.PipelineType = EPipelineType::Graphics;
	pipelineDesc.VertexShader = material->GetVertexShader()->GetRHIShader();
	pipelineDesc.PixelShader = material->GetPixelShader()->GetRHIShader();
	pipelineDesc.BlendMode = material->GetBlendMode();
	pipelineDesc.CullMode = material->GetCullMode();
	pipelineDesc.ColorAttachmentCount = renderPass->GetRenderTargetCount();
	pipelineDesc.InputLayout = mesh->GetInputLayoutDesc();


	for (uint32 i = 0; i < pipelineDesc.ColorAttachmentCount; ++i)
	{
		pipelineDesc.ColorAttachmentFormats[i] = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i))->GetFormat();
	}

	wstring depthStencilName = renderPass->GetDepthStencilName();
	if (!depthStencilName.empty())
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
#pragma endregion

