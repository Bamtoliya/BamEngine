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
public:
	virtual void SetVertexBuffer(void* arg) override {};
	virtual void SetIndexBuffer(void* arg) override {};

#pragma region Bind
	virtual void BindPipeline(void* arg) override {};
	virtual void BindVertexBuffer(void* arg) override {};
	virtual void BindIndexBuffer(void* arg) override {};
	virtual void BindConstantBuffer(void* arg, uint32 slot) override {};
	virtual void BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) override {};
#pragma endregion

#pragma region Draw
	virtual void Draw(uint32 count) override {};
	virtual void DrawIndexed(uint32 count) override {};
	virtual void DrawIndexedInstanced() override {};
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