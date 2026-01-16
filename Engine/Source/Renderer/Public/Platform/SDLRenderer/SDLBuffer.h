#pragma once 

#include "RHIBuffer.h"

BEGIN(Engine)
class SDLBuffer final : public RHIBuffer
{
	friend class SDLRendererRHI;
private:
	SDLBuffer(ERHIBufferType bufferType, uint32 size, uint32 stride) : 
		RHIBuffer(bufferType, size, stride)
	{
		m_Data.resize(size);
	}
	virtual ~SDLBuffer() = default;
public:
	virtual void* GetNativeHandle() const override { return (void*)m_Data.data(); }
private:
	vector<uint8> m_Data;
};
END