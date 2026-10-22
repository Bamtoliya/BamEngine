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
	ClearPipelineCache();
	Safe_Release(m_DefaultPipeline);
	Safe_Release(m_RHI);
	__super::Free();
}
#pragma endregion

#pragma region Pipeline Management
RHIPipeline* PipelineManager::GetOrCreatePipeline(const RHIPipelineDesc& desc)
{
	if (!m_RHI) return nullptr;

	if (m_LastPipelineDesc && m_LastPipeline && *m_LastPipelineDesc == desc)
		return m_LastPipeline;

	auto it = m_Pipelines.find(desc);
	if (it == m_Pipelines.end())
	{
		RHIPipeline* pipeline = m_RHI->CreatePipeline(desc);
		if (!pipeline) return nullptr;

		const auto [entry, inserted] = m_Pipelines.emplace(desc, pipeline);
		if (!inserted) Safe_Release(pipeline);
		it = entry;
	}

	m_LastPipelineDesc = &it->first;
	m_LastPipeline = it->second;
	return m_LastPipeline;
}

void PipelineManager::RebuildPipelinesWithShader(RHIShader* shader)
{
	if (!shader) return;
	for (auto& [desc, pipeline] : m_Pipelines)
	{
		if (desc.vertexShader == shader || desc.pixelShader == shader ||
			desc.computeShader == shader || desc.geometryShader == shader ||
			desc.hullShader == shader || desc.domainShader == shader)
		{
			if (pipeline)
			{
				pipeline->Rebuild();
			}
		}
	}
}

void PipelineManager::ClearPipelineCache()
{
	m_LastPipelineDesc = nullptr;
	m_LastPipeline = nullptr;
	RELEASE_MAP(m_Pipelines);
}
#pragma endregion