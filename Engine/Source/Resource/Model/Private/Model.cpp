#pragma once

#include "Model.h"
#include "Logger.h"
#include "ResourceManager.h"

#pragma region Constructor&Destructor

EResult Model::Initialize(void* arg)
{
    if (const EResult result = __super::Initialize(arg); result != EResult::Success)
        return result;
    CAST_DESC
    m_Nodes = desc->Nodes;
    m_Meshes = desc->Meshes;
    m_Materials = desc->Materials;
    m_Skeleton = desc->Skeleton;
    m_Animations = desc->Animations;
	m_MeshMaterialIndices = desc->MeshMaterialIndices;
    return OnPostLoad();
}

Model* Model::Create(void* arg)
{
    Model* instance = new Model();
    if (IsFailure(instance->Initialize(arg)))
    {
        instance->Free();
        delete instance;
        return nullptr;
    }
    return instance;
}

void Model::Free()
{
	m_Nodes.clear();
	m_Meshes.clear();
	m_Materials.clear();
	m_Skeleton.Reset();
	m_Animations.clear();
	m_MeshMaterialIndices.clear();
}

EResult Model::OnPostLoad()
{
	// 빈 배열은 연결 정보가 없던 이전 파일로 취급합니다.
	if (!m_MeshMaterialIndices.empty())
	{
		if (m_MeshMaterialIndices.size() != m_Meshes.size())
		{
			ENGINE_LOG_ERROR("Model mesh/material mapping count mismatch.");
			return EResult::InvalidArgument;
		}

		for (size_t i = 0; i < m_MeshMaterialIndices.size(); ++i)
		{
			if (m_MeshMaterialIndices[i] >= m_Materials.size())
			{
				ENGINE_LOG_ERROR("Invalid model material slot: mesh={}, slot={}", i, m_MeshMaterialIndices[i]);
				return EResult::InvalidArgument;
			}
		}
	}

	for (size_t nodeIndex = 0; nodeIndex < m_Nodes.size(); ++nodeIndex)
	{
		const auto& node = m_Nodes[nodeIndex];

		// 부모는 없거나, 현재 노드보다 앞에 있어야 합니다.
		if (node.ParentIndex < -1 ||
			(node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) >= nodeIndex))
		{
			ENGINE_LOG_ERROR("Invalid model node parent: node={}, parent={}", nodeIndex, node.ParentIndex);
			return EResult::InvalidArgument;
		}

		for (const uint32 meshIndex : node.MeshIndices)
		{
			if (meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex].IsValid())
			{
				ENGINE_LOG_ERROR("Invalid model node mesh: node={}, mesh={}", nodeIndex, meshIndex);
				return EResult::InvalidArgument;
			}
		}

		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(node.LocalTransform[column][row]))
				{
					ENGINE_LOG_ERROR("Non-finite model node transform: node={}", nodeIndex);
					return EResult::InvalidArgument;
				}
			}
		}
	}

	return EResult::Success;
}

#pragma endregion

#pragma region Getter
ResourceHandle<MaterialInterface> Model::GetMeshMaterial(uint32 meshIndex) const
{
	if (meshIndex >= m_MeshMaterialIndices.size())
		return {};
	const uint32 materialIndex = m_MeshMaterialIndices[meshIndex];
	if (materialIndex >= m_Materials.size())
		return {};
	return m_Materials[materialIndex];
}
#pragma endregion
#pragma region Setter
EResult Model::SetMaterial(uint32 slotIndex, const ResourceHandle<MaterialInterface>& material)
{
	if (slotIndex >= m_Materials.size())
		return EResult::InvalidArgument;
	const Handle handle = material.GetRawHandle();
	if (handle.IsValid() && dynamic_cast<MaterialInterface*>(ResourceManager::Get().GetResource(handle)) == nullptr)
		return EResult::InvalidArgument;
	if (m_Materials[slotIndex] == material)
		return EResult::Success;
	m_Materials[slotIndex] = material;
	IncreaseVersion();
	return EResult::Success;
}
#pragma endregion
#pragma region Save&Load
archive::ArchiveResult Model::ReadArchive(const archive::IArchive& ar)
{
	Model candidate;
	const auto result = candidate.Resource::ReadArchive(ar);
	if (!result)
		return result;

	m_Key = std::move(candidate.m_Key);
	m_Path = std::move(candidate.m_Path);
	m_Nodes = std::move(candidate.m_Nodes);
	m_Meshes = std::move(candidate.m_Meshes);
	m_Materials = std::move(candidate.m_Materials);
	m_MeshMaterialIndices = std::move(candidate.m_MeshMaterialIndices);
	m_Skeleton = std::move(candidate.m_Skeleton);
	m_Animations = std::move(candidate.m_Animations);

	return result;
}
#pragma endregion

EResult Model::Bind(uint32 slot)
{
    return EResult();
}
