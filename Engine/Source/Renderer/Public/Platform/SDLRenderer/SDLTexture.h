#pragma once

#include "RHIResource.h"

struct SDL_Texture;
struct SDL_Surface;
BEGIN(Engine)
class SDLTexture final : public RHITexture
{
	friend class SDLRendererRHI;
#pragma region Constructor&Destructor
private:
	SDLTexture(uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) :
		RHITexture{width, height}
	{}

	SDLTexture(SDL_Renderer* renderer, SDL_Surface* surface) :
		RHITexture{static_cast<uint32>(surface->w), static_cast<uint32>(surface->h)}
	{
		m_Texture = SDL_CreateTextureFromSurface(renderer, surface);
	}

	SDLTexture(SDL_Renderer* renderer, SDL_PixelFormat format, SDL_TextureAccess access, uint32 w, uint32 h) :
		RHITexture{ w, h }
	{
		m_Texture = SDL_CreateTexture(renderer, format, access, w, h);
	}

	virtual ~SDLTexture() = default;
public:
	virtual void Free() override {};
#pragma endregion
public:
	virtual void* GetNativeHandle() const override { return m_Texture; }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		if (m_Texture)
		{
			SDL_DestroyTexture(m_Texture);
		}
		m_Texture = static_cast<SDL_Texture*>(nativeHandle);
	}
private:
	SDL_Texture* m_Texture = { nullptr };
};
END