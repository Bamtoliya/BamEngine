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
	if (!arg) return EResult::Fail;
	CAST_DESC
	m_VertexShaderHandle = desc->VertexShaderHandle;
	m_PixelShaderHandle = desc->PixelShaderHandle;

	m_BlendMode = desc->BlendMode;
	m_CullMode = desc->CullMode;
	m_FillMode = desc->FillMode;
	m_DepthMode = desc->DepthMode;
	m_DepthCompareOp = desc->DepthCompareOp;
	
	return EResult::Success;
}
Material* Material::Create(void* arg)
{
	Material* instance = new Material();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
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

