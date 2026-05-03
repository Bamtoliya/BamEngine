#pragma once
#include "MeshRenderer.h"

#include "Material.h"
#include "MaterialInstance.h"

#include "GameObject.h"
#include "Transform.h"
#include "Renderer.h"
#include "MeshFilter.h"

#include "ResourceManager.h"
#include "RenderPass.h"

REGISTER_COMPONENT(MeshRenderer)

#pragma region Contructor&Destructor
EResult MeshRenderer::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;

	return EResult::Success;
}

Component* MeshRenderer::Create(void* arg)
{
	MeshRenderer* instance = new MeshRenderer();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* MeshRenderer::Clone(GameObject* owner, void* arg)
{
	MeshRenderer* instance = new MeshRenderer(*this);
	instance->SetOwner(owner);
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void MeshRenderer::Free()
{
	__super::Free();
}
#pragma endregion

#pragma region Render


EResult MeshRenderer::Render(f32 dt, RenderPass* renderPass)
{
	if(!m_DrawShadow && renderPass && renderPass->GetPassType() == ERenderPassType::Shadow) return EResult::Success;
	MeshFilter* meshFilter = m_Owner->GetComponent<MeshFilter>();
	if (!meshFilter) return EResult::Success;
	Mesh* mesh = meshFilter->GetMesh();
	if (!mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();
	MaterialInterface* material = GetMaterial();

	if (!material)
	{
		ENGINE_LOG_WARN("MeshRenderer material is null. Owner: {}", WStrToStr(m_Owner->GetName()));
		return EResult::Fail; // 또는 fallback material
	}

	if (renderPass && renderPass->GetPassType() == ERenderPassType::Shadow)
	{
		material = ResourceManager::Get().GetResourceHandle<Material>(L"Resources/Material/ShadowDepthMaterial").Get();
		if (!material) return EResult::Success;
	}
	else
	{
		if (IsFailure(material->Bind(2)))
			return EResult::Fail;
	}
	
	

	if (IsFailure(BindPipeline(mesh, material, renderPass)))
		return EResult::Fail;

	SceneUBO uboData;
	uboData.worldMatrix = m_Owner->GetComponent<Transform>()->GetWorldMatrix();

	if (IsFailure(rhi->BindConstantBuffer((void*)&uboData, sizeof(SceneUBO), 1)))
		return EResult::Fail;

	if (IsFailure(mesh->Bind(0)))
		return EResult::Fail;

	return mesh->GetIndexBuffer() ? rhi->DrawIndexed(mesh->GetIndexCount()) : rhi->Draw(mesh->GetVertexCount());
}

#pragma endregion

#pragma region Setter
#pragma endregion