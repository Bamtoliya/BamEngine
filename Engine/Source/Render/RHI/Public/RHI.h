#pragma once
#include "Base.h"
#include "RenderTypes.h"

struct tagRHIDesc
{
    void* WindowHandle = { nullptr };
    Engine::uint32 Width = { 0 };
    Engine::uint32 Height = { 0 };
    bool IsVSync = { true };
};

struct tagRHIBufferDesc;
struct tagRHIShaderDesc;
struct tagRHITextureDesc;

struct tagRHIPipelineDesc;

enum class ERHIBufferType;

BEGIN(Engine)
TODO("tagSamplerDesc should move to out of Engine namespace because it is used in both RHI and RenderPass, and it is not related to RHI directly");
struct tagSamplerDesc;
class RHIBuffer;
class RHIShader;
class RHISampler;
class RHITexture;
class RHIPipeline;

class RenderPass;
class ENGINE_API RHI : public Base
{
protected:
	using DESC = tagRHIDesc;
    RHI() {}
    virtual ~RHI() = default;
    virtual EResult Initialize(void* arg) BAM_PURE;
public:
    virtual void Free() override;
public:
    virtual EResult Resize(uint32 width, uint32 height) { m_SwapChainWidth = width; m_SwapChainHeight = height; return EResult::Success; }
public:
    virtual EResult BeginFrame() BAM_PURE;
    virtual EResult EndFrame() BAM_PURE;


#pragma region Create Resources
public:
	virtual RHIBuffer* CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type) BAM_PURE;
	virtual RHIBuffer* CreateVertexBuffer(void* data, uint32 size, uint32 stride) BAM_PURE;
	virtual RHIBuffer* CreateIndexBuffer(void* data, uint32 size, uint32 stride) BAM_PURE;
public:
    virtual RHITexture* CreateTextureFromFile(const char* filename) BAM_PURE;
    virtual RHITexture* CreateTextureFromFile(const wchar* filename) BAM_PURE;
    virtual RHITexture* CreateTextureFromMemory(const tagRHITextureDesc& desc) BAM_PURE;
	virtual RHITexture* CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) BAM_PURE;
	virtual RHITexture* CreateTextureCube(void* data, uint32 size, uint32 mipLevels) BAM_PURE;
	virtual RHITexture* CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels) BAM_PURE;
	virtual RHITexture* CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) BAM_PURE;
	virtual RHITexture* CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) BAM_PURE;
	virtual RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) BAM_PURE;
public:
    virtual RHIPipeline* CreatePipeline(const tagRHIPipelineDesc& desc) BAM_PURE;
public:
    virtual RHISampler* CreateSampler(const tagSamplerDesc& desc) BAM_PURE;
public:
	virtual RHIShader* CreateShader(const tagRHIShaderDesc& desc) BAM_PURE;
#pragma endregion

#pragma region Bind Resources
public:
    virtual EResult BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil) BAM_PURE;
	virtual EResult BindTexture(RHITexture* texture, uint32 slot) BAM_PURE;
    virtual EResult BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot) BAM_PURE;
    virtual EResult BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil) BAM_PURE;
public:
    virtual EResult BindShader(RHIShader* shader) BAM_PURE;
public:
    virtual EResult BindPipeline(RHIPipeline* pipeline) BAM_PURE;
public:
    virtual EResult BindSampler(RHISampler* sampler) BAM_PURE;
public:
    virtual EResult BindVertexBuffer(RHIBuffer* vertexBuffer); 
    virtual EResult BindIndexBuffer(RHIBuffer* indexBuffer);
    virtual EResult BindConstantBuffer(void* arg, uint32 slot) BAM_PURE;
    virtual EResult BindConstantBuffer(void* arg, uint32 size, uint32 slot) { return EResult::NotImplemented; }
    virtual EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) BAM_PURE;
public:
    virtual EResult BeginRenderPass(RenderPass* renderPass) { return EResult::NotImplemented; }
#pragma endregion

#pragma region Clear Resources
public:
	virtual EResult EndRenderPass() { return EResult::NotImplemented; }
    virtual EResult ClearRenderPass() { return EResult::NotImplemented; }
    virtual EResult ClearRenderTarget(RHITexture* renderTarget, vec4 color) BAM_PURE;
	virtual EResult ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil) BAM_PURE;
#pragma endregion

#pragma region Draw
public:
    virtual EResult Draw(uint32 count) BAM_PURE;
    virtual EResult DrawIndexed(uint32 count) BAM_PURE;
    virtual EResult DrawIndexedInstanced() BAM_PURE;
    virtual EResult DrawTexture(RHITexture* texture) BAM_PURE;
#pragma endregion

#pragma region Setter
    virtual EResult SetClearColor(vec4 color) BAM_PURE;
    virtual EResult SetViewport(int32 x, int32 y, uint32 width, uint32 height) BAM_PURE;
    void SetSwapChainWidth(uint32 width) { m_SwapChainWidth = width; }
    void SetSwapChainHeight(uint32 height) { m_SwapChainHeight = height; }
#pragma endregion

#pragma region Getter
    virtual void* GetNativeRHI() const BAM_PURE;
    virtual void* GetWindowHandle() const BAM_PURE;
    virtual void* GetCurrentCommandBuffer() const { return nullptr; }
	virtual RHITexture* GetBackBuffer() const { return m_BackBuffer; }
    uint32 GetSwapChainWidth() { return m_SwapChainWidth; }
    uint32 GetSwapChainHeight() { return m_SwapChainHeight; }
	virtual RenderPass* GetCurrentRenderPass() const { return m_CurrentRenderPass; }
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
	RHITexture* m_CurrentRenderTargets[MAX_RENDER_TARGET_COUNT] = {nullptr};
	uint32 m_CurrentRenderTargetCount = { 0 };
	RHITexture* m_CurrentDepthStencil = { nullptr };
	RHITexture* m_CurrentTextures[MAX_TEXTURE_SLOTS] = { nullptr };
protected:
	RHIPipeline* m_CurrentPipeline = { nullptr };
    RenderPass* m_CurrentRenderPass = { nullptr };
#pragma endregion
};

END