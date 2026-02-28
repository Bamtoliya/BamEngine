#pragma once 

#include "RHIBuffer.h"

BEGIN(Engine)
class SDLBuffer final : public RHIBuffer
{
	friend class SDLRendererRHI;
private:
	SDLBuffer(ERHIBufferType bufferType, uint32 size, uint32 stride) : 
		RHIBuffer(nullptr, bufferType, size, stride)
	{
		m_Data.resize(size);
	}
	virtual ~SDLBuffer() = default;
public:
	virtual void* GetNativeHandle() const override { return (void*)m_Data.data(); }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		if (nativeHandle)
		{
			memcpy(m_Data.data(), nativeHandle, m_Size);
		}
	}

public:
	virtual void SetData(const void* data, uint32 size) override
	{
		if (size > m_Data.size())
		{
			m_Size = size;
			m_Data.resize(size);
		}
		memcpy(m_Data.data(), data, size);
	}
private:
	vector<uint8> m_Data;
};
END