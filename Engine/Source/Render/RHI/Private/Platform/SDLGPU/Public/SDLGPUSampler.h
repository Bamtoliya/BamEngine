#pragma once

#include "RHISampler.h"

BEGIN(Engine)
class ENGINE_API SDLGPUSampler final : public RHISampler
{
#pragma region Constructor&Destructor
private:
	SDLGPUSampler(RHI* rhi, const DESC& desc) :
		RHISampler(rhi, desc)
	{
	}
	virtual ~SDLGPUSampler() = default;
	EResult Initialize(const DESC& desc);
public:
	static SDLGPUSampler* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
#pragma endregion
public:
	virtual void* GetNativeHandle() const override { return m_Sampler; }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_Sampler = static_cast<SDL_GPUSampler*>(nativeHandle);
	}
private:
	SDL_GPUSampler* m_Sampler = { nullptr };
};
END