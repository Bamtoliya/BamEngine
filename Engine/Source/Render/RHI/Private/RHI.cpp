#pragma once

#include "RHI.h"

void RHI::Free()
{
	Safe_Release(m_VertexBuffer);
	Safe_Release(m_IndexBuffer);
	Safe_Release(m_CurrentShader);
	Safe_Release(m_CurrentPipeline);
	Safe_Release(m_BackBuffer);
}