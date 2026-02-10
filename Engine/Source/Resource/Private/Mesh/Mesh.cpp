#pragma once
#include "Mesh.h"
#include "Renderer.h"

#pragma region Constructor&Destructor
EResult Mesh::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	DESC* meshDesc = reinterpret_cast<DESC*>(arg);
	m_VertexCount = static_cast<uint32>(meshDesc->VertexCount);
	m_IndexCount = static_cast<uint32>(meshDesc->IndexCount);

	RHI* rhi = Renderer::Get().GetRHI();

	// Create Vertex Buffer
	{
		tagRHIBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.BufferType = ERHIBufferType::Vertex;
		uint32 totalSize = meshDesc->VertexStride * m_VertexCount;
		m_VertexBuffer = rhi->CreateBuffer(
			meshDesc->VertexData,
			totalSize,
			meshDesc->VertexStride,
			ERHIBufferType::Vertex
		);
		if (!m_VertexBuffer)
			return EResult::Fail;
	}

	// Create Index Buffer
	{
		uint32 totalSize = meshDesc->IndexStride * m_IndexCount;
		m_IndexBuffer = rhi->CreateBuffer(
			meshDesc->IndexData,
			totalSize,
			meshDesc->IndexStride,
			ERHIBufferType::Index
		);
		if (!m_IndexBuffer)
			return EResult::Fail;
	}
	return EResult::Success;
}

Mesh* Mesh::Create(void* arg)
{
	Mesh* instance = new Mesh();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

void Mesh::Free()
{
	Base::Free();
	Safe_Release(m_VertexBuffer);
	Safe_Release(m_IndexBuffer);
}
#pragma endregion

#pragma region Setter
EResult Mesh::SetVertexBuffer(const void* data, uint32 vertexCount)
{
	if (!data || vertexCount == 0)
		return EResult::InvalidArgument;
	// 현재 메쉬가 사용하는 버텍스 구조체의 크기 (예: Position + UV)
	// 기존에 생성된 버퍼가 있다면 그 stride를 유지하거나, 
	// 혹은 Mesh가 Stride 정보를 별도로 알고 있어야 합니다.
	// 여기서는 최초 생성 시 결정된 Stride를 유지한다고 가정합니다.
	uint32 stride = (m_VertexBuffer) ? m_VertexBuffer->GetStride() : sizeof(Vertex); // Vertex 구조체 크기

	uint32 totalSize = vertexCount * stride;

	// 1. 버퍼가 이미 존재하면 -> 데이터만 교체 (Update)
	if (m_VertexBuffer != nullptr)
	{
		// 만약 Stride가 바뀌는 상황이라면 새로 만들어야 하지만,
		// 스프라이트 렌더링에서는 Stride가 바뀔 일이 거의 없습니다.
		m_VertexBuffer->SetData(data, totalSize);
	}
	// 2. 버퍼가 없으면 -> 새로 생성 (Create)
	else
	{
		// RHIResource 관리자 등을 통해 생성하거나 직접 생성
		// (여기서는 예시로 직접 new 하지만, 실제 엔진에서는 Factory 패턴 사용 권장)
		tagRHIBufferDesc desc;
		desc.BufferType = ERHIBufferType::Vertex;
		desc.Size = totalSize;
		desc.Stride = stride;
		desc.InitialData = (void*)data;

		// SDL용 버퍼 생성 (실제로는 Renderer가 Factory로 만들어줘야 함)
		RHI* rhi = Renderer::Get().GetRHI();
		m_VertexBuffer = rhi->CreateVertexBuffer((void*)data, totalSize, stride);
		m_VertexBuffer->SetData(data, totalSize);
	}

	// 메쉬의 버텍스 개수 정보 갱신
	m_VertexCount = vertexCount;

	return EResult::Success;
}

//EResult Mesh::SetIndexBuffer(const void* data, uint32 indexCount)
//{
//	if (!data || indexCount == 0)
//		return EResult::InvalidArgument;
//	Safe_Release(m_IndexBuffer);
//	m_IndexBuffer = indexBuffer;
//	Engine::Safe_AddRef(m_IndexBuffer);
//	return EResult::Success;
//}
#pragma endregion

#pragma region Bind
EResult Mesh::Bind(uint32 slot)
{
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;
	rhi->BindVertexBuffer(m_VertexBuffer);
	rhi->BindIndexBuffer(m_IndexBuffer);
	return EResult::Success;
}
#pragma endregion
