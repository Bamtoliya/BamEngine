#pragma once

#include "RHIPipeline.h"

BEGIN(Engine)
class ENGINE_API SDLGPUPipeline final : public RHIPipeline
{
#pragma region Constructor&Destructor
private:
	SDLGPUPipeline(RHI* rhi, const DESC& desc) :
		RHIPipeline(rhi, desc)
	{
	}
	virtual ~SDLGPUPipeline() = default;
	EResult Initialize(const DESC& desc);
public:
	static SDLGPUPipeline* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
#pragma endregion
public:
	virtual void* GetNativeHandle() const override { return m_Pipeline; }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_Pipeline = static_cast<SDL_GPUGraphicsPipeline*>(nativeHandle);
	}
private:
	SDL_GPUGraphicsPipeline* m_Pipeline = { nullptr };
};
END