#pragma once
#include "Base.h"
#include "RHIResource.h"
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "RHIShader.h"
#include "RHIPipeline.h"

BEGIN(Engine)

typedef struct tagRHICreateInfo
{
	void* WindowHandle = { nullptr };
	uint32 Width = { 0 };
    uint32 Height = { 0 };
    bool IsVSync = { true };
} RHICREATEINFO;

class ENGINE_API RHI : public Base
{
protected:
    RHI() {}
    virtual ~RHI() = default;
    virtual EResult Initialize(void* arg) PURE;
public:
    virtual void Free() override {};
public:
    virtual EResult Resize(uint32 width, uint32 height) { m_SwapChainWidth = width; m_SwapChainHeight = height; return EResult::Success; }
public:
    virtual EResult BeginFrame() PURE;
    virtual EResult EndFrame() PURE;


#pragma region Create Resources
public:
	virtual RHIBuffer* CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type) PURE;
	virtual RHIBuffer* CreateVertexBuffer(void* data, uint32 size, uint32 stride) PURE;
	virtual RHIBuffer* CreateIndexBuffer(void* data, uint32 size, uint32 stride) PURE;
public:
    virtual RHITexture* CreateTextureFromFile(const char* filename) PURE;
    virtual RHITexture* CreateTextureFromFile(const wchar* filename) PURE;
    virtual RHITexture* CreateTextureFromMemory(void* data, uint32 size) PURE;
	virtual RHITexture* CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) PURE;
	virtual RHITexture* CreateTextureCube(void* data, uint32 size, uint32 mipLevels) PURE;
	virtual RHITexture* CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels) PURE;
	virtual RHITexture* CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) PURE;
	virtual RHITexture* CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) PURE;
	virtual RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) PURE;
public:
	//virtual RHIShader* CreateShader(void* arg) PURE;
    //virtual RHIPipeline* CreatePipeline(void* arg) PURE;
#pragma endregion

#pragma region Bind Resources
public:
    virtual EResult BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil) PURE;
	virtual EResult BindTexture(RHITexture* texture, uint32 slot) PURE;
    virtual EResult BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil) PURE;
public:
    virtual EResult BindShader(RHIShader* shader) PURE;
public:
    virtual EResult BindPipeline(void* arg) PURE;
    virtual EResult BindVertexBuffer(RHIBuffer* vertexBuffer) { if (!vertexBuffer) return EResult::Fail; m_VertexBuffer = vertexBuffer; return EResult::Success; }
    virtual EResult BindIndexBuffer(RHIBuffer* indexBuffer) { if (!indexBuffer) return EResult::Fail; m_IndexBuffer = indexBuffer;  return EResult::Success; }
    virtual EResult BindConstantBuffer(void* arg, uint32 slot) PURE;
    virtual EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) PURE;
#pragma endregion

#pragma region Clear Resources
public:
    virtual EResult ClearRenderTarget(RHITexture* renderTarget, vec4 color) PURE;
	virtual EResult ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil) PURE;
#pragma endregion

#pragma region Draw
public:
    virtual EResult Draw(uint32 count) PURE;
    virtual EResult DrawIndexed(uint32 count) PURE;
    virtual EResult DrawIndexedInstanced() PURE;
#pragma endregion

#pragma region Setter
    virtual EResult SetClearColor(vec4 color) PURE;
    virtual EResult SetViewport(int32 x, int32 y, uint32 width, uint32 height) PURE;
    void SetSwapChainWidth(uint32 width) { m_SwapChainWidth = width; }
    void SetSwapChainHeight(uint32 height) { m_SwapChainHeight = height; }
#pragma endregion

#pragma region Getter
    virtual void* GetNativeRHI() const PURE;
    uint32 GetSwapChainWidth() { return m_SwapChainWidth; }
    uint32 GetSwapChainHeight() { return m_SwapChainHeight; }
#pragma endregion

#pragma region Variable
protected:
    RHIBuffer* m_VertexBuffer = { nullptr };
    RHIBuffer* m_IndexBuffer = { nullptr };
protected:
	RHIShader* m_CurrentShader = { nullptr };
protected:
    uint32 m_SwapChainWidth = { 0 };
    uint32 m_SwapChainHeight = { 0 };
	RHITexture* m_BackBuffer = { nullptr };
protected:
	static const uint32 MAX_RENDER_TARGET_COUNT = 8;
    static const uint32 MAX_TEXTURE_SLOTS = 128;

	RHITexture* m_CurrentRenderTargets[MAX_RENDER_TARGET_COUNT] = {nullptr};
	uint32 m_CurrentRenderTargetCount = { 0 };
	RHITexture* m_CurrentDepthStencil = { nullptr };
	RHITexture* m_CurrentTextures[MAX_TEXTURE_SLOTS] = { nullptr };
#pragma endregion
};

END