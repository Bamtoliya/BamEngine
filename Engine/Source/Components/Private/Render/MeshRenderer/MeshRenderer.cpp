#pragma once
#include "MeshRenderer.h"
#include "Renderer.h"
#include "Mesh.h"

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
	Safe_Release(m_Mesh);
	__super::Free();
}
#pragma endregion

#pragma region Loop
void MeshRenderer::Update(f32 dt)
{
}

EResult MeshRenderer::Render(f32 dt)
{
	if (!m_Mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();

	RHIBuffer* vertexBuffer = m_Mesh->GetVertexBuffer();
	RHIBuffer* indexBuffer = m_Mesh->GetIndexBuffer();

	if (!rhi || !vertexBuffer) return EResult::Fail;

	rhi->BindVertexBuffer(vertexBuffer);

	if (indexBuffer)
	{
		rhi->BindIndexBuffer(indexBuffer);
		rhi->DrawIndexed(m_Mesh->GetIndexCount());
	}
	else
	{
		rhi->Draw(m_Mesh->GetVertexCount());
	}

	return EResult::Success;
}

#pragma endregion

#pragma region Setter
void MeshRenderer::SetMesh(Mesh* mesh)
{
	if(m_Mesh)
		Safe_Release(m_Mesh);
	m_Mesh = mesh;
	if (m_Mesh)
		Safe_AddRef(m_Mesh);
}
#pragma endregion