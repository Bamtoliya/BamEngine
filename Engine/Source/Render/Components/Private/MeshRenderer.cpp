#pragma once
#include "MeshRenderer.h"

#include "Material.h"
#include "MaterialInstance.h"

#include "GameObject.h"
#include "Transform.h"
#include "Renderer.h"
#include "MeshFilter.h"

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
	MeshFilter* meshFilter = m_Owner->GetComponent<MeshFilter>();
	if (!meshFilter) return EResult::Success;
	Mesh* mesh = meshFilter->GetMesh();
	if (!mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();
	MaterialInterface* material = GetMaterial();

	if (IsFailure(BindPipeline(mesh, material, renderPass)))
		return EResult::Fail;

	if (IsFailure(material->Bind(2)))
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