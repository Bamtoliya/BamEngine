#pragma once

#include "SpriteRenderer.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Transform.h"
#include "CameraManager.h"

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
	Safe_Release(m_Sprite);
	__super::Free();
}
#pragma endregion


#pragma region Loop
void SpriteRenderer::LateUpdate(f32 dt)
{
	if (m_Sprite)
	{
		UpdateMesh();
	}
	__super::LateUpdate(dt);
}

struct SceneUBO
{
	mat4 worldMatrix;
	mat4 viewMatrix;
	mat4 projMatrix;
};

EResult SpriteRenderer::Render(f32 dt)
{
	if (!m_Sprite || !m_Mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();

	Camera* mainCamera = CameraManager::Get().GetMainCamera();
	if (!mainCamera) return EResult::Fail;

	RHIBuffer* vertexBuffer = m_Mesh->GetVertexBuffer();
	RHIBuffer* indexBuffer = m_Mesh->GetIndexBuffer();
	RHITexture* texture = m_Sprite->GetTexture()->GetRHITexture();
	MaterialInstance* material = GetMaterialInstance();

	if (!rhi || !vertexBuffer) return EResult::Fail;

	material->Bind(0);

	SceneUBO uboData;
	uboData.worldMatrix = m_Owner->GetComponent<Transform>()->GetWorldMatrix();
	// 카메라의 View/Projection 행렬
	uboData.viewMatrix = mainCamera->GetViewMatrix();
	uboData.projMatrix = mainCamera->GetProjMatrix();

	rhi->BindConstantBuffer((void*)&uboData, sizeof(SceneUBO), 0);

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
EResult SpriteRenderer::SetSprite(Sprite* sprite)
{
	if (m_Sprite)
		Safe_Release(m_Sprite);
	m_Sprite = sprite;
	Safe_AddRef(m_Sprite);

	if (IsFailure(UpdateMesh()))
		return EResult::Fail;
	if (IsFailure(UpdateMaterialInstance()))
		return EResult::Fail;
	return EResult::Success;
}
EResult SpriteRenderer::SetSprite(Texture* texture)
{
	if(m_Sprite)
		Safe_Release(m_Sprite);
	tagSpriteCreateDesc desc;
	desc.Texture = texture;
	m_Sprite = Sprite::Create(&desc);
	if (IsFailure(UpdateMesh()))
		return EResult::Fail;
	if (IsFailure(UpdateMaterialInstance()))
		return EResult::Fail;
	return EResult();
}
EResult SpriteRenderer::UpdateMesh()
{
	if (!m_Sprite) return EResult::Fail;

	m_CachedSpriteVersion = m_Sprite->GetVersion();

	f32 ppu = m_Sprite->GetTexture()->GetPixelPerUnit();
	f32 width = m_Sprite->GetTexture()->GetWorldWidth();
	f32 height = m_Sprite->GetTexture()->GetWorldHeight();

	vec2 pivot = m_Sprite->GetPivot();
	Rect region = m_Sprite->GetRegion();

	f32 worldWidth = region.Width / ppu;
	f32 worldHeight = region.Height / ppu;

	f32 xOffset = width * pivot.x;
	f32 yOffset = height * pivot.y;

	f32 left = -worldWidth * pivot.x;
	f32 right = worldWidth * (1.0f - pivot.x);
	f32 bottom = -worldHeight * pivot.y;
	f32 top = worldHeight * (1.0f - pivot.y);

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
EResult SpriteRenderer::UpdateMaterialInstance()
{
	if (!m_Sprite || !m_Sprite->GetTexture()) return EResult::Fail;

	// MaterialInstance가 없으면 base material로부터 생성
	if (!m_MaterialInstances[0])
	{
		Material* baseMaterial = GetSharedMaterial();
		if (!baseMaterial) return EResult::Fail;
		m_MaterialInstances[0] = MaterialInstance::Create(baseMaterial);
		if (!m_MaterialInstances[0]) return EResult::Fail;
	}

	// Sprite 텍스처를 slot 0에 오버라이드
	m_MaterialInstances[0]->SetTextureBySlot(0, m_Sprite->GetTexture()->GetRHITexture());
	return EResult::Success;
}
#pragma endregion


