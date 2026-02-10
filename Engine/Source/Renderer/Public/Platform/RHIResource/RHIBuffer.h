#pragma once

#include "RHIResource.h"
enum class ERHIBufferType
{
	Vertex,
	Index,
	Constant,
	Structured,
	IndirectArgs,
	Readback,
	Upload,
};

struct tagRHIBufferDesc
{
	ERHIBufferType BufferType = ERHIBufferType::Vertex;
	uint32 Size = 0;
	uint32 Stride = 0;
	void* InitialData = nullptr;
};

BEGIN(Engine)
class ENGINE_API RHIBuffer : public RHIResource
{
protected:
	using DESC = tagRHIBufferDesc;
	RHIBuffer(RHI* rhi, ERHIBufferType bufferType, uint32 size, uint32 stride) :
		RHIResource(rhi, ERHIResourceType::Buffer),
		m_BufferType{ bufferType },
		m_Size{ size },
		m_Stride{ stride } {
	}
	RHIBuffer(RHI* rhi, DESC desc) :
		RHIResource(rhi, ERHIResourceType::Buffer),
		m_BufferType{ desc.BufferType },
		m_Size{ desc.Size },
		m_Stride{ desc.Stride }
	{
	}
	virtual ~RHIBuffer() = default;

#pragma region Getter
public:
	ERHIBufferType GetBufferType() const { return m_BufferType; }
	uint32 GetSize() const { return m_Size; }
	uint32 GetStride() const { return m_Stride; }
#pragma endregion

#pragma region Setter
public:
	virtual void SetData(const void* data, uint32 size) PURE;
#pragma endregion

protected:
	ERHIBufferType m_BufferType = { ERHIBufferType::Vertex };
	uint32 m_Size = { 0 };
	uint32 m_Stride = { 0 };
};
END