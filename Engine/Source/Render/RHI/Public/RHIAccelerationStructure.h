#pragma once

#include "RHIResource.h"

BEGIN(Engine)

class RHIBuffer;
class RHIAccelerationStructure;

enum class ERHIAccelerationStructureType : uint8
{
	BottomLevel,
	TopLevel,
};

// BLAS 입력: 정적 삼각형 리스트 하나. 위치는 float3(R32G32B32_FLOAT)이며 stride로 건너뛴다.
// indexBuffer가 없으면 비인덱스 기하로 처리하고 vertexCount는 3의 배수여야 한다.
struct RHIRaytracingGeometryDesc
{
	RHIBuffer* positionBuffer = nullptr;
	uint32 vertexCount = 0;
	uint32 vertexStride = 0;
	RHIBuffer* indexBuffer = nullptr;
	uint32 indexCount = 0;
	uint32 indexStride = 0; // 2 또는 4
};

// TLAS 인스턴스: BLAS 하나와 월드 행렬. instanceID는 하위 24비트만 사용한다.
struct RHIRaytracingInstanceDesc
{
	mat4 worldMatrix = mat4(1.0f);
	RHIAccelerationStructure* bottomLevel = nullptr;
	uint32 instanceID = 0;
	uint8 instanceMask = 0xFF;
};

// 레이트레이싱 가속 구조(BLAS/TLAS)의 결과 버퍼를 소유한다.
// 해제 시 GPU 사용이 끝날 때까지 백엔드가 지연 해제한다.
class ENGINE_API RHIAccelerationStructure : public RHIResource
{
protected:
	RHIAccelerationStructure(RHI* rhi, ERHIAccelerationStructureType type) :
		RHIResource(rhi, ERHIResourceType::Unknown), m_Type{ type } {}
	virtual ~RHIAccelerationStructure() = default;
public:
	ERHIAccelerationStructureType GetType() const { return m_Type; }
	uint64 GetResultSizeInBytes() const { return m_ResultSizeInBytes; }
	virtual void SetNativeHandle(void* nativeHandle) override {}
protected:
	ERHIAccelerationStructureType m_Type = ERHIAccelerationStructureType::BottomLevel;
	uint64 m_ResultSizeInBytes = 0;
};

END
