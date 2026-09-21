#pragma once
#include "Base.h"
#include "RHITypes.h"
#include "RenderTypes.h"
#include "ShaderReflection.h"

struct RHIDesc
{
    void* windowHandle = { nullptr };
    Engine::uint32 width = { 0 };
    Engine::uint32 height = { 0 };
    bool isVSync = { true };
};

struct RHIBufferDesc;
struct RHIShaderDesc;
struct RHITextureDesc;

struct RHIPipelineDesc;

enum class ERHIBufferType;

BEGIN(Engine)
struct SamplerDesc;
class RHIBuffer;
class RHIShader;
class RHISampler;
class RHITexture;
class RHIPipeline;

class RenderPass;

// CBV 캐싱 상태를 기록하는 장부 구조체
struct ConstantBufferBinding
{
    RHIBuffer* buffer = nullptr;         // 1번 방식: RHIBuffer 객체가 들어왔을 때
    uint32     offset = 0;        // 2번 방식: 링 버퍼 내 복사된 데이터의 오프셋
    bool       isDynamic = false;        // true면 링 버퍼(임시 데이터), false면 객체
};

class ENGINE_API RHI : public Base
{
protected:
	using DESC = RHIDesc;
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
    virtual RHITexture* CreateTexture(const RHITextureDesc& desc) BAM_PURE;
	virtual RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) BAM_PURE;
public:
    virtual RHIPipeline* CreatePipeline(const RHIPipelineDesc& desc) BAM_PURE;
public:
    virtual RHISampler* CreateSampler(const SamplerDesc& desc) BAM_PURE;
public:
	virtual RHIShader* CreateShader(const RHIShaderDesc& desc) BAM_PURE;
#pragma endregion

#pragma region Bind Resources
public:
    virtual EResult BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil) BAM_PURE;
	virtual EResult BindTexture(RHITexture* texture, uint32 slot);
    virtual EResult BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot) BAM_PURE;
    virtual EResult BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil) BAM_PURE;
public:
    virtual EResult BindShader(RHIShader* shader) BAM_PURE;
public:
    virtual EResult BindPipeline(RHIPipeline* pipeline) BAM_PURE;
public:
    virtual EResult BindSampler(RHISampler* sampler) BAM_PURE;
public:
    virtual EResult BindVertexBuffers(uint32 firstSlot, RHIBuffer** vertexBuffers, uint32 count);
    virtual EResult BindIndexBuffer(RHIBuffer* indexBuffer);
    virtual EResult BindConstantBuffer(RHIBuffer* buffer, uint32 slot);
    virtual EResult BindConstantBuffer(const void* data, uint32 size, uint32 slot);
    virtual EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size);
	virtual EResult BindStorageBuffer(RHIBuffer* buffer, uint32 slot);
#pragma endregion

#pragma region RenderPass
public:
    virtual EResult BeginRenderPass(RenderPass* renderPass) { return EResult::NotImplemented; }
    virtual EResult EndRenderPass() { return EResult::NotImplemented; }
    virtual EResult ClearRenderPass() { return EResult::NotImplemented; }
#pragma endregion

#pragma region Clear Resources
    virtual EResult ClearRenderTarget(RHITexture* renderTarget, vec4 color) BAM_PURE;
	virtual EResult ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil) BAM_PURE;
#pragma endregion

#pragma region Draw
public:
    virtual EResult Draw(uint32 count) BAM_PURE;
    virtual EResult DrawIndexed(uint32 count) BAM_PURE;
    virtual EResult DrawIndexedInstanced() BAM_PURE;
    virtual EResult DrawTexture(RHITexture* texture) BAM_PURE;
    virtual EResult BlitTexture(class RHITexture* src, class RHITexture* dst) { return EResult::NotImplemented; }
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

protected:
	EResult InitializeConstantBuffers();

#pragma region Member Variable
protected:
    RHIBuffer* m_VertexBuffers[MAX_BUFFER_SLOTS] = {nullptr};
    uint32 m_NumVertexBuffersBound = { 0 };
    RHIBuffer* m_IndexBuffer = { nullptr };
protected:
    ConstantBufferBinding m_ConstantBuffers[MAX_CONSTANT_BUFFER_SLOTS];
	RHITexture* m_CurrentTextures[MAX_TEXTURE_SLOTS] = { nullptr };
	RHIBuffer*  m_StorageBuffers[MAX_STORAGE_BUFFERS] = { nullptr };
protected:
    static constexpr uint32 DYNAMIC_CONSTANT_BUFFER_SIZE = 1024 * 1024 * 4; // 4MB
    RHIBuffer* m_DynamicConstantBuffers[MAX_SWAPCHAIN_BUFFERS] = { nullptr };
    uint32     m_DynamicBufferCursor = 0;
    uint32     m_ConstantBufferAlignment = 256;
protected:
	RHIShader* m_CurrentShader = { nullptr };
protected:
    uint32 m_SwapChainWidth = { 0 };
    uint32 m_SwapChainHeight = { 0 };

	RHITexture* m_SwapChainBuffers[MAX_SWAPCHAIN_BUFFERS] = { nullptr };
	uint32 m_SwapChainBufferCount = { 0 };
	uint32 m_CurrentBackBufferIndex = { 0 };

	RHITexture* m_BackBuffer = { nullptr };
protected:
	RHITexture* m_CurrentRenderTargets[MAX_RENDER_TARGET_COUNT] = {nullptr};
	uint32 m_CurrentRenderTargetCount = { 0 };
	RHITexture* m_CurrentDepthStencil = { nullptr };
protected:
	RHIPipeline* m_CurrentPipeline = { nullptr };
    RenderPass* m_CurrentRenderPass = { nullptr };
#pragma endregion
};

END