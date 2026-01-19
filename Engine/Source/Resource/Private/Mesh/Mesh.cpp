#pragma once
#include "Mesh.h"
#include "Renderer.h"

#pragma region Constructor&Destructor
EResult Mesh::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	MESHDESC* meshDesc = reinterpret_cast<MESHDESC*>(arg);
	m_VertexCount = static_cast<uint32>(meshDesc->VertexCount);
	m_IndexCount = static_cast<uint32>(meshDesc->IndexCount);

	RHI* rhi = Renderer::Get().GetRHI();

	// Create Vertex Buffer
	{
		RHIBUFFERDESC vertexBufferDesc = {};
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

