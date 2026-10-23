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
public:
	EResult QueryUploadComplete(bool& complete) override;
protected:
	EResult UploadRangeInternal(const void* data, uint32 size, uint32 offset) override;
private:
	SDL_GPUFence* m_UploadFence = nullptr;
	SDL_GPUBuffer* m_GPUBuffer = { nullptr };
};
END