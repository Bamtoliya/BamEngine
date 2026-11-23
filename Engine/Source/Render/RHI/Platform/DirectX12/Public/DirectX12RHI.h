#pragma once 
#include "RHI.h"
#include "DirectX12Types.h"
#include "DirectX12DescriptorAllocator.h"
#include "GpuHeapPageAllocator.h"
#include <vector>
#include <array>
#include <unordered_map>
#include <cstddef>

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
#pragma region Presentation
public:
	bool SupportsVSync(bool enabled) const override;
	EResult SetVSync(bool enabled) override;
#pragma endregion

#pragma region Create Resources
public:
	RHIBuffer* CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type) override;
	RHIBuffer* CreateVertexBuffer(void* data, uint32 size, uint32 stride) override;
	RHIBuffer* CreateIndexBuffer(void* data, uint32 size, uint32 stride) override;
	EResult UploadBufferData(RHIBuffer* buffer, const void* data, uint32 size, uint32 offset = 0);
public:
	RHITexture* CreateTextureFromFile(const char* filename) override;
	RHITexture* CreateTextureFromFile(const wchar* filename) override;
	RHITexture* CreateTexture(const RHITextureDesc& desc) override;
	RHITexture* CreateTextureFromNativeHandle(void* nativeHandle) override;
	EResult UploadTextureData(RHITexture* texture, const RHITextureDesc& desc, uint32 dstX = 0, uint32 dstY = 0) override;
public:
	RHIPipeline* CreatePipeline(const RHIPipelineDesc& desc) override;
	RHISampler* CreateSampler(const SamplerDesc& desc) override;
	RHIShader* CreateShader(const RHIShaderDesc& desc) override;
#pragma endregion

#pragma region Bind Resources
public:
	EResult BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil) override;
	EResult BindDepthSlice(RHITexture* depthStencil, uint32 sliceIndex) override;
	EResult BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot) override;
	EResult BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil) override;
	EResult BindShader(RHIShader* shader) override;
	EResult BindPipeline(RHIPipeline* pipeline) override;
	void InvalidatePipelineBinding(const RHIPipeline* pipeline) override
	{
		if (m_CurrentPipeline == pipeline)
			m_CurrentPipeline = nullptr;
	}
	EResult BindSampler(RHISampler* sampler) override;
	EResult BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size) override;
	EResult BindInstanceBuffer(RHIBuffer* buffer, uint32 offset = 0) override;
	EResult BindComputeStorageBuffer(RHIBuffer* buffer, uint32 slot = 0) override;
	EResult BindComputeStorageTexture(RHITexture* texture, uint32 slot) override;
	EResult BindDynamicVertexBuffer(uint32 slot, const void* data, uint32 vertexCount, uint32 stride) override;
	bool SupportsDynamicVertexBuffers() const override { return true; }
#pragma endregion

#pragma region RenderPass
public:
	EResult BeginRenderPass(RenderPass* renderPass) override;
	EResult EndRenderPass() override;
	EResult ClearRenderPass() override;
#pragma endregion

#pragma region Clear Resources
public:
	EResult ClearRenderTarget(RHITexture* renderTarget, vec4 color) override;
	EResult ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil) override;
	EResult ClearDepthSlice(RHITexture* depthStencil, uint32 sliceIndex, f32 depth, uint8 stencil) override;
#pragma endregion

#pragma region MyRegion
public:
	// heapAllocation이 있으면 객체를 놓은 뒤 그 페이지도 돌려준다.
	void DeferRelease(ID3D12Object* object, const GpuHeapAllocation& heapAllocation = {});
	// 정적 지오메트리 힙에 버퍼를 만든다. 큰 버퍼거나 만들 수 없으면 false이며 호출자가 독립 메모리를 쓴다.
	bool CreatePlacedGeometryBuffer(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState,
		ComPtr<ID3D12Resource>& outResource, GpuHeapAllocation& outAllocation);
	ENGINE_API void DeferDescriptorFree(DirectX12DescriptorAllocator* allocator, uint32 index);
	ENGINE_API EResult WaitForGPU();
private:
	EResult SignalFence(uint64& fenceValue);
	EResult WaitForFence(uint64 fenceValue);
	void CollectDeferredReleases();
#pragma endregion

#pragma region Pipeline Library
public:
	ID3D12PipelineLibrary* GetPipelineLibrary() const { return m_PipelineLibrary.Get(); }
	void MarkPipelineLibraryDirty() { m_PipelineLibraryDirty = true; }

	bool ContainsPipeline(const std::wstring& name) const
	{
		return m_PipelineNames.contains(name);
	}
	void RegisterPipelineName(const std::wstring& name)
	{
		m_PipelineNames.insert(name);
		m_PipelineLibraryDirty = true;
	}
private:
	void InitializePipelineLibrary();
	void SavePipelineLibrary();
#pragma endregion


#pragma region Draw
private:
	EResult ApplyShaderResources();
	D3D12_VERTEX_BUFFER_VIEW GetBoundVertexBufferView(uint32 slot) const;
public:
	EResult Draw(uint32 count) override;
	EResult DrawIndexed(uint32 count) override;
	virtual EResult DrawInstanced(uint32 vertexCountPerInstance, uint32 instanceCount, uint32 startVertexLocation = 0, uint32 startInstanceLocation = 0) override;
	virtual EResult DrawIndexedInstanced(uint32 indexCountPerInstance, uint32 instanceCount, uint32 startIndexLocation = 0, int32 baseVertexLocation = 0, uint32 startInstanceLocation = 0) override;
	EResult DrawTexture(RHITexture* texture) override;
	EResult ReadTexturePixel(RHITexture* texture, uint32 x, uint32 y, vec4& pixel) override;	
	EResult ReadTexturePixels(RHITexture* texture, void* dstBuffer, uint32 dstBufferSize) override;
	EResult BlitTexture(RHITexture* src, RHITexture* dst) override;
	EResult CopyTexture(RHITexture* src, RHITexture* dst) override;
#pragma endregion

#pragma region Compute
public:
	EResult Dispatch(uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ) override;
	EResult ResourceBarrierUAV(RHIBuffer* buffer) override;
	EResult ResourceBarrierUAV(RHITexture* texture) override;
	EResult TransitionStorageBufferToShaderRead(RHIBuffer* buffer) override;
#pragma endregion

#pragma region Raytracing
public:
	RHIAccelerationStructure* CreateBottomLevelAS(const RHIRaytracingGeometryDesc& desc) override;
	EResult UpdateBottomLevelAS(RHIAccelerationStructure* bottomLevel, const RHIRaytracingGeometryDesc& desc) override;
	EResult BuildTopLevelAS(RHIAccelerationStructure*& topLevel, const RHIRaytracingInstanceDesc* instances, uint32 instanceCount) override;
	EResult BindAccelerationStructure(RHIAccelerationStructure* topLevel, uint32 slot) override;
#pragma endregion

#pragma region Setter
public:
	EResult SetClearColor(vec4 color) override;
	EResult SetViewport(int32 x, int32 y, uint32 width, uint32 height) override;
	EResult SetScissorRect(uint32 left, uint32 top, uint32 right, uint32 bottom) override;
#pragma endregion

#pragma region Getter
public:
	virtual void* GetNativeRHI() const override { return m_Device.Get(); }
	virtual void* GetWindowHandle() const override { return m_WindowHandle; }
public:
	ID3D12CommandQueue* GetCommandQueue() const { return m_CommandQueue.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }
	// 런타임이 인터페이스를 제공하지 않으면 nullptr를 반환한다.
	// DXR 사용 가능 여부는 인터페이스 존재가 아니라 GetCapabilities()의 SupportsInlineRaytracing으로 판정한다.
	ID3D12Device5* GetDevice5() const { return m_Device5.Get(); }
	ID3D12GraphicsCommandList4* GetCommandList4() const { return m_CommandList4.Get(); }
	bool IsFrameRecording() const { return m_FrameRecording; }
	// 버퍼는 커맨드 리스트 제출이 끝나면 COMMON으로 감쇠한다. 추적 상태가 실제 상태와 같은 것은
	// 이번 프레임 커맨드 리스트에서 전환을 기록한 버퍼뿐이다(현재는 프레임 내 업로드).
	bool IsBufferStateRecordedThisFrame(const RHIBuffer* buffer) const { return m_BuffersTransitionedThisFrame.contains(buffer); }
	void MarkBufferStateRecordedThisFrame(const RHIBuffer* buffer) { m_BuffersTransitionedThisFrame.insert(buffer); }
public:
	DirectX12DescriptorAllocator* GetRTVAllocator() const { return m_RtvAllocator; }
	DirectX12DescriptorAllocator* GetDSVAllocator() const { return m_DsvAllocator; }
	DirectX12DescriptorAllocator* GetSRVAllocator() const { return m_SrvAllocator; }
	DirectX12DescriptorAllocator* GetSamplerAllocator() const { return m_SamplerAllocator; }
public:
	ID3D12RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature.Get(); }
	// 전역 루트 시그니처 구성이 바뀔 때마다 올린다. PSO 디스크 캐시 키에 섞여 이전 구성의 PSO를 불러오지 않게 한다.
	// 2: [11] Compute Storage Texture UAV Table(u2-u5, space3) 추가
	static constexpr uint32 GLOBAL_ROOT_SIGNATURE_LAYOUT_VERSION = 3;
#pragma endregion


#pragma region Bindless
public:
	virtual uint32 GetBindlessIndex(RHITexture* texture) override;
	virtual uint32 GetBindlessIndex(RHIBuffer* buffer) override;
	// 버퍼가 해제될 때 번호를 지연 반환한다.
	void ReleaseBindlessBuffer(uint32 descriptorIndex);
#pragma endregion

#pragma region Frame Metrics
public:
	virtual void BeginPassMetrics(const std::wstring& passName) override;
	virtual void EndPassMetrics() override;
	virtual void EndMetricsFrame(bool renderSucceeded) override;
	virtual RHIMemoryStats QueryMemoryStats() override;
private:
	void InitializeCapabilities();
	void InitializeTimestampQueries();
#pragma endregion

private:
	EResult CreateDeviceOnHighPerformanceAdapter();



	
private:
	ComPtr<ID3D12Resource> CreateTextureUploadBuffer(UINT64 size);

	// 프레임 안에서 올리는 버퍼 데이터는 업로드 힙을 새로 만들지 않고 구역을 나눠 쓴다.
	struct FrameUploadChunk
	{
		ComPtr<ID3D12Resource> Buffer;
		uint8* Memory = nullptr;
		UINT64 Used = 0;
	};
	static constexpr UINT64 FRAME_UPLOAD_CHUNK_SIZE = 8ull * 1024 * 1024;
	std::vector<FrameUploadChunk> m_FrameUploadArenas[MAX_SWAPCHAIN_BUFFERS];
	void ResetFrameUploadArena();
	bool AllocateFrameUpload(UINT64 size, ComPtr<ID3D12Resource>& outBuffer, UINT64& outOffset, uint8*& outMemory);

	std::vector<ComPtr<ID3D12Resource>> m_TextureUploadBuffers[MAX_SWAPCHAIN_BUFFERS];
	std::size_t m_TextureUploadCursors[MAX_SWAPCHAIN_BUFFERS] = {};
private:

	//DirectX 12 Core
	ComPtr<IDXGIFactory4> m_DXGIFactory;
	ComPtr<IDXGIAdapter1> m_Adapter; // 장치를 생성한 어댑터 (메모리 통계 조회에도 사용)
	ComPtr<ID3D12Device> m_Device;
	ComPtr<ID3D12Device5> m_Device5; // DXR 가속 구조 생성용, 미지원 시 nullptr

	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12CommandAllocator> m_CommandAllocators[MAX_SWAPCHAIN_BUFFERS];
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;
	ComPtr<ID3D12GraphicsCommandList4> m_CommandList4; // 같은 커맨드 리스트의 DXR 인터페이스, 미지원 시 nullptr

	ComPtr<IDXGISwapChain3> m_SwapChain;

	ComPtr<ID3D12DescriptorHeap> m_DynamicHeaps[MAX_SWAPCHAIN_BUFFERS];
	uint32 m_DynamicHeapCursor = 0;

	ComPtr<ID3D12Fence> m_Fence;
	uint64 m_CurrentFenceValue = 0;
	uint64 m_FrameFenceValues[MAX_SWAPCHAIN_BUFFERS] = {};
	bool m_FrameRecording = false;
	bool m_FrameFailed = false;
	HANDLE m_FenceEvent = nullptr;

	HWND m_WindowHandle = nullptr;
	vec4 m_ClearColor = { 0.3f, 0.3f, 0.3f, 1.0f };

	static constexpr uint32 SRV_TABLE_SIZE = MAX_TEXTURE_SLOTS;
	static constexpr uint32 UAV_TABLE_SIZE = MAX_COMPUTE_STORAGE_TEXTURE_SLOTS; // Root Param [11]: u2-u5, space3
	static constexpr uint32 COMPUTE_UAV_TABLE_ROOT_INDEX = 11;
	// 다음 Dispatch 한 번에만 적용되는 쓰기 텍스처. Dispatch와 프레임 시작 시 비운다(참조를 보유하지 않음).
	RHITexture* m_ComputeStorageTextures[UAV_TABLE_SIZE] = {};
	// 다음 Dispatch 한 번에만 적용되는 TLAS SRV(참조를 보유하지 않음). Dispatch와 프레임 시작 시 비운다.
	RHIAccelerationStructure* m_ComputeAccelerationStructure = nullptr;
	uint32 m_ComputeAccelerationStructureSlot = UINT32_MAX;
	// 이번 프레임 커맨드 리스트에서 상태 전환을 기록한 버퍼(주소 비교만 하며 역참조하지 않는다). BeginFrame에서 비운다.
	std::unordered_set<const RHIBuffer*> m_BuffersTransitionedThisFrame;
	static constexpr uint32 FRAME_SRV_DESCRIPTOR_COUNT = SRV_TABLE_SIZE * 1024;
	// 번호로 읽는 배열은 SRV 힙 전체를 가리킨다. 텍스처는 자기 SRV 번호를, 버퍼는 처음 요청할 때 만든 원시 SRV 번호를 쓴다.
	static constexpr uint32 BINDLESS_TEXTURE_ROOT_INDEX = 12;
	static constexpr uint32 BINDLESS_BUFFER_ROOT_INDEX = 13;
	static constexpr uint32 MAX_BINDLESS_BUFFERS = 16384;
	bool m_BindlessSupported = false;
	uint32 m_BindlessBufferCount = 0;
	uint32 m_SRVTableStartIndices[MAX_SWAPCHAIN_BUFFERS] = {};
	// 같은 프레임에서 리소스 조합이 같은 그래픽스 드로우는 SRV 테이블을 공유한다. 드로우마다 새 테이블을 쓰면
	// 캐스케이드처럼 드로우가 많은 프레임이 FRAME_SRV_DESCRIPTOR_COUNT를 넘는다.
	// 뷰 설정은 리소스·텍스처·조명 패스 여부로 정해진다. 해제된 리소스는 프레임이 끝날 때까지 지연되므로 주소가 겹치지 않는다.
	struct SRVTableKey
	{
		std::array<ID3D12Resource*, SRV_TABLE_SIZE> Resources{};
		std::array<const class DirectX12Texture*, SRV_TABLE_SIZE> Textures{};
		bool IsLighting = false;
		bool operator==(const SRVTableKey&) const = default;
	};
	struct SRVTableKeyHash
	{
		size_t operator()(const SRVTableKey& key) const
		{
			size_t seed = key.IsLighting ? 1u : 0u;
			const auto combine = [&seed](const void* value)
				{
					seed ^= std::hash<const void*>()(value) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
				};
			for (const auto* resource : key.Resources) combine(resource);
			for (const auto* texture : key.Textures) combine(texture);
			return seed;
		}
	};
	std::unordered_map<SRVTableKey, uint32, SRVTableKeyHash> m_FrameSRVTables; // 값은 힙 안의 테이블 시작 인덱스

	DirectX12DescriptorAllocator* m_RtvAllocator = nullptr; // 렌더타겟용 주차장
	DirectX12DescriptorAllocator* m_DsvAllocator = nullptr; // 뎁스용 주차장
	DirectX12DescriptorAllocator* m_SrvAllocator = nullptr; // 일반 텍스처(셰이더 리소스)용 주차장
	DirectX12DescriptorAllocator* m_SamplerAllocator = nullptr; // 샘플러용 주차장

	ComPtr<ID3D12RootSignature> m_GlobalRootSignature = nullptr;

	ComPtr<ID3D12PipelineLibrary> m_PipelineLibrary;
	std::vector<uint8_t> m_PipelineLibraryData;
	std::unordered_set<std::wstring> m_PipelineNames;
	bool m_PipelineLibraryDirty = false;

	// 배치된 버퍼보다 나중에 파괴되도록 지연 해제 목록보다 먼저 선언한다.
	static constexpr UINT64 GEOMETRY_HEAP_PAGE_SIZE = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // 64KB
	static constexpr UINT64 GEOMETRY_HEAP_SIZE = 32ull * 1024 * 1024;
	static constexpr UINT64 MAX_PLACED_GEOMETRY_SIZE = 8ull * 1024 * 1024;
	std::vector<ComPtr<ID3D12Heap>> m_GeometryHeaps;
	GpuHeapPageAllocator m_GeometryPages{ static_cast<uint32>(GEOMETRY_HEAP_SIZE / GEOMETRY_HEAP_PAGE_SIZE) };

	struct DeferredReleaseEntry
	{
		uint64 fenceValue = 0;
		ComPtr<ID3D12Object> object;
		GpuHeapAllocation heapAllocation;
		DirectX12DescriptorAllocator* descriptorAllocator = nullptr;
		uint32 descriptorIndex = UINT32_MAX;
	};
	std::vector<DeferredReleaseEntry> m_DeferredReleases;

#pragma region Frame Metrics
	static constexpr uint32 MAX_TIMESTAMP_QUERIES = 128;
	ComPtr<ID3D12QueryHeap> m_TimestampQueryHeap;
	ComPtr<ID3D12Resource> m_TimestampReadbackBuffers[MAX_SWAPCHAIN_BUFFERS];
	uint64 m_TimestampFrequency = 0;
	uint32 m_TimestampQueryCount = 0;
	uint32 m_ResolvedQueryCounts[MAX_SWAPCHAIN_BUFFERS] = {};
	// depth 0은 최상위 패스, 1 이상은 바깥 패스 시간에 이미 포함된 세부 구간이다.
	struct PassQueryRange { std::wstring passName; uint32 startIndex = 0; uint32 endIndex = 0; uint32 depth = 0; };
	std::vector<PassQueryRange> m_ActivePassQueries;
	// 열린 구간의 m_ActivePassQueries 인덱스. 쿼리를 배정받지 못한 구간은 UINT32_MAX다.
	std::vector<uint32> m_OpenPassQueries;
	uint32 m_OpenTimestampCount = 0;
	std::vector<PassQueryRange> m_CompletedPassQueries[MAX_SWAPCHAIN_BUFFERS];
	// 프레임 제한 뷰포트는 매 프레임 제출하지 않으므로 패스별 마지막 결과를 유지하고, 오래 갱신되지 않은 항목만 지운다.
	struct GPUTimestampResult { f32 TimeMs = 0.0f; uint64 ReadbackIndex = 0; };
	static constexpr uint64 TIMESTAMP_RESULT_RETENTION = 600;
	std::unordered_map<std::wstring, GPUTimestampResult> m_LastGPUTimestampResults;
	uint64 m_TimestampReadbackIndex = 0;
	f32 m_LastTotalGPUTimeMs = 0.0f;
#pragma endregion

};
END
