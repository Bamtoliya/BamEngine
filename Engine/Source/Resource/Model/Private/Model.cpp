#pragma once

#include "Model.h"

#pragma region Constructor&Destructor

EResult Model::Initialize(void* arg)
{
    if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
    CAST_DESC

    m_Meshes = desc->Meshes;
    m_Materials = desc->Materials;
    m_Skeleton = desc->Skeleton;
    m_Animations = desc->Animations;

    return EResult::Success;
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
    m_Meshes.clear();
    m_Materials.clear();
    m_Skeleton.Reset();
    m_Animations.clear();
}

#pragma endregion

EResult Model::Bind(uint32 slot)
{
    return EResult();
}
