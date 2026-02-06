#pragma once

#include "RHIResource.h"

struct SDL_Texture;
struct SDL_Surface;
BEGIN(Engine)
class SDLTexture final : public RHITexture
{
	friend class SDLRendererRHI;
private:
	SDLTexture(uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) :
		RHITexture{width, height}
	{}

	SDLTexture(SDL_Renderer* renderer, SDL_Surface* surface) :
		RHITexture{static_cast<uint32>(surface->w), static_cast<uint32>(surface->h)}
	{
		m_Texture = SDL_CreateTextureFromSurface(renderer, surface);
	}

	virtual ~SDLTexture() = default;
public:
	virtual void* GetNativeHandle() const override { return m_Texture; }
private:
	SDL_Texture* m_Texture = { nullptr };
};
END