#pragma once
#include "MeshRenderer.h"

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
	if (meshFilter)
	{
		Mesh* mesh = meshFilter->GetMesh();
		if (!mesh)
			return EResult::Success;

		RHI* rhi = Renderer::Get().GetRHI();

		RHIBuffer* vertexBuffer = mesh->GetVertexBuffer();
		RHIBuffer* indexBuffer = mesh->GetIndexBuffer();
		MaterialInstance* material = GetMaterialInstance();

		if (IsFailure(BindPipeline(mesh, material, renderPass)))
			return EResult::Fail;

		material->Bind(2);

		SceneUBO uboData;
		uboData.worldMatrix = m_Owner->GetComponent<Transform>()->GetWorldMatrix();

		rhi->BindConstantBuffer((void*)&uboData, sizeof(SceneUBO), 1);

		rhi->BindVertexBuffer(vertexBuffer);

		if (indexBuffer)
		{
			rhi->BindIndexBuffer(indexBuffer);
			rhi->DrawIndexed(mesh->GetIndexCount());
		}
		else
		{
			rhi->Draw(mesh->GetVertexCount());
		}

		return EResult::Success;
	}
	return EResult::Success;
}

#pragma endregion

#pragma region Setter
#pragma endregion