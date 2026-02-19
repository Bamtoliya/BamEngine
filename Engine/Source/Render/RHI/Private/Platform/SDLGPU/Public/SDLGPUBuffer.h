#pragma once 

#include "RHIBuffer.h"

BEGIN(Engine)
class SDLGPUBuffer final : public RHIBuffer
{
	friend class SDLGPURHI;
private:
	SDLGPUBuffer(RHI* rhi, const DESC& desc) :
		RHIBuffer(rhi, desc)
	{
		m_Data.resize(m_Size);
	}
	virtual ~SDLGPUBuffer() = default;
	EResult Initialize(const DESC& desc);
public:
	static SDLGPUBuffer* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
public:
	virtual void* GetNativeHandle() const override { return m_GPUBuffer; }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_GPUBuffer = static_cast<SDL_GPUBuffer*>(nativeHandle);
	}

public:
	virtual void SetData(const void* data, uint32 size) override;
private:
	vector<uint8> m_Data;
	SDL_GPUBuffer* m_GPUBuffer = { nullptr };
};
END