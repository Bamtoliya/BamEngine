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
		UpdateMaterialInstance();
		SetDirty(false);
	}
	__super::LateUpdate(dt);
}

EResult SpriteRenderer::Render(f32 dt, RenderPass* renderPass)
{
	if (!m_Sprite || !m_Mesh) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();

	MaterialInterface* material = GetMaterial();

	if (!rhi) return EResult::Fail;

	if(IsFailure(BindPipeline(m_Mesh.Get(), material, renderPass)))
		return EResult::Fail;

	if(IsFailure(material->Bind(2)))
		return EResult::Fail;

	SceneUBO uboData;
	uboData.worldMatrix = m_Owner->GetComponent<Transform>()->GetWorldMatrix();

	if(IsFailure(rhi->BindConstantBuffer((void*)&uboData, sizeof(SceneUBO), 1)))
		return EResult::Fail;

	if(IsFailure(m_Mesh->Bind(0)))
		return EResult::Fail;

	return m_Mesh->GetIndexBuffer() ? rhi->DrawIndexed(m_Mesh->GetIndexCount()) : rhi->Draw(m_Mesh->GetVertexCount());
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

	f32 left = 0.0f - xOffset;
	f32 right = width - xOffset;
	f32 bottom = 0.0f - yOffset;
	f32 top = height - yOffset;

	// Position 스트림
	vector<VertexPosition> positions = {
		{ vec3(left, bottom, 0) },
		{ vec3(right, bottom, 0) },
		{ vec3(right, top, 0) },
		{ vec3(left, top, 0) }
	};

	// Material 스트림
	VertexMaterial matDefault;
	matDefault.normal = vec3(0.0f, 0.0f, -1.0f);

	VertexMaterial mat0 = matDefault; mat0.texCoord = vec2(0, m_Tiling.y);
	VertexMaterial mat1 = matDefault; mat1.texCoord = vec2(m_Tiling.x, m_Tiling.y);
	VertexMaterial mat2 = matDefault; mat2.texCoord = vec2(m_Tiling.x, 0);
	VertexMaterial mat3 = matDefault; mat3.texCoord = vec2(0, 0);

	vector<VertexMaterial> materials = { mat0, mat1, mat2, mat3 };

	vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

	m_Mesh = ResourceManager::Get().GetResourceHandle<Mesh>(L"QuadMesh");
	if (!m_Mesh)
	{
		tagMeshCreateDesc desc;

		// Streams 배열에 직접 세팅
		desc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
		desc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };

		desc.IndexData = indices.data();
		desc.IndexCount = static_cast<uint32>(indices.size());
		desc.IndexStride = sizeof(uint32);

		m_Mesh = ResourceManager::Get().AddResource(L"QuadMesh", Mesh::Create(&desc));
	}

	// 다이내믹 업데이트도 통일된 함수 하나로 처리
	m_Mesh->SetStreamBuffer(EMeshStream::Position, positions.data(), (uint32)positions.size(), sizeof(VertexPosition));
	m_Mesh->SetStreamBuffer(EMeshStream::Material, materials.data(), (uint32)materials.size(), sizeof(VertexMaterial));

	return EResult::Success;
}

EResult SpriteRenderer::UpdateMaterialInstance()
{
	if (!m_Sprite || !m_Sprite->GetTexture()) return EResult::Fail;

	// MaterialInstance가 없으면 base material로부터 생성
	if (m_Materials.empty())
	{
		MaterialInterface* baseMaterial = GetMaterial();
		if (!baseMaterial) return EResult::Fail;
		wstring instanceKey = baseMaterial->GetKey() + L"_Instance";
		ResourceHandle<MaterialInstance> instanceHandle = ResourceManager::Get().GetResourceHandle<MaterialInstance>(instanceKey);
		if (!instanceHandle)
		{
			instanceHandle = ResourceManager::Get().AddResource(instanceKey, MaterialInstance::Create(baseMaterial));
		}
		m_Materials.push_back(instanceHandle);
		if (!m_Materials[0]) return EResult::Fail;
	}

	// Sprite 텍스처를 slot 0에 오버라이드
	m_Materials[0]->SetTextureBySlot(0, m_Sprite.Get()->GetTextureHandle());
	return EResult::Success;
}
#pragma endregion

#pragma region Save&Load

#pragma endregion

void SpriteRenderer::Deserialize(Archive& ar)
{
	SetDirty();
}