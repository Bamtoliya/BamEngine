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

#include "LightManager.h"
#include "SerializationHelper.h"


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
		const bool bTransparent = IsTransparent();
		const auto& activePasses = Renderer::Get().GetActiveViewportCameras();
		for (const auto& passInfo : activePasses)
		{
			if (!passInfo.RenderPass) continue;

			const ERenderPassType passType = passInfo.RenderPass->GetPassType();

			if (passType == ERenderPassType::Shadow)
			{
				if (!m_DrawShadow) continue;                                       
				if (bTransparent) continue;                                        
				if (LightManager::Get().GetShadowCastingLights().empty()) continue;
			}
			else if (passType == ERenderPassType::ForwardTransparent)
			{
				if (!bTransparent) continue;
				if (!passInfo.Camera) continue;
			}
			else if (passType == ERenderPassType::Geometry)
			{
				if (bTransparent) continue;
				if (!passInfo.Camera) continue;
			}
			else
			{
				// Shadow, ForwardTransparent, Geometry 패스가 아니면 (UI, PostProcess, Custom 등) 
				// 메쉬/스프라이트를 그리는 패스가 아니므로 스킵합니다.
				continue;
			}

			tagFrustum frustum;
			bool isShadow = false;
			if (Renderer::Get().TryGetPassFrustum(passInfo.RenderPass->GetID(), frustum, isShadow))
			{
				const std::optional<AABB> localBounds = GetLocalBounds();
				if (localBounds.has_value())
				{
					Transform* transform = m_Owner->GetComponent<Transform>();
					if (transform)
					{
						AABB worldAABB = FrustumCuller::TransformAABB(localBounds.value(), transform->GetWorldMatrix());

						// Shadow Pass: 큰 오브젝트의 그림자 누락 방지를 위해 AABB를 보수적으로 확장
						if (isShadow)
							worldAABB = FrustumCuller::ExpandAABB(worldAABB, 1.f);

						if (!FrustumCuller::TestAABB(frustum, worldAABB))
							continue; // 프러스텀 밖 → 컬링
					}
				}
			}

			Renderer::Get().Submit(this, passInfo.RenderPass->GetID());
		}
	}
}
bool RenderComponent::IsTransparent() const
{
	const MaterialInterface* mat = GetMaterial(0);
	if (!mat) return false;
	const EBlendMode mode = mat->GetBlendMode();
	return (mode == EBlendMode::AlphaBlend
		|| mode == EBlendMode::Additive
		|| mode == EBlendMode::NonPremultiplied);
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

MaterialInterface* RenderComponent::GetSharedMaterial(uint32 index) const
{
	if (index >= m_Materials.size())
		return nullptr;
	return m_Materials[index].Get();
}

MaterialInstance* RenderComponent::GetDynamicMaterialInstance(uint32 index) const
{
	auto it = m_DynamicInstances.find(index);
	if (it == m_DynamicInstances.end())
		return nullptr;
	return it->second;
}

MaterialInterface* RenderComponent::GetEditableMaterial(uint32 index, bool forceDynamicInstance)
{
	if (forceDynamicInstance && !HasDynamicMaterialInstance(index))
	{
		if (IsFailure(CreateDynamicMaterialInstance(index)))
			return GetMaterial(index);
	}
	return GetMaterial(index);
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
	if (MaterialInstance* existingInst = dynamic_cast<MaterialInstance*>(original))
	{
		for (const auto& binding : existingInst->GetTextureBindings())
		{
			dynamicInst->SetTextureBinding(binding.name, binding.slot, binding.texture);
			if (binding.hasCustomSampler)
				dynamicInst->SetSamplerDescBySlot(binding.slot, binding.samplerDesc);
		}
	}

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

#pragma endregion

#pragma region Save
void RenderComponent::Serialize(Archive& ar)
{
	// 1) 기존 PROPERTY (m_Materials, m_DrawShadow 등) 직렬화
	SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);

	// 2) DynamicInstances inline 직렬화
	if (ar.IsWriting())
	{
		ar.BeginArray("m_DynamicOverrides");
		for (auto& [slotIdx, inst] : m_DynamicInstances)
		{
			if (!inst) continue;
			ar.BeginArrayElement();

			uint32 slot = slotIdx;
			ar.Process("slot", slot);

			wstring baseKey = inst->GetBaseMaterial() ? inst->GetBaseMaterial()->GetKey() : L"";
			ar.Process("baseKey", baseKey);

			// MaterialInterface의 m_Parameters, m_TextureBindings 등 그대로 직렬화
			SerializationHelper::SerializeReflectionProperties(ar, &MaterialInterface::GetStaticTypeInfo(), inst);

			ar.EndArrayElement();
		}
		ar.EndArray();
	}
	else
	{
		size_t count = ar.BeginArray("m_DynamicOverrides");
		for (size_t i = 0; i < count; ++i)
		{
			ar.BeginArrayElement();

			uint32 slot = 0;
			ar.Process("slot", slot);

			wstring baseKey;
			ar.Process("baseKey", baseKey);

			ResourceHandle<Material> baseHandle =
				ResourceManager::Get().GetResourceHandle<Material>(baseKey);

			if (baseHandle)
			{
				tagMaterialInstanceDesc desc{};
				desc.BaseMaterialHandle = baseHandle;
				MaterialInstance* inst = MaterialInstance::Create(&desc);
				if (inst)
				{
					SerializationHelper::SerializeReflectionProperties(ar, &MaterialInterface::GetStaticTypeInfo(), inst);
					m_DynamicInstances[slot] = inst;
				}
			}

			ar.EndArrayElement();
		}
		ar.EndArray();
	}
}
#pragma endregion
