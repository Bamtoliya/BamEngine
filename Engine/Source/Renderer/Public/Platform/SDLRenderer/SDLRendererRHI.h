#pragma once

#include "RHI.h"
#include <SDL3/SDL.h>

BEGIN(Engine)
class ENGINE_API SDLRendererRHI final : public RHI
{
#pragma region Constructor&Destructor
private:
	SDLRendererRHI() {}
	virtual ~SDLRendererRHI() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static RHI* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

	virtual void Resize() override {};
public:
	virtual EResult BeginFrame() override;
	virtual EResult EndFrame() override;

#pragma region Create Resources
public:
	virtual RHIBuffer* CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type) override;
	virtual RHIBuffer* CreateVertexBuffer(void* data, uint32 size, uint32 stride) override;
	virtual RHIBuffer* CreateIndexBuffer(void* data, uint32 size, uint32 stride) override;

	virtual RHITexture* CreateTextureFromFile(const char* filename) override;
	virtual RHITexture* CreateTextureFromMemory(void* data, uint32 size) override;
	virtual RHITexture* CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	virtual RHITexture* CreateTextureCube(void* data, uint32 size, uint32 mipLevels) override;
	virtual RHITexture* CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels) override;
	virtual RHITexture* CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	virtual RHITexture* CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	virtual RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) override;
#pragma endregion

#pragma region Bind
	virtual EResult BindPipeline(void* arg) override { return EResult::NotImplemented; }
	virtual EResult BindConstantBuffer(void* arg, uint32 slot) override { return EResult::NotImplemented;  }
	virtual EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) override { return EResult::NotImplemented; }
#pragma endregion

#pragma region Draw
	virtual EResult Draw(uint32 count) override;
	virtual EResult DrawIndexed(uint32 count) override;
	virtual EResult DrawIndexedInstanced() override { return EResult::NotImplemented; }
#pragma endregion

#pragma region Setter
	virtual void SetClearColor(void* arg) override {};
	virtual void SetViewport(void* arg) override {};
#pragma endregion

#pragma region Getter
	virtual void* GetNativeRHI() const override { return m_Renderer; }
#pragma endregion

	virtual void CreateTexture(void* data, uint32 size) override {};
	

#pragma region Variables
private:
	SDL_Window*		m_Window		= { nullptr };
	SDL_Renderer*	m_Renderer		= { nullptr };
	SDL_Texture*	m_BackBuffer	= { nullptr };
#pragma endregion



};
END