#pragma once

#include "SpriteRenderer.h"

REGISTER_COMPONENT(SpriteRenderer)

#pragma region Constructor&Destructor
EResult SpriteRenderer::Initialize(void* arg)
{
	__super::Initialize(arg);

	return EResult::Success;
}

SpriteRenderer* SpriteRenderer::Create(void* arg)
{
	SpriteRenderer* instance = new SpriteRenderer();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* SpriteRenderer::Clone(GameObject* owner, void* arg)
{
	SpriteRenderer* instance = new SpriteRenderer();
	instance->SetOwner(owner);
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void SpriteRenderer::Free()
{
	Safe_Release(m_Mesh);
	Safe_Release(m_Texture);
	__super::Free();
}
#pragma endregion


#pragma region Loop
void SpriteRenderer::LateUpdate(f32 dt)
{
	if (m_Texture)
	{
		if (m_CachedPPU != m_Texture->GetPixelPerUnit() || m_PrevPivot != m_Pivot)
		{
			UpdateMesh();
		}
	}
	__super::LateUpdate(dt);
}
EResult SpriteRenderer::Render(f32 dt)
{
	if (!m_Texture || !m_Mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();

	RHIBuffer* vertexBuffer = m_Mesh->GetVertexBuffer();
	RHIBuffer* indexBuffer = m_Mesh->GetIndexBuffer();
	RHITexture* texture = m_Texture->GetRHITexture();


	rhi->BindConstantBuffer((void*)&m_Owner->GetComponent<Transform>()->GetWorldMatrix(), 0);
	rhi->BindTexture(texture, 0);

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
EResult SpriteRenderer::SetTexture(Texture* texture)
{
	if (m_Texture)
		Safe_Release(m_Texture);
	m_Texture = texture;
	Safe_AddRef(m_Texture);

	if (IsFailure(UpdateMesh()))
		return EResult::Fail;
	return EResult::Success;
}
EResult SpriteRenderer::SetPivot(vec2 pivot)
{
	m_Pivot = pivot;
	if (IsFailure(UpdateMesh()))
		return EResult::Fail;
	return EResult();
}
EResult SpriteRenderer::UpdateMesh()
{
	if (!m_Texture) return EResult::Fail;

	m_CachedPPU = m_Texture->GetPixelPerUnit();
	m_PrevPivot = m_Pivot;

	f32 ppu = m_Texture->GetPixelPerUnit();
	f32 width = m_Texture->GetWorldWidth();
	f32 height = m_Texture->GetWorldHeight();

	f32 xOffset = width * m_Pivot.x;
	f32 yOffset = height * m_Pivot.y;

	f32 left = 0.0f - xOffset;
	f32 right = width - xOffset;
	f32 bottom = 0.0f - yOffset;
	f32 top = height - yOffset;

	vector<Vertex> vertices;

	vertices.push_back({ vec3(left, bottom, 0), vec2(0, m_Tiling.y) });
	vertices.push_back({ vec3(right, bottom, 0), vec2(m_Tiling.x, m_Tiling.y) });
	vertices.push_back({ vec3(right, top, 0), vec2(m_Tiling.x, 0) });
	vertices.push_back({ vec3(left, top, 0), vec2(0, 0) });

	vector<uint32> indices = {
		0, 1, 2,
		0, 2, 3
	};

	if (!m_Mesh)
	{
		tagMeshCreateInfo desc;
		desc.VertexData = vertices.data();
		desc.VertexCount = static_cast<uint32>(vertices.size());
		desc.VertexStride = sizeof(Vertex);

		desc.IndexData = indices.data();
		desc.IndexCount = static_cast<uint32>(indices.size());
		desc.IndexStride = sizeof(uint32);

		m_Mesh = Mesh::Create(&desc);
	}

	m_Mesh->SetVertexBuffer(vertices.data(), static_cast<uint32>(vertices.size()));

	return EResult::Success;
}
#pragma endregion


