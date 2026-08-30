#pragma once

#include "RHIPipeline.h"

BEGIN(Engine)
class ENGINE_API DirectX12Pipeline final : public RHIPipeline
{
#pragma region Constructor&Destructor
private:
	DirectX12Pipeline(RHI* rhi, const DESC& desc) :
		RHIPipeline(rhi, desc)
	{
	}
	virtual ~DirectX12Pipeline() = default;
	EResult Initialize(const DESC& desc);
public:
	static DirectX12Pipeline* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
#pragma endregion
public:
	virtual void* GetNativeHandle() const override { return m_Pipeline.Get(); }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_Pipeline = static_cast<ID3D12PipelineState*>(nativeHandle);
	}
private:
	ComPtr<ID3D12PipelineState> m_Pipeline;
};
END