#pragma once

#include "Renderer.h"
#include "RHI.h"
#include "SDLRendererRHI.h"


IMPLEMENT_SINGLETON(Renderer)


#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{
	RENDERERDESC* pDesc = reinterpret_cast<RENDERERDESC*>(arg);
	if (!pDesc) return EResult::Fail;
	switch (pDesc->RHIType)
	{
	case ERHIType::SDLRenderer:
		m_RHI = SDLRendererRHI::Create(pDesc);
		break;
	default:
		return EResult::Fail;
	}
	if (!m_RHI) return EResult::Fail;
	return EResult::Success;
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