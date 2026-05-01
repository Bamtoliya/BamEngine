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
	m_Materials.clear();
	for (auto& pair : m_DynamicInstances)
	{
		if (pair.second)
		{
			pair.second->Free();
			ResourceManager::Get().DestroyResource(pair.second);
		}
	}
	m_DynamicInstances.clear();
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
			if(passInfo.Camera)
				Renderer::Get().Submit(this, passInfo.PassID);
		}
	}
}
#pragma endregion

#pragma region Material Management
void RenderComponent::SetMaterial(const ResourceHandle<MaterialInterface>& material, uint32 index)
{
	if (!material) return;
	if (index >= m_Materials.size())
		m_Materials.resize(index + 1);
	m_Materials[index] = material;
}

MaterialInterface* RenderComponent::GetMaterial(uint32 index) const
{
	auto it = m_DynamicInstances.find(index);
	if (it != m_DynamicInstances.end() && it->second)
		return it->second;
	if (index >= m_Materials.size())
		return nullptr;
	return m_Materials[index].Get();
}
EResult RenderComponent::CreateDynamicMaterialInstance(uint32 index)
{
	// 이미 Dynamic Instance가 있으면 성공으로 반환 (중복 생성 방지)
	if (m_DynamicInstances.count(index) && m_DynamicInstances[index])
		return EResult::Success;
	// 원본 머티리얼 가져오기
	if (index >= m_Materials.size() || !m_Materials[index])
		return EResult::InvalidArgument;
	MaterialInterface* original = m_Materials[index].Get();
	if (!original)
		return EResult::Fail;
	// 원본이 Material이면 그것을 Base로, MaterialInstance이면 그 Base를 가져옴
	Material* baseMaterial = nullptr;
	if (MaterialInstance* existingInst = dynamic_cast<MaterialInstance*>(original))
	{
		baseMaterial = existingInst->GetBaseMaterial();
	}
	else
	{
		baseMaterial = dynamic_cast<Material*>(original);
	}
	if (!baseMaterial)
		return EResult::Fail;
	// ResourceManager를 거치지 않고 직접 생성 (파일 없는 런타임 전용)
	tagMaterialInstanceDesc desc = {};
	desc.BaseMaterialHandle = ResourceManager::Get().GetResourceHandle<Material>(baseMaterial->GetKey());
	MaterialInstance* dynamicInst = MaterialInstance::Create(&desc);
	if (!dynamicInst)
		return EResult::Fail;
	m_DynamicInstances[index] = dynamicInst;
	return EResult::Success;
}
bool RenderComponent::HasDynamicMaterialInstance(uint32 index) const
{
	auto it = m_DynamicInstances.find(index);
	return it != m_DynamicInstances.end() && it->second != nullptr;
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
	pipelineDesc.InputLayouts = mesh->GetInputLayoutDescs();

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

