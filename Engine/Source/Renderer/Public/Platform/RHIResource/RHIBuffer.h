#pragma once

#include "RHIResource.h"

BEGIN(Engine)
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

typedef struct tagRHIBufferDesc
{
	ERHIBufferType BufferType = ERHIBufferType::Vertex;
	uint32 Size = 0;
	uint32 Stride = 0;
	void* InitialData = nullptr;
} RHIBUFFERDESC;

class ENGINE_API RHIBuffer : public RHIResource
{
protected:
	RHIBuffer(ERHIBufferType bufferType, uint32 size, uint32 stride) :
		RHIResource(ERHIResourceType::Buffer),
		m_BufferType{ bufferType },
		m_Size{ size },
		m_Stride{ stride } {
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