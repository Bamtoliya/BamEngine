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
    virtual void Resize() PURE;
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

#pragma region Bind
public:
    virtual EResult BindPipeline(void* arg) PURE;
    virtual EResult BindVertexBuffer(RHIBuffer* vertexBuffer) { if (!vertexBuffer) return EResult::Fail; m_VertexBuffer = vertexBuffer; return EResult::Success; }
    virtual EResult BindIndexBuffer(RHIBuffer* indexBuffer) { if (!indexBuffer) return EResult::Fail; m_IndexBuffer = indexBuffer;  return EResult::Success; }
    virtual EResult BindConstantBuffer(void* arg, uint32 slot) PURE;
    virtual EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) PURE;
#pragma endregion

#pragma region Draw
public:
    virtual EResult Draw(uint32 count) PURE;
    virtual EResult DrawIndexed(uint32 count) PURE;
    virtual EResult DrawIndexedInstanced() PURE;
#pragma endregion

#pragma region Setter
    virtual void SetClearColor(void* arg) PURE;
    virtual void SetViewport(void* arg) PURE;
#pragma endregion

#pragma region Getter
    virtual void* GetNativeRHI() const PURE;
#pragma endregion

protected:
    RHIBuffer* m_VertexBuffer   = { nullptr };
	RHIBuffer* m_IndexBuffer    = { nullptr };
};

END