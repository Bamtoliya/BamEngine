#pragma once

#include "RHI.h"
#include "RHITexture.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"

void RHI::Free()
{
	Safe_Release(m_VertexBuffer);
	Safe_Release(m_IndexBuffer);
	Safe_Release(m_CurrentShader);
	Safe_Release(m_CurrentPipeline);
	Safe_Release(m_BackBuffer);
}

EResult RHI::BindVertexBuffer(RHIBuffer* vertexBuffer)
{
	if (!vertexBuffer)  return EResult::Fail;
	Safe_Release(m_VertexBuffer);
	m_VertexBuffer = vertexBuffer;
	Safe_AddRef(m_VertexBuffer);
	return EResult::Success;
}

EResult RHI::BindIndexBuffer(RHIBuffer* indexBuffer)
{
	if (!indexBuffer) return EResult::Fail;
	Safe_Release(m_IndexBuffer);
	m_IndexBuffer = indexBuffer;
	Safe_AddRef(m_IndexBuffer);
	return EResult::Success;
}