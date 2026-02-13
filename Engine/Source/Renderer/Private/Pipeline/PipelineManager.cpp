#pragma once

#include "PipelineManager.h"
#include "PipelineManager.h"

IMPLEMENT_SINGLETON(PipelineManager)

#pragma region Constructor&Destructor

EResult PipelineManager::Initialize(void* arg)
{
	if (!arg) return EResult::Fail;
	CAST_DESC
	m_RHI = desc->rhi;
	Safe_AddRef(m_RHI);
	return EResult::Success;
}
void PipelineManager::Free()
{
	RELEASE_MAP(m_Pipelines);
	Safe_Release(m_DefaultPipeline);
	Safe_Release(m_RHI);
	__super::Free();
}
#pragma endregion

#pragma region Pipeline Management
RHIPipeline* PipelineManager::GetOrCreatePipeline(const tagRHIPipelineDesc& desc)
{
	auto it = m_Pipelines.find(desc);
	if (it != m_Pipelines.end())
	{
		return it->second;
	}
	RHIPipeline* pipeline = m_RHI->CreatePipeline(desc);
	if (pipeline)
	{
		m_Pipelines[desc] = pipeline;
	}
	return pipeline;
}
#pragma endregion