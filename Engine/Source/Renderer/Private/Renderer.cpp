#pragma once

#include "Renderer.h"
#include "RHI.h"
#include "SDLRendererRHI.h"


IMPLEMENT_SINGLETON(Renderer)


#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{
	RENDERERDESC* pDesc = reinterpret_cast<RENDERERDESC*>(arg);
	RHICREATEINFO RHIDesc = {};

	RHIDesc.WindowHandle = pDesc->WindowHandle;
	RHIDesc.Width = pDesc->Width;
	RHIDesc.Height = pDesc->Height;
	RHIDesc.IsVSync = pDesc->IsVSync;

	if (!pDesc) return EResult::Fail;
	switch (pDesc->RHIType)
	{
	case ERHIType::SDLRenderer:
		m_RHI = SDLRendererRHI::Create(&RHIDesc);
		break;
	default:
		return EResult::Fail;
	}
	if (!m_RHI) return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	Safe_Release(m_RHI);
}
#pragma endregion



EResult Renderer::BeginFrame()
{
	if (m_RHI)
	{
		return m_RHI->BeginFrame();
	}
	return EResult::Fail;
}

EResult Renderer::EndFrame()
{
	if (m_RHI)
	{
		return m_RHI->EndFrame();
	}
	return EResult::Fail;
}