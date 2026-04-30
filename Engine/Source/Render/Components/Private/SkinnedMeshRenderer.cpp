#pragma once

#include "SkinnedMeshRenderer.h"

#include "Material.h"
#include "MaterialInstance.h"

#include "GameObject.h"
#include "Transform.h"
#include "Renderer.h"
#include "MeshFilter.h"
#include "Animator.h"

REGISTER_COMPONENT(SkinnedMeshRenderer)

#pragma region Contructor&Destructor
EResult SkinnedMeshRenderer::Initialize(void* arg)
{
	return __super::Initialize(arg);
}

SkinnedMeshRenderer* SkinnedMeshRenderer::Create(void* arg)
{
	SkinnedMeshRenderer* instance = new SkinnedMeshRenderer();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

SkinnedMeshRenderer* SkinnedMeshRenderer::Clone(GameObject* owner, void* arg)
{
	SkinnedMeshRenderer* instance = new SkinnedMeshRenderer(*this);
	instance->SetOwner(owner);
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void SkinnedMeshRenderer::Free()
{
	Safe_Release(m_BoneSSBO);
	__super::Free();
}
#pragma endregion

#pragma region Render


EResult SkinnedMeshRenderer::Render(f32 dt, RenderPass* renderPass)
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

	Animator* animator = m_Owner->GetComponent<Animator>();
	if (animator)
	{
		const vector<mat4>& bones = animator->GetFinalBoneMatrices();
		if (!bones.empty())
		{
			uint32 boneCount = static_cast<uint32>(bones.size());
			uint32 dataSize = boneCount * sizeof(mat4);
			// SSBO가 없거나 뼈 개수가 변경된 경우 재생성
			if (!m_BoneSSBO || m_LastBoneCount != boneCount)
			{
				Safe_Release(m_BoneSSBO);
				m_BoneSSBO = rhi->CreateBuffer(
					nullptr,        // 초기 데이터 없음 (아래에서 SetData)
					dataSize,
					sizeof(mat4),
					ERHIBufferType::Structured
				);
				m_LastBoneCount = boneCount;
			}
			// 매 프레임 뼈대 행렬 업로드
			if (m_BoneSSBO)
			{
				m_BoneSSBO->SetData(bones.data(), dataSize);
			}
		}
	}

	if (!m_BoneSSBO)
	{
		mat4 identity = glm::identity<mat4>();
		m_BoneSSBO = rhi->CreateBuffer(&identity, sizeof(mat4), sizeof(mat4), ERHIBufferType::Structured);
		m_LastBoneCount = 1;
	}

	if (IsFailure(mesh->Bind(0)))
		return EResult::Fail;

	RHIBuffer* ssboToBind = m_BoneSSBO;
	if (IsFailure(rhi->BindVertexStorageBuffers(0, &ssboToBind, 1)))
	{
		return EResult::Fail;
	}

	return mesh->GetIndexBuffer() ? rhi->DrawIndexed(mesh->GetIndexCount()) : rhi->Draw(mesh->GetVertexCount());
}

#pragma endregion