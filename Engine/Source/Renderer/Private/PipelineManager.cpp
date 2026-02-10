#pragma once

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
RHIPipeline* PipelineManager::CreatePipeline(const wstring& name, const tagRHIPipelineDesc& desc)
{
	RHIPipeline* pipeline = m_RHI->CreatePipeline(desc);
	if(pipeline)
	{
		m_Pipelines[name] = pipeline;
		return pipeline;
	}
	return nullptr;
}
RHIPipeline* PipelineManager::GetPipeline(const wstring& name) const
{
	auto it = m_Pipelines.find(name);
	if (it != m_Pipelines.end())
	{
		return it->second;
	}
	return nullptr;
}
EResult PipelineManager::RemovePipeline(const wstring& name)
{
	auto it = m_Pipelines.find(name);
	if (it != m_Pipelines.end())
	{
		Safe_Release(it->second);
		m_Pipelines.erase(it);
		return EResult::Success;
	}
	return EResult::Fail;
}
#pragma endregion