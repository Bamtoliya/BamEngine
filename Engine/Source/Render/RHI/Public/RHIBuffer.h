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

struct RHIBufferDesc
{
	ERHIBufferType bufferType = ERHIBufferType::Vertex;
	uint32 size = 0;
	uint32 stride = 0;
	void* initialData = nullptr;
};

BEGIN(Engine)
class ENGINE_API RHIBuffer : public RHIResource
{
protected:
	using DESC = RHIBufferDesc;
	RHIBuffer(RHI* rhi, ERHIBufferType bufferType, uint32 size, uint32 stride) :
		RHIResource(rhi, ERHIResourceType::Buffer),
		m_BufferType{ bufferType },
		m_Size{ size },
		m_Stride{ stride } {
	}
	RHIBuffer(RHI* rhi, DESC desc) :
		RHIResource(rhi, ERHIResourceType::Buffer),
		m_BufferType{ desc.bufferType },
		m_Size{ desc.size },
		m_Stride{ desc.stride }
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
	virtual void SetData(const void* data, uint32 size) BAM_PURE;
#pragma endregion

protected:
	ERHIBufferType m_BufferType = { ERHIBufferType::Vertex };
	uint32 m_Size = { 0 };
	uint32 m_Stride = { 0 };
};
END