#pragma once

#include "SpriteRenderer.h"

#include "MaterialInstance.h"

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
	__super::Free();
}
#pragma endregion


#pragma region Loop
void SpriteRenderer::LateUpdate(f32 dt)
{
	if(IsDirty())
	{
		UpdateMesh();
		SetDirty(false);
	}
	__super::LateUpdate(dt);
}

EResult SpriteRenderer::Render(f32 dt, RenderPass* renderPass)
{
	if (!m_Sprite || !m_Mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();

	RHIBuffer* vertexBuffer = m_Mesh->GetVertexBuffer();
	RHIBuffer* indexBuffer = m_Mesh->GetIndexBuffer();
	MaterialInstance* material = GetMaterialInstance();

	if (!rhi || !vertexBuffer) return EResult::Fail;

	if(IsFailure(BindPipeline(m_Mesh.Get(), material, renderPass)))
		return EResult::Fail;

	material->Bind(2);

	SceneUBO uboData;
	uboData.worldMatrix = m_Owner->GetComponent<Transform>()->GetWorldMatrix();

	rhi->BindConstantBuffer((void*)&uboData, sizeof(SceneUBO), 1);

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
EResult SpriteRenderer::SetSprite(const ResourceHandle<Sprite>& sprite)
{
	m_Sprite = sprite;

	if (IsFailure(UpdateMesh()))
		return EResult::Fail;
	if (IsFailure(UpdateMaterialInstance()))
		return EResult::Fail;
	return EResult::Success;
}
EResult SpriteRenderer::SetSprite(const ResourceHandle<Texture>& texture)
{
	tagSpriteCreateDesc desc;
	desc.Texture = texture;
	TODO("이미 로드한 Texture 로부터 Sprite 리소스를 생성하게 만들어야함");
	m_Sprite = ResourceManager::Get().LoadResource<Sprite>(&desc);
	
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

	Texture* texture = m_Sprite->GetTexture();
	if (!texture) return EResult::Fail;

	f32 ppu = texture->GetPixelPerUnit();
	f32 width = texture->GetWorldWidth();
	f32 height = texture->GetWorldHeight();

	vec2 pivot = m_Sprite->GetPivot();
	Rect region = m_Sprite->GetRegion();

	f32 worldWidth = region.Width / ppu;
	f32 worldHeight = region.Height / ppu;

	f32 xOffset = width * pivot.x;
	f32 yOffset = height * pivot.y;

	//f32 left = -worldWidth * pivot.x;
	//f32 right = worldWidth * (1.0f - pivot.x);
	//f32 bottom = -worldHeight * pivot.y;
	//f32 top = worldHeight * (1.0f - pivot.y);

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

	m_Mesh = ResourceManager::Get().GetResourceHandle<Mesh>(L"QuadMesh");
	if (!m_Mesh)
	{
		tagMeshCreateDesc desc;
		desc.VertexData = vertices.data();
		desc.VertexCount = static_cast<uint32>(vertices.size());
		desc.VertexStride = sizeof(Vertex);

		desc.IndexData = indices.data();
		desc.IndexCount = static_cast<uint32>(indices.size());
		desc.IndexStride = sizeof(uint32);

		m_Mesh = ResourceManager::Get().AddResource(L"QuadMesh", Mesh::Create(&desc));
	}

	m_Mesh->SetVertexBuffer(vertices.data(), static_cast<uint32>(vertices.size()));

	return EResult::Success;
}
EResult SpriteRenderer::UpdateMaterialInstance()
{
	if (!m_Sprite || !m_Sprite->GetTexture()) return EResult::Fail;

	// MaterialInstance가 없으면 base material로부터 생성
	if (m_MaterialInstances.empty())
	{
		Material* baseMaterial = GetSharedMaterial();
		if (!baseMaterial) return EResult::Fail;
		wstring instanceKey = baseMaterial->GetKey() + L"_Instance";
		ResourceHandle<MaterialInstance> instanceHandle = ResourceManager::Get().GetResourceHandle<MaterialInstance>(instanceKey);
		if (!instanceHandle)
		{
			instanceHandle = ResourceManager::Get().AddResource(instanceKey, MaterialInstance::Create(baseMaterial));
		}
		m_MaterialInstances.push_back(instanceHandle);
		if (!m_MaterialInstances[0]) return EResult::Fail;
	}

	// Sprite 텍스처를 slot 0에 오버라이드
	m_MaterialInstances[0]->SetTextureBySlot(0, m_Sprite.Get()->GetTextureHandle());
	return EResult::Success;
}
#pragma endregion

#pragma region Save&Load

#pragma endregion

void SpriteRenderer::Deserialize(Archive& ar)
{
	__super::Deserialize(ar);
	//ResourceManager& resourceManager = ResourceManager::Get();
	//resourceManager.LoadTexture(m_SpriteTag, m_SpritePath);
	//SetSprite(resourceManager.GetTexture(m_SpriteTag));
	//for (auto& materialInstance : m_MaterialInstances)
	//{
	//	materialInstance->Deserialize(ar);
	//}
}