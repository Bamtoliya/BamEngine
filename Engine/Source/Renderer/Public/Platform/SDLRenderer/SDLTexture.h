#pragma once

#include "RHIResource.h"

struct SDL_Texture;
BEGIN(Engine)
class SDLTexture final : public RHITexture
{
	friend class SDLRendererRHI;
private:
	SDLTexture(uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) :
		RHITexture{}
	{}

	virtual ~SDLTexture() = default;
public:
	virtual void* GetNativeHandle() const override { return m_Texture; }
private:
	SDL_Texture* m_Texture = { nullptr };
};
END