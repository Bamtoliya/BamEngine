#pragma once
#include "SDLRendererRHI.h"


#pragma region Constructor&Destructor
EResult SDLRendererRHI::Initialize(void* arg)
{
	RHICREATEINFO* pDesc = reinterpret_cast<RHICREATEINFO*>(arg);
	m_Window = reinterpret_cast<SDL_Window*>(pDesc->WindowHandle);
	m_Renderer = SDL_CreateRenderer(m_Window, nullptr);
	if (!m_Window || !m_Renderer) return EResult::Fail;

	int32 width, height;
	SDL_GetWindowSize(m_Window, &width, &height);
	m_BackBuffer = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
	if (!m_BackBuffer)
	{
		return EResult::Fail;
	}
	return EResult::Success;
}

RHI* SDLRendererRHI::Create(void* arg)
{
	SDLRendererRHI* Instance = new SDLRendererRHI();
	if (IsFailure(Instance->Initialize(arg)))
	{
		delete Instance;
		Instance = nullptr;
		fmt::print(stderr, "SDLRendererRHI Creation Failed\n");
	}
	return Instance;
}

void SDLRendererRHI::Free()
{
	if (m_BackBuffer)
	{
		SDL_DestroyTexture(m_BackBuffer);
		m_BackBuffer = nullptr;
	}
	if (m_Renderer)
	{
		SDL_DestroyRenderer(m_Renderer);
		m_Renderer = nullptr;
	}
}
#pragma endregion


EResult SDLRendererRHI::BeginFrame()
{
	if (m_Renderer)
	{
		SDL_SetRenderTarget(m_Renderer, m_BackBuffer);
		SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
		SDL_RenderClear(m_Renderer);
		return EResult::Success;
	}
	return EResult::Fail;
}


EResult SDLRendererRHI::EndFrame()
{
	if (m_Renderer)
	{
		SDL_RenderPresent(m_Renderer);
		return EResult::Success;
	}
	return EResult::Fail;
}

