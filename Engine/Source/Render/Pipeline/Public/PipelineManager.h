#pragma once

#include "RHIPipeline.h"

struct PipelineManagerDesc
{
	RHI* rhi = { nullptr };
};

BEGIN(Engine)
class ENGINE_API PipelineManager : public Base
{
	DECLARE_SINGLETON(PipelineManager)
	using DESC = PipelineManagerDesc;
#pragma region Constructor&Destructor
private:
	PipelineManager() {}
	virtual ~PipelineManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Pipeline Management
public:
	RHIPipeline* GetOrCreatePipeline(const RHIPipelineDesc& desc);
	void RebuildPipelinesWithShader(RHIShader* shader);
	void ClearPipelineCache();
public:
	void SetDefaultPipeline(RHIPipeline* pipeline) { m_DefaultPipeline = pipeline; }
	RHIPipeline* GetDefaultPipeline() const { return m_DefaultPipeline; }
#pragma endregion


#pragma region Variable
private:
	unordered_map<RHIPipelineDesc, RHIPipeline*, hash<RHIPipelineDesc>> m_Pipelines;
	RHIPipeline* m_DefaultPipeline = { nullptr };
	RHI* m_RHI = { nullptr };
	const RHIPipelineDesc* m_LastPipelineDesc = nullptr;
	RHIPipeline* m_LastPipeline = nullptr;
#pragma endregion


};
END