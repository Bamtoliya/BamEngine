#pragma once

#include "Base.h"
#include "RHIPipeline.h"

struct tagPipelineManagerDesc
{
	RHI* rhi = { nullptr };
};

BEGIN(Engine)
class ENGINE_API PipelineManager : public Base
{
	DECLARE_SINGLETON(PipelineManager)
	using DESC = tagPipelineManagerDesc;
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
	RHIPipeline* CreatePipeline(const wstring& name, const tagRHIPipelineDesc& desc);
	EResult RemovePipeline(const wstring& name);
	RHIPipeline* GetPipeline(const wstring& name) const;

public:
	void SetDefaultPipeline(RHIPipeline* pipeline) { m_DefaultPipeline = pipeline; }
	RHIPipeline* GetDefaultPipeline() const { return m_DefaultPipeline; }
#pragma endregion


#pragma region Variable
private:
	unordered_map<wstring, RHIPipeline*> m_Pipelines;
	RHIPipeline* m_DefaultPipeline = { nullptr };
	RHI* m_RHI = { nullptr };
#pragma endregion


};
END