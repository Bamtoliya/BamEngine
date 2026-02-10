#pragma once
#include "Material.h"
#include "RHIPipeline.h"
#include "RHITexture.h"
#include "RHISampler.h"
#include "PipelineManager.h"
#include "SamplerManager.h"
#include "Renderer.h"

#pragma region Cosntructor&Destructor
EResult Material::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_PipelineKey = desc->PipelineKey;
	}
	else
	{
		m_PipelineKey = L"Default";
	}
	return EResult::Success;
}
Material* Material::Create(void* arg)
{
	Material* instance = new Material();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
void Material::Free()
{
	__super::Free();
}
#pragma endregion


#pragma region Bind
EResult Material::Bind(uint32 slot)
{
	return __super::Bind(slot);
}
#pragma endregion

