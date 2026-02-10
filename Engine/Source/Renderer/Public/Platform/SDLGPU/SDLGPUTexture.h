#pragma once

#include "SDLGPURHI.h"
#include "RHITexture.h"

BEGIN(Engine)
class SDLGPUTexture final : public RHITexture
{
#pragma region Constructor&Destructor
private:
	SDLGPUTexture(SDLGPURHI* rhi) : RHITexture{ rhi } {}
	virtual ~SDLGPUTexture() = default;
	EResult Initialize(const SDL_GPUTextureCreateInfo& createInfo);
public:
	static SDLGPUTexture* Create(SDLGPURHI* rhi, const SDL_GPUTextureCreateInfo& createInfo = {}, bool isOwned = true);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	EResult Bind(uint32 slot = 0) override;
#pragma endregion


public:
	virtual void* GetNativeHandle() const override { return m_Texture; }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		if (m_Texture && m_IsOwned)
		{
			SDL_ReleaseGPUTexture(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_Texture);
		}
		m_Texture = static_cast<SDL_GPUTexture*>(nativeHandle);
	}
private:
	SDL_GPUTexture* m_Texture = { nullptr };
	bool m_IsOwned = { true };
};
END