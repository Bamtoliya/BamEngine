#pragma once

#include "Model.h"

#pragma region Constructor&Destructor

#pragma endregion

EResult Model::Initialize(void* arg)
{
    if (!arg) return EResult::InvalidArgument;
    CAST_DESC


    m_Materials = desc->Materials;
    m_Meshes = desc->Meshes;
    m_Skeleton = desc->Skeleton;

	for (auto& material : m_Materials)
    {
        if (!material) return EResult::InvalidArgument;
		Safe_AddRef(material);
    }
    for (auto& mesh : m_Meshes)
    {
		if (!mesh) return EResult::InvalidArgument;
        Safe_AddRef(mesh);
	}

	Safe_AddRef(m_Skeleton);

    return EResult::Success;
}

Model* Model::Create(void* arg)
{
    Model* instance = new Model();
    if (IsFailure(instance->Initialize(arg)))
    {
		Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

void Model::Free()
{
    RELEASE_VECTOR(m_Meshes);
	RELEASE_VECTOR(m_Materials);
	Safe_Release(m_Skeleton);
}

EResult Model::Bind(uint32 slot)
{
    return EResult();
}
