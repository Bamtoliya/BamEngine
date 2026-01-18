#pragma once

#include "Pipeline.h"


#pragma region Constructor&Destructor
EResult Pipeline::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	// Create RHI Pipeline
	{
		//RHIPIPELINEDESC* pipelineDesc = reinterpret_cast<RHIPIPELINEDESC*>(arg);
		//RHI* rhi = Renderer::Get().GetRHI();
		// For simplicity, assuming RHI has a method CreatePipeline (not shown in previous snippets)
		// m_RHIPipeline = rhi->CreatePipeline(pipelineDesc);
		// if (!m_RHIPipeline)
		//     return EResult::Fail;
	}
	return EResult::Success;
}
Pipeline* Pipeline::Create(void* arg)
{
	Pipeline* instance = new Pipeline();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}
Pipeline* Pipeline::Clone(void* arg)
{
	return nullptr;
}
void Pipeline::Free()
{
	Base::Free();
	Safe_Release(m_RHIPipeline);
}
#pragma endregion