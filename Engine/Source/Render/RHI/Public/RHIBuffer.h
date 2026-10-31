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
	Storage,
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
	virtual void* GetMappedPointer() const { return nullptr; }
#pragma endregion

#pragma region Setter
public:
	virtual void SetData(const void* data, uint32 size) BAM_PURE;
public:
	// 새 정점·인덱스 버퍼의 초기 업로드용입니다.
	// data는 이번 청크의 시작 주소이며, offset은 목적지의 바이트 오프셋입니다.
	EResult UploadRange(const void* data, uint32 size, uint32 offset)
	{
		if (m_BufferType != ERHIBufferType::Vertex && m_BufferType != ERHIBufferType::Index)
			return EResult::NotImplemented;

		if (!data || size == 0 || offset > m_Size || size > m_Size - offset)
			return EResult::InvalidArgument;

		if ((offset % 4) != 0 || (size % 4) != 0)
			return EResult::InvalidArgument;

		return UploadRangeInternal(data, size, offset);
	}

	// complete=true는 미완료 청크가 없다는 의미입니다.
	// 버퍼 전체가 채워졌는지는 호출자가 업로드 바이트 수로 관리합니다.
	virtual EResult QueryUploadComplete(bool& complete)
	{
		complete = false;
		return EResult::NotImplemented;
	}

protected:
	virtual EResult UploadRangeInternal(const void* data, uint32 size, uint32 offset)
	{
		return EResult::NotImplemented;
	}
#pragma endregion

protected:
	ERHIBufferType m_BufferType = { ERHIBufferType::Vertex };
	uint32 m_Size = { 0 };
	uint32 m_Stride = { 0 };
};
END