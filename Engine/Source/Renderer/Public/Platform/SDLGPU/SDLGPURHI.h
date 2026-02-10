#pragma once

#include "RHI.h"
#include <SDL3/SDL_GPU.h>


BEGIN(Engine)
class ENGINE_API SDLGPURHI final : public RHI
{
#pragma region Constructor&Destructor
private:
	SDLGPURHI() {}
	virtual ~SDLGPURHI() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static RHI* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

	virtual EResult Resize(uint32 width, uint32 height) override;
public:
	virtual EResult BeginFrame() override;
	virtual EResult EndFrame() override;

#pragma region Create Resources
public:
	virtual RHIBuffer* CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type) override;
	virtual RHIBuffer* CreateVertexBuffer(void* data, uint32 size, uint32 stride) override;
	virtual RHIBuffer* CreateIndexBuffer(void* data, uint32 size, uint32 stride) override;
public:
	virtual RHITexture* CreateTextureFromFile(const char* filename) override;
	virtual RHITexture* CreateTextureFromFile(const wchar* filename) override;
	virtual RHITexture* CreateTextureFromMemory(void* data, uint32 size) override;
	virtual RHITexture* CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	virtual RHITexture* CreateTextureCube(void* data, uint32 size, uint32 mipLevels) override;
	virtual RHITexture* CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels) override;
	virtual RHITexture* CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	virtual RHITexture* CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	virtual RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) override;
public:
	virtual RHIPipeline* CreatePipeline(const tagRHIPipelineDesc& desc) override;
public:
	virtual RHISampler* CreateSampler(const tagRHISamplerDesc& desc) override;
public:
	virtual RHIShader* CreateShader(const tagRHIShaderDesc& desc) override;
public:
	EResult UploadTextureData(SDL_GPUTexture* texture, void* data, uint32 width, uint32 height);
	EResult UploadBufferData(SDL_GPUBuffer* buffer, void* data, uint32 size);
#pragma endregion

#pragma region Bind Resources
public:
	virtual EResult BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil)  override;
	virtual EResult BindTexture(RHITexture* texture, uint32 slot) override;
	virtual EResult BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot) override;
	virtual EResult BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil) override;
public:
	virtual EResult BindShader(RHIShader* shader) override;
public:
	virtual EResult BindPipeline(RHIPipeline* pipeline) override;
public:
	virtual EResult BindSampler(RHISampler* sampler) override { return EResult::NotImplemented; }
	virtual EResult BindConstantBuffer(void* arg, uint32 slot) override;
	virtual EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) override { return EResult::NotImplemented; }
#pragma endregion

#pragma region Clear Resources
public:
	virtual EResult ClearRenderTarget(RHITexture* renderTarget, vec4 color) override;
	virtual EResult ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil) override;
	virtual EResult ClearRenderPass();
#pragma endregion

#pragma region Draw
	virtual EResult Draw(uint32 count) override;
	virtual EResult DrawIndexed(uint32 count) override;
	virtual EResult DrawIndexedInstanced() override { return EResult::NotImplemented; }
	virtual EResult DrawTexture(RHITexture* texture) override;
#pragma endregion

#pragma region Setter
	virtual EResult SetClearColor(vec4 color) override;
	virtual EResult SetViewport(int32 x, int32 y, uint32 width, uint32 height) override;
#pragma endregion

#pragma region Getter
	virtual void* GetNativeRHI() const override { return m_Device; }
	virtual void* GetCurrentCommandBuffer() const override { return m_CurrentCommandBuffer; }
#pragma endregion

#pragma region Variables
private:
	SDL_Window* m_Window = { nullptr };
	SDL_GPUDevice* m_Device = { nullptr };
	SDL_GPUCommandBuffer* m_CurrentCommandBuffer = { nullptr };
	SDL_GPURenderPass* m_CurrentRenderPass = { nullptr };
private:
	mat4 m_WorldMatrix = glm::identity<mat4>();
	vec4 m_MaterialColor = vec4(1.0f);
	vec4 m_ClearColor = vec4(0.1f, 0.1f, 0.1f, 1.0f);
#pragma endregion
};
END