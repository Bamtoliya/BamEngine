#pragma once 
#include "RHI.h"
#include "DirectX12Types.h"
#include "DirectX12DescriptorAllocator.h"

struct DirectX12RHIDesc : public RHIDesc
{
	DirectX12RHIDesc() : RHIDesc() {}
};

BEGIN(Engine)
class DirectX12RHI : public RHI
{
private:
	using DESC = DirectX12RHIDesc;
#pragma region Constructor&Destructor
private:
	DirectX12RHI() = default;
	virtual ~DirectX12RHI() = default;
	EResult Initialize(void* arg) override;
public:
	static DirectX12RHI* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

public:
	virtual EResult Resize(uint32 width, uint32 height) override;
public:
	EResult BeginFrame() override;
	EResult EndFrame() override;
#pragma region Create Resources
	RHIBuffer* CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type) override;
	RHIBuffer* CreateVertexBuffer(void* data, uint32 size, uint32 stride) override;
	RHIBuffer* CreateIndexBuffer(void* data, uint32 size, uint32 stride) override;
	RHITexture* CreateTextureFromFile(const char* filename) override;
	RHITexture* CreateTextureFromFile(const wchar* filename) override;
	RHITexture* CreateTexture(const RHITextureDesc& desc) override;
	RHITexture* CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	RHITexture* CreateTextureCube(void* data, uint32 size, uint32 mipLevels) override;
	RHITexture* CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels) override;
	RHITexture* CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	RHITexture* CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize) override;
	RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) override;
	RHIPipeline* CreatePipeline(const RHIPipelineDesc& desc) override;
	RHISampler* CreateSampler(const SamplerDesc& desc) override;
	RHIShader* CreateShader(const RHIShaderDesc& desc) override;
#pragma endregion

#pragma region Bind Resources
	EResult BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil) override;
	EResult BindTexture(RHITexture* texture, uint32 slot) override;
	EResult BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot) override;
	EResult BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil) override;
	EResult BindShader(RHIShader* shader) override;
	EResult BindPipeline(RHIPipeline* pipeline) override;
	EResult BindSampler(RHISampler* sampler) override;
	EResult BindConstantBuffer(void* arg, uint32 slot) override;
	EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) override;
#pragma endregion

#pragma region RenderPass
	EResult BeginRenderPass(RenderPass* renderPass) override;
	EResult EndRenderPass() override;
	EResult ClearRenderPass() override;
#pragma endregion

#pragma region Clear Resources
	EResult ClearRenderTarget(RHITexture* renderTarget, vec4 color) override;
	EResult ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil) override;
#pragma endregion



#pragma region Draw Call
	EResult Draw(uint32 count) override;
	EResult DrawIndexed(uint32 count) override;
	EResult DrawIndexedInstanced() override;
	EResult DrawTexture(RHITexture* texture) override;
#pragma endregion
	EResult SetClearColor(vec4 color) override;
	EResult SetViewport(int32 x, int32 y, uint32 width, uint32 height) override;
	virtual void* GetNativeRHI() const override { return m_Device.Get(); }
	virtual void* GetWindowHandle() const override { return m_WindowHandle; }

public:
	ID3D12CommandQueue* GetCommandQueue() const { return m_CommandQueue.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }

public:
	DirectX12DescriptorAllocator* GetRTVAllocator() const { return m_RtvAllocator; }
	DirectX12DescriptorAllocator* GetDSVAllocator() const { return m_DsvAllocator; }
	DirectX12DescriptorAllocator* GetSRVAllocator() const { return m_SrvAllocator; }
	DirectX12DescriptorAllocator* GetSamplerAllocator() const { return m_SamplerAllocator; }
public:
	ID3D12RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature.Get(); }
private:

	//DirectX 12 Core
	ComPtr<IDXGIFactory4> m_DXGIFactory;
	ComPtr<ID3D12Device> m_Device;

	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	ComPtr<IDXGISwapChain3> m_SwapChain;

	ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
	ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
	uint32 m_RTVDescriptorSize = 0;
	uint32 m_DSVDescriptorSize = 0;

	ComPtr<ID3D12Fence> m_Fence;
	uint64 m_CurrentFenceValue = 0;
	HANDLE m_FenceEvent = nullptr;

	HWND m_WindowHandle = nullptr;
	vec4 m_ClearColor = { 0.3f, 0.3f, 0.3f, 1.0f };

	DirectX12DescriptorAllocator* m_RtvAllocator = nullptr; // 렌더타겟용 주차장
	DirectX12DescriptorAllocator* m_DsvAllocator = nullptr; // 뎁스용 주차장
	DirectX12DescriptorAllocator* m_SrvAllocator = nullptr; // 일반 텍스처(셰이더 리소스)용 주차장
	DirectX12DescriptorAllocator* m_SamplerAllocator = nullptr; // 샘플러용 주차장

	ComPtr<ID3D12RootSignature> m_GlobalRootSignature = nullptr;
};
END