#pragma once
#include "DirectX12RHI.h"
#include "DirectX12Buffer.h"
#include "DirectX12Shader.h"
#include "DirectX12Sampler.h"
#include "DirectX12Texture.h"
#include "DirectX12Pipeline.h"
#include "DirectX12AccelerationStructure.h"
#include "RenderTargetManager.h"
#include "RenderPass.h"

#pragma region Constructor&Destructor
EResult DirectX12RHI::CreateDeviceOnHighPerformanceAdapter()
{
    // 내장 GPU와 외장 GPU가 함께 있는 환경에서 기본 어댑터(nullptr)가 내장 GPU로 잡히지 않도록
    // 고성능 순서로 어댑터를 열거해 장치 생성에 성공한 첫 하드웨어 어댑터를 사용한다.
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(m_DXGIFactory.As(&factory6)))
    {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 desc = {};
            if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device))))
            {
                m_Adapter = adapter;
                break;
            }
        }
    }

    // IDXGIFactory6을 지원하지 않는 런타임에서는 기존 동작(기본 어댑터)을 유지한다.
    if (!m_Device)
    {
        ENGINE_LOG_WARN("High-performance adapter selection is unavailable. Falling back to the default adapter.");
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device))))
            return EResult::Fail;
    }

    if (!m_Adapter)
        m_DXGIFactory->EnumAdapterByLuid(m_Device->GetAdapterLuid(), IID_PPV_ARGS(&m_Adapter));

    DXGI_ADAPTER_DESC1 desc = {};
    if (m_Adapter && SUCCEEDED(m_Adapter->GetDesc1(&desc)))
    {
        m_Capabilities.AdapterName = desc.Description;
        ENGINE_LOG_INFO("D3D12 device created on adapter: {} ({} MB dedicated VRAM)", WStrToStr(desc.Description), static_cast<uint64>(desc.DedicatedVideoMemory / (1024 * 1024)));
    }
    return EResult::Success;
}

EResult DirectX12RHI::Initialize(void* arg)
{
    if (!arg)
        return EResult::InvalidArgument;

    CAST_DESC;
	m_WindowHandle = static_cast<HWND>(desc->windowHandle);
	m_SwapChainWidth = desc->width;
	m_SwapChainHeight = desc->height;
    m_VSyncEnabled = desc->isVSync;
	m_SwapChainBufferCount = 2; // 더블 버퍼링

	uint32 dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	// DXGI Factory 생성
    if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_DXGIFactory))))
        return EResult::Fail;

    // 디바이스 생성 (D3D_FEATURE_LEVEL_11_0 이상, 고성능 어댑터 우선)
    if (IsFailure(CreateDeviceOnHighPerformanceAdapter()))
        return EResult::Fail;

#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_Device.As(&infoQueue)))
    {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnID(D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, FALSE);
    }
#endif

    m_RtvAllocator = new DirectX12DescriptorAllocator(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256);
    m_DsvAllocator = new DirectX12DescriptorAllocator(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256);

    const uint32 srvHeapCapacity = FRAME_SRV_DESCRIPTOR_COUNT * m_SwapChainBufferCount + 4096;
    m_SrvAllocator = new DirectX12DescriptorAllocator(
        m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvHeapCapacity);
    m_SamplerAllocator = new DirectX12DescriptorAllocator(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128);

    if (!m_RtvAllocator->GetHeap() || !m_DsvAllocator->GetHeap() ||
        !m_SrvAllocator->GetHeap() || !m_SamplerAllocator->GetHeap())
    {
        ENGINE_LOG_ERROR("Failed to create descriptor heaps.");
        return EResult::Fail;
    }

    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE regionStart = {};
        if (!m_SrvAllocator->Allocate(regionStart, m_SRVTableStartIndices[i], FRAME_SRV_DESCRIPTOR_COUNT))
        {
            ENGINE_LOG_ERROR("Failed to reserve frame SRV descriptors.");
            return EResult::OutOfMemory;
        }
    }

	// 커맨드 큐 생성
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue))))
        return EResult::Fail;

	// Command Allocator 생성
    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_CommandAllocators[i]))))
            return EResult::Fail;
    }

	// Command List 생성
    if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_CommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_CommandList))))
        return EResult::Fail;

    if (FAILED(m_CommandList->Close()))
        return EResult::Fail;

    // DXR 인터페이스는 선택 사항이다. 실패해도 기존 래스터 경로는 계속 사용한다.
    m_Device.As(&m_Device5);
    m_CommandList.As(&m_CommandList4);

	// Swap Chain 생성
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = m_SwapChainBufferCount;
	swapChainDesc.Width = m_SwapChainWidth;
	swapChainDesc.Height = m_SwapChainHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> swapChain1;

    if (FAILED(m_DXGIFactory->CreateSwapChainForHwnd(
        m_CommandQueue.Get(),
        m_WindowHandle, 
        &swapChainDesc,
        nullptr, 
        nullptr,
        &swapChain1
    )))
    {
        return EResult::Fail;
    }

	if (FAILED(swapChain1.As(&m_SwapChain)))
	{
		return EResult::Fail;
	}

	m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

	//// Descriptor Heap 생성 (Render Target View)
	//D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	//rtvHeapDesc.NumDescriptors = m_SwapChainBufferCount;
	//rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	//rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	//if (FAILED(m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap))))
	//{
	//	return EResult::Fail;
	//}
 //   m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 

    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        if (FAILED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer))))
            return EResult::Fail;

        DirectX12TextureDesc textureDesc = {};
        textureDesc.width = m_SwapChainWidth;
        textureDesc.height = m_SwapChainHeight;
        textureDesc.nativeHandle = backBuffer.Get();
        textureDesc.initialState = D3D12_RESOURCE_STATE_PRESENT;

        uint32 rtvIndex = 0;
        if (!m_RtvAllocator->Allocate(textureDesc.rtvHandle, rtvIndex))
        {
            ENGINE_LOG_ERROR("Failed to allocate back buffer RTV: {}", i);
            return EResult::OutOfMemory;
        }

        m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, textureDesc.rtvHandle);

        m_SwapChainBuffers[i] = DirectX12Texture::Create(this, textureDesc, false);
        if (!m_SwapChainBuffers[i])
        {
            ENGINE_LOG_ERROR("Failed to create back buffer wrapper: {}", i);
            return EResult::Fail;
        }
    }

    // 9. 동기화용 Fence 생성
    if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence))))
        return EResult::Fail;
    m_CurrentFenceValue = 0;
    m_FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
    if (m_FenceEvent == nullptr)
        return EResult::Fail;

    {
        // SRV/Sampler는 여전히 Descriptor Table로 유지 (여러 개를 묶어서 바인딩)
        CD3DX12_DESCRIPTOR_RANGE srvRanges[4];
        srvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 1);     // Material SRV: t0-t15, space1
        srvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 8, 0, 1);  // Material Sampler: s0-s7, space1
        srvRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 2);      // Object SRV: t0, space2
        srvRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 3);     // Pass SRV: t0-t15, space3

        CD3DX12_ROOT_PARAMETER rootParameters[12];

        // [0] Global CBV (b0, space0) — Root Descriptor (직접 GPU 주소 바인딩)
        rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        // [1] Material SRV Table (t0-t15, space1) — Descriptor Table
        rootParameters[1].InitAsDescriptorTable(1, &srvRanges[0], D3D12_SHADER_VISIBILITY_ALL);

        // [2] Material Sampler Table (s0-s7, space1) — Descriptor Table
        rootParameters[2].InitAsDescriptorTable(1, &srvRanges[1], D3D12_SHADER_VISIBILITY_ALL);

        // [3] Object CBV (b0, space2) — Root Descriptor
        rootParameters[3].InitAsConstantBufferView(0, 2, D3D12_SHADER_VISIBILITY_ALL);

        // [4] Object Instance SRV (t0, space2) — Root Descriptor (초고속 GPU 가상 주소 바인딩)
        rootParameters[4].InitAsShaderResourceView(0, 2, D3D12_SHADER_VISIBILITY_VERTEX);

        // [5] Pass CBV #0 (b0, space3) — Root Descriptor
        rootParameters[5].InitAsConstantBufferView(0, 3, D3D12_SHADER_VISIBILITY_ALL);

        // [6] Pass CBV #1 (b1, space3) — Root Descriptor (기존 테이블에서 분리!)
        rootParameters[6].InitAsConstantBufferView(1, 3, D3D12_SHADER_VISIBILITY_ALL);

        // [7] Pass SRV Table (t0-t15, space3) — Descriptor Table
        rootParameters[7].InitAsDescriptorTable(1, &srvRanges[3], D3D12_SHADER_VISIBILITY_ALL);

        // [8] Pass Sampler Table (s0-s7, space3) — Descriptor Table (기존 7번 → 8번으로 이동)
        // Note: 샘플러 range를 재사용하거나 별도 선언 필요
        CD3DX12_DESCRIPTOR_RANGE passSamplerRange;
        passSamplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 8, 0, 3);
        rootParameters[8].InitAsDescriptorTable(1, &passSamplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // [9] Pass UAV #0 (u0, space3) — Tile Light Grid 버퍼
        rootParameters[9].InitAsUnorderedAccessView(0, 3, D3D12_SHADER_VISIBILITY_ALL);
        // [10] Pass UAV #1 (u1, space3) — Tile Light Index List 버퍼
        rootParameters[10].InitAsUnorderedAccessView(1, 3, D3D12_SHADER_VISIBILITY_ALL);

        // [11] Compute Storage Texture UAV Table (u2-u5, space3) — RWTexture2D 쓰기 대상
        // 루트 UAV는 버퍼만 가리킬 수 있으므로 텍스처 UAV는 디스크립터 테이블로 묶는다.
        CD3DX12_DESCRIPTOR_RANGE computeUavRange;
        computeUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, UAV_TABLE_SIZE, 2, 3);
        rootParameters[COMPUTE_UAV_TABLE_ROOT_INDEX].InitAsDescriptorTable(1, &computeUavRange, D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
        rootSigDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            if (error) fmt::print(stderr, "루트 시그니처 오류: {}\n", (char*)error->GetBufferPointer());
            return EResult::Fail;
        }
        if (FAILED(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GlobalRootSignature))))
        {
            return EResult::Fail;
        }
    }

    if (IsFailure(InitializeConstantBuffers()) || IsFailure(InitializeInstanceBuffers()))
    {
        ENGINE_LOG_ERROR("Failed to initialize dynamic buffers.");
        return EResult::Fail;
    }

    InitializeCapabilities();
    InitializeTimestampQueries();
    InitializePipelineLibrary();
    return EResult::Success;
}

DirectX12RHI* DirectX12RHI::Create(void* arg)
{
	DirectX12RHI* instance = new DirectX12RHI();
    const EResult result = instance->Initialize(arg);
    if (result != EResult::Success)
    {
        fmt::print(stderr, "DirectX12RHI Creation Failed: {}\n", ResultToString(result));
        Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

void DirectX12RHI::Free()
{
    if (m_CommandQueue && m_Fence && IsFailure(WaitForGPU()))
    {
        ENGINE_LOG_ERROR("GPU synchronization failed during DirectX12RHI shutdown. Proceeding with resource cleanup.");
    }
    SavePipelineLibrary();
    m_PipelineLibrary.Reset();
    m_PipelineLibraryData.clear();
    m_PipelineLibraryData.shrink_to_fit();
    for (auto& buffers : m_TextureUploadBuffers)
        buffers.clear();

    for (auto& cursor : m_TextureUploadCursors)
        cursor = 0;

    // 백버퍼 배열 원소를 가리키는 비소유 포인터입니다.
    m_BackBuffer = nullptr;

    // 바인딩된 리소스를 allocator가 살아 있는 동안 해제합니다.
    RHI::Free();

    for (auto& backBuffer : m_SwapChainBuffers)
    {
        Safe_Release(backBuffer);
        backBuffer = nullptr;
    }

    if (!m_DeferredReleases.empty() && IsFailure(WaitForGPU()))
    {
        ENGINE_LOG_ERROR("Deferred resource synchronization failed during DirectX12RHI shutdown. Clearing deferred list.");
    }

    Safe_Delete(m_RtvAllocator);
    Safe_Delete(m_DsvAllocator);
    Safe_Delete(m_SrvAllocator);
    Safe_Delete(m_SamplerAllocator);

    if (m_FenceEvent)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
}
#pragma endregion

EResult DirectX12RHI::Resize(uint32 width, uint32 height)
{
    if (width == 0 || height == 0)
        return EResult::Success;

    if (!m_Device || !m_SwapChain || !m_CommandQueue || !m_Fence || !m_FenceEvent)
        return EResult::Fail;

    if (!m_CommandList || m_FrameFailed || m_FrameRecording || m_CurrentRenderPass)
        return EResult::Fail;

    if (m_SwapChainBufferCount == 0 || m_SwapChainBufferCount > MAX_SWAPCHAIN_BUFFERS)
        return EResult::Fail;

    if (width == m_SwapChainWidth && height == m_SwapChainHeight)
        return EResult::Success;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    if (FAILED(m_SwapChain->GetDesc1(&swapChainDesc)))
        return EResult::Fail;

    if (IsFailure(WaitForGPU()))
        return EResult::Fail;

    // 완료된 이전 프레임의 명령 기록도 비웁니다.
    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        auto& allocator = m_CommandAllocators[i];
        if (!allocator || FAILED(allocator->Reset()))
            return EResult::Fail;
    }

    if (FAILED(m_CommandList->Reset(m_CommandAllocators[0].Get(), nullptr)))
        return EResult::Fail;

    if (FAILED(m_CommandList->Close()))
        return EResult::Fail;

    // 기존 RTV 슬롯은 재사용합니다.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[MAX_SWAPCHAIN_BUFFERS] = {};
    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        if (!m_SwapChainBuffers[i])
            return EResult::Fail;

        auto* texture = static_cast<DirectX12Texture*>(m_SwapChainBuffers[i]);
        rtvHandles[i] = texture->GetRTVHandle();
    }

    for (uint32 slot = 0; slot < MAX_TEXTURE_SLOTS; ++slot)
        BindTexture(nullptr, slot);

    // m_BackBuffer는 배열 원소를 가리키는 참조용 포인터입니다.
    m_BackBuffer = nullptr;
    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
        Safe_Release(m_SwapChainBuffers[i]);

    const HRESULT hr = m_SwapChain->ResizeBuffers(
        m_SwapChainBufferCount, width, height, swapChainDesc.Format, swapChainDesc.Flags);

    if (FAILED(hr))
    {
        fmt::print(stderr, "ResizeBuffers failed: HRESULT=0x{:08X}\n", static_cast<uint32>(hr));
        return EResult::Fail;
    }

    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        if (FAILED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer))))
            return EResult::Fail;

        m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandles[i]);

        DirectX12TextureDesc textureDesc = {};
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.format = ETextureFormat::R8G8B8A8_UNORM;
        textureDesc.usage = ETextureUsage::RenderTarget;
        textureDesc.nativeHandle = backBuffer.Get();
        textureDesc.rtvHandle = rtvHandles[i];
        textureDesc.initialState = D3D12_RESOURCE_STATE_PRESENT;

        m_SwapChainBuffers[i] = DirectX12Texture::Create(this, textureDesc, false);
        if (!m_SwapChainBuffers[i])
            return EResult::Fail;
    }

    m_SwapChainWidth = width;
    m_SwapChainHeight = height;
    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
    m_BackBuffer = m_SwapChainBuffers[m_CurrentBackBufferIndex];

    return EResult::Success;
}

#pragma region Frame
EResult DirectX12RHI::BeginFrame()
{
    if (m_FrameFailed || m_FrameRecording || m_CurrentRenderPass)
        return EResult::Fail;
	m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
    if (m_CurrentBackBufferIndex >= m_SwapChainBufferCount || m_CurrentBackBufferIndex >= MAX_SWAPCHAIN_BUFFERS)
        return EResult::Fail;
    const EResult waitResult = WaitForFence(m_FrameFenceValues[m_CurrentBackBufferIndex]);
    if (IsFailure(waitResult))
    {
        m_FrameFailed = true;
        return waitResult;
    }

    // ── 완료된 프레임의 타임스탬프 회수 (GPU 스톨 없는 지연 회수) ──
    const uint32 frameIdx = m_CurrentBackBufferIndex;
    if (m_ResolvedQueryCounts[frameIdx] > 0 && m_TimestampFrequency > 0 && m_TimestampReadbackBuffers[frameIdx])
    {
        D3D12_RANGE readRange = { 0, m_ResolvedQueryCounts[frameIdx] * sizeof(uint64) };
        uint64* timestampData = nullptr;
        if (SUCCEEDED(m_TimestampReadbackBuffers[frameIdx]->Map(0, &readRange, reinterpret_cast<void**>(&timestampData))))
        {
            ++m_TimestampReadbackIndex;
            f32 totalGpuMs = 0.0f;
            for (const auto& passQuery : m_CompletedPassQueries[frameIdx])
            {
                if (passQuery.startIndex < m_ResolvedQueryCounts[frameIdx] && passQuery.endIndex < m_ResolvedQueryCounts[frameIdx])
                {
                    const uint64 start = timestampData[passQuery.startIndex];
                    const uint64 end = timestampData[passQuery.endIndex];
                    if (end >= start)
                    {
                        const f32 ms = static_cast<f32>(end - start) / static_cast<f32>(m_TimestampFrequency) * 1000.0f;
                        m_LastGPUTimestampResults[passQuery.passName] = { ms, m_TimestampReadbackIndex };
                        if (passQuery.depth == 0)
                            totalGpuMs += ms;
                    }
                }
            }
            m_LastTotalGPUTimeMs = totalGpuMs;
            std::erase_if(m_LastGPUTimestampResults, [this](const auto& result)
                {
                    return m_TimestampReadbackIndex - result.second.ReadbackIndex > TIMESTAMP_RESULT_RETENTION;
                });
            m_TimestampReadbackBuffers[frameIdx]->Unmap(0, nullptr);
        }
        m_ResolvedQueryCounts[frameIdx] = 0;
    }

    m_TimestampQueryCount = 0;
    m_ActivePassQueries.clear();
    m_OpenPassQueries.clear();
    m_OpenTimestampCount = 0;

    m_TextureUploadCursors[m_CurrentBackBufferIndex] = 0;
	m_BackBuffer = m_SwapChainBuffers[m_CurrentBackBufferIndex];
	m_DynamicHeapCursor = 0;
	m_FrameSRVTables.clear();
	std::fill(std::begin(m_ComputeStorageTextures), std::end(m_ComputeStorageTextures), nullptr);
	m_ComputeAccelerationStructure = nullptr;
	m_ComputeAccelerationStructureSlot = UINT32_MAX;
	m_BuffersTransitionedThisFrame.clear();

	// 다이내믹 CBV 링 버퍼 커서 초기화 (화이트보드 되감기)
	m_DynamicBufferCursor = 0;
    m_DynamicInstanceBufferCursor = 0;
    ResetDynamicVertexBindings();
	// 캐시 장부에서 동적 데이터(임시 구조체)만 초기화 (객체 바인딩은 유지)
	for (auto& cache : m_ConstantBuffers)
	{
		if (cache.isDynamic)
		{
			cache.buffer = nullptr;
			cache.offset = 0;
			cache.isDynamic = false;
		}
	}

    auto& allocator = m_CommandAllocators[m_CurrentBackBufferIndex];
    if (!allocator || FAILED(allocator->Reset()))
        return EResult::Fail;

    if (FAILED(m_CommandList->Reset(allocator.Get(), nullptr)))
        return EResult::Fail;

    m_CurrentPipeline = nullptr;
    m_CommandList->SetGraphicsRootSignature(m_GlobalRootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = { m_SrvAllocator->GetHeap(), m_SamplerAllocator->GetHeap() };

    m_CommandList->SetDescriptorHeaps(_countof(heaps), heaps);
    m_FrameRecording = true;

    return EResult::Success;
}

EResult DirectX12RHI::EndFrame()
{
    if (m_FrameFailed || !m_FrameRecording)
        return EResult::Fail;

    auto* dxBackBuffer = static_cast<DirectX12Texture*>(m_BackBuffer);
    if (dxBackBuffer && dxBackBuffer->GetCurrentState() == D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<ID3D12Resource*>(dxBackBuffer->GetNativeHandle());
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_CommandList->ResourceBarrier(1, &barrier);
        dxBackBuffer->SetCurrentState(D3D12_RESOURCE_STATE_PRESENT);
    }

    // ── 타임스탬프 쿼리 결과 리졸브 ──
    if (m_TimestampQueryCount > 0 && m_TimestampQueryHeap && m_TimestampReadbackBuffers[m_CurrentBackBufferIndex])
    {
        m_CommandList->ResolveQueryData(m_TimestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            0, m_TimestampQueryCount, m_TimestampReadbackBuffers[m_CurrentBackBufferIndex].Get(), 0);
        m_ResolvedQueryCounts[m_CurrentBackBufferIndex] = m_TimestampQueryCount;
        m_CompletedPassQueries[m_CurrentBackBufferIndex] = m_ActivePassQueries;
    }

    const HRESULT closeResult = m_CommandList->Close();
    if (FAILED(closeResult))
    {
        m_FrameFailed = true;
        ENGINE_LOG_ERROR("Command list Close failed: HRESULT=0x{:08X}", static_cast<uint32>(closeResult));
        return EResult::Fail;
    }

    ID3D12CommandList* commandLists[] = { m_CommandList.Get() };
    m_FrameRecording = false;
    m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    const UINT syncInterval = m_VSyncEnabled ? 1u : 0u;
    const HRESULT presentResult = m_SwapChain->Present(syncInterval, 0);
    if (FAILED(presentResult))
        ENGINE_LOG_ERROR("Present failed: HRESULT=0x{:08X}", static_cast<uint32>(presentResult));

    uint64 frameFenceValue = 0;
    const EResult signalResult = SignalFence(frameFenceValue);
    if (IsFailure(signalResult))
    {
        m_FrameFailed = true;
        return signalResult;
    }

    m_FrameFenceValues[m_CurrentBackBufferIndex] = frameFenceValue;

    if (FAILED(presentResult))
    {
        const EResult waitResult = WaitForFence(frameFenceValue);
        if (IsFailure(waitResult))
        {
            m_FrameFailed = true;
            return waitResult;
        }

        return EResult::Fail;
    }

    CollectDeferredReleases();
    return EResult::Success;
}
#pragma endregion

#pragma region Presentation
bool DirectX12RHI::SupportsVSync(bool) const
{
    return m_SwapChain.Get() != nullptr && !m_FrameFailed;
}

EResult DirectX12RHI::SetVSync(bool enabled)
{
    if (!m_SwapChain)
        return EResult::InvalidArgument;

    if (m_FrameFailed)
        return EResult::Fail;

    if (m_FrameRecording || m_CurrentRenderPass)
        return EResult::AccessDenied;

    m_VSyncEnabled = enabled;
    return EResult::Success;
}
#pragma endregion

#pragma region Create Resources
EResult DirectX12RHI::ReadTexturePixel(RHITexture* texture, uint32 x, uint32 y, vec4& pixel)
{
	auto* sourceTexture = dynamic_cast<DirectX12Texture*>(texture);
	if (!m_Device || !m_CommandQueue || m_FrameRecording || !sourceTexture || !texture->GetNativeHandle() ||
		texture->GetFormat() != ETextureFormat::R32G32B32A32_FLOAT ||
		texture->GetSampleCount() != ETextureSampleCount::TextureSampleCount1 ||
		x >= texture->GetWidth() || y >= texture->GetHeight()) return EResult::InvalidArgument;

	ComPtr<ID3D12Resource> readback;
	const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	if (FAILED(m_Device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return EResult::Fail;

	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> list;
	ComPtr<ID3D12Fence> fence;
	if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
		FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
			IID_PPV_ARGS(&list))) ||
		FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return EResult::Fail;

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = sourceTexture->GetCurrentState();
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	const bool transition = barrier.Transition.StateBefore != barrier.Transition.StateAfter;
	if (transition) list->ResourceBarrier(1, &barrier);

	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = barrier.Transition.pResource;
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = readback.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destination.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	destination.PlacedFootprint.Footprint.Width = 1;
	destination.PlacedFootprint.Footprint.Height = 1;
	destination.PlacedFootprint.Footprint.Depth = 1;
	destination.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
	const D3D12_BOX region{ x, y, 0, x + 1, y + 1, 1 };
	list->CopyTextureRegion(&destination, 0, 0, 0, &source, &region);
	if (transition)
	{
		std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		list->ResourceBarrier(1, &barrier);
	}
	if (FAILED(list->Close())) return EResult::Fail;
	ID3D12CommandList* lists[] = { list.Get() };
	m_CommandQueue->ExecuteCommandLists(1, lists);
	// Match the existing upload path: submitted resources must outlive the GPU copy.
    if (FAILED(m_CommandQueue->Signal(fence.Get(), 1)) || FAILED(fence->SetEventOnCompletion(1, nullptr)))
    {
        ENGINE_LOG_ERROR("DirectX12RHI::ReadTexturePixel failed to signal or wait on fence.");
        return EResult::Fail;
    }
	if (FAILED(m_Device->GetDeviceRemovedReason())) return EResult::Fail;
	void* data = nullptr;
	const D3D12_RANGE readRange{ 0, sizeof(float) * 4 };
	if (FAILED(readback->Map(0, &readRange, &data))) return EResult::Fail;
	std::memcpy(&pixel, data, sizeof(float) * 4);
	const D3D12_RANGE writeRange{ 0, 0 };
	readback->Unmap(0, &writeRange);
	return EResult::Success;
}

EResult DirectX12RHI::ReadTexturePixels(RHITexture* texture, void* dstBuffer, uint32 dstBufferSize)
{
    auto* sourceTexture = dynamic_cast<DirectX12Texture*>(texture);
    if (!m_Device || !m_CommandQueue || m_FrameRecording || !sourceTexture || !texture->GetNativeHandle() || !dstBuffer)
        return EResult::InvalidArgument;
    const uint32 width = texture->GetWidth();
    const uint32 height = texture->GetHeight();
    const uint32 bytesPerPixel = 4;
    const uint32 rowPitch = (width * bytesPerPixel + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const uint32 totalBufferBytes = rowPitch * height;
    if (dstBufferSize < width * height * bytesPerPixel)
        return EResult::InvalidArgument;
    ComPtr<ID3D12Resource> readback;
    const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBufferBytes);
    if (FAILED(m_Device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return EResult::Fail;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))) ||
        FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return EResult::Fail;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = sourceTexture->GetCurrentState();
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    const bool transition = barrier.Transition.StateBefore != barrier.Transition.StateAfter;
    if (transition) list->ResourceBarrier(1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = barrier.Transition.pResource;
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    DXGI_FORMAT copyFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (texture->GetFormat() == ETextureFormat::B8G8R8A8_UNORM || texture->GetFormat() == ETextureFormat::B8G8R8A8_UNORM_SRGB)
        copyFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint.Offset = 0;
    destination.PlacedFootprint.Footprint.Format = copyFormat;
    destination.PlacedFootprint.Footprint.Width = width;
    destination.PlacedFootprint.Footprint.Height = height;
    destination.PlacedFootprint.Footprint.Depth = 1;
    destination.PlacedFootprint.Footprint.RowPitch = rowPitch;
    const D3D12_BOX region{ 0, 0, 0, width, height, 1 };
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, &region);
    if (transition)
    {
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        list->ResourceBarrier(1, &barrier);
    }
    if (FAILED(list->Close())) return EResult::Fail;
    ID3D12CommandList* lists[] = { list.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);
    if (FAILED(m_CommandQueue->Signal(fence.Get(), 1)) || FAILED(fence->SetEventOnCompletion(1, nullptr)))
        return EResult::Fail;
    void* mapped = nullptr;
    const D3D12_RANGE readRange{ 0, totalBufferBytes };
    if (FAILED(readback->Map(0, &readRange, &mapped))) return EResult::Fail;
    auto* srcBytes = static_cast<const uint8_t*>(mapped);
    auto* dstBytes = static_cast<uint8_t*>(dstBuffer);
    const uint32 copyRowBytes = width * bytesPerPixel;
    for (uint32 y = 0; y < height; ++y)
    {
        std::memcpy(dstBytes + (y * copyRowBytes), srcBytes + (y * rowPitch), copyRowBytes);
    }
    const D3D12_RANGE writeRange{ 0, 0 };
    readback->Unmap(0, &writeRange);
    return EResult::Success;
}

EResult DirectX12RHI::BlitTexture(RHITexture* src, RHITexture* dst)
{
    if (!src || !dst || !m_Device || !m_CommandQueue)
        return EResult::InvalidArgument;

    auto* dxSrc = static_cast<DirectX12Texture*>(src);
    auto* dxDst = static_cast<DirectX12Texture*>(dst);
    auto* srcRes = static_cast<ID3D12Resource*>(dxSrc->GetNativeHandle());
    auto* dstRes = static_cast<ID3D12Resource*>(dxDst->GetNativeHandle());

    if (!srcRes || !dstRes)
        return EResult::Fail;

    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;

    if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))) ||
        FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
    {
        return EResult::Fail;
    }

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = srcRes;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = dxSrc->GetCurrentState();
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dstRes;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = dxDst->GetCurrentState();
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    list->ResourceBarrier(2, barriers);
    list->CopyResource(dstRes, srcRes);

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = dxSrc->GetCurrentState();

    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

    list->ResourceBarrier(2, barriers);
    dxDst->SetCurrentState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    if (FAILED(list->Close()))
        return EResult::Fail;

    ID3D12CommandList* lists[] = { list.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);

    if (FAILED(m_CommandQueue->Signal(fence.Get(), 1)) || FAILED(fence->SetEventOnCompletion(1, nullptr)))
        return EResult::Fail;

    return EResult::Success;
}

EResult DirectX12RHI::CopyTexture(RHITexture* src, RHITexture* dst)
{
    if (!m_CommandList || !src || !dst)
        return EResult::InvalidArgument;

    auto* dxSrc = static_cast<DirectX12Texture*>(src);
    auto* dxDst = static_cast<DirectX12Texture*>(dst);
    auto* srcRes = static_cast<ID3D12Resource*>(dxSrc->GetNativeHandle());
    auto* dstRes = static_cast<ID3D12Resource*>(dxDst->GetNativeHandle());

    if (!srcRes || !dstRes)
        return EResult::Fail;

    const D3D12_RESOURCE_STATES srcBefore = dxSrc->GetCurrentState();
    const D3D12_RESOURCE_STATES dstBefore = dxDst->GetCurrentState();

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    uint32 barrierCount = 0;

    if (srcBefore != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[barrierCount].Transition.pResource = srcRes;
        barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[barrierCount].Transition.StateBefore = srcBefore;
        barriers[barrierCount].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        ++barrierCount;
    }

    if (dstBefore != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[barrierCount].Transition.pResource = dstRes;
        barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[barrierCount].Transition.StateBefore = dstBefore;
        barriers[barrierCount].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        ++barrierCount;
    }

    if (barrierCount > 0)
        m_CommandList->ResourceBarrier(barrierCount, barriers);

    m_CommandList->CopyResource(dstRes, srcRes);

    barrierCount = 0;
    if (srcBefore != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[barrierCount].Transition.pResource = srcRes;
        barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[barrierCount].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[barrierCount].Transition.StateAfter = srcBefore;
        ++barrierCount;
    }

    barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[barrierCount].Transition.pResource = dstRes;
    barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[barrierCount].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[barrierCount].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    ++barrierCount;

    m_CommandList->ResourceBarrier(barrierCount, barriers);
    dxDst->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return EResult::Success;
}

#pragma region Buffer
RHIBuffer* DirectX12RHI::CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type)
{
    RHIBufferDesc desc = {};
    desc.bufferType = type;
    desc.size = size;
    desc.stride = stride;
    desc.initialData = data;
    return DirectX12Buffer::Create(this, desc);
}

RHIBuffer* DirectX12RHI::CreateVertexBuffer(void* data, uint32 size, uint32 stride)
{
    return CreateBuffer(data, size, stride, ERHIBufferType::Vertex);
}

RHIBuffer* DirectX12RHI::CreateIndexBuffer(void* data, uint32 size, uint32 stride)
{
    return CreateBuffer(data, size, stride, ERHIBufferType::Index);
}

EResult DirectX12RHI::UploadBufferData(RHIBuffer* buffer, const void* data, uint32 size, uint32 offset)
{
    if (!buffer || !data || size == 0 || !m_Device || !m_CommandQueue)
        return EResult::InvalidArgument;

    if (buffer->GetBufferType() == ERHIBufferType::Readback)
        return EResult::InvalidArgument;

    auto* dxBuffer = static_cast<DirectX12Buffer*>(buffer);
    auto* destination = static_cast<ID3D12Resource*>(dxBuffer->GetNativeHandle());
    if (!destination)
        return EResult::Fail;

    if (offset > dxBuffer->GetSize() || size > dxBuffer->GetSize() - offset)
        return EResult::InvalidArgument;

    if (void* mappedPtr = dxBuffer->GetMappedPointer())
    {
        std::memcpy(static_cast<uint8*>(mappedPtr) + offset, data, size);
        return EResult::Success;
    }

    if (m_FrameFailed)
        return EResult::Fail;

    const bool recordInFrame = m_FrameRecording && (m_CommandList != nullptr);

    ComPtr<ID3D12Resource> uploadBuffer;
    if (recordInFrame)
    {
        auto& buffers = m_TextureUploadBuffers[m_CurrentBackBufferIndex];
        auto& cursor = m_TextureUploadCursors[m_CurrentBackBufferIndex];

        if (cursor == buffers.size())
            buffers.emplace_back();

        auto& cachedBuffer = buffers[cursor];
        if (!cachedBuffer || cachedBuffer->GetDesc().Width < size)
        {
            auto newBuffer = CreateTextureUploadBuffer(size);
            if (!newBuffer)
                return EResult::Fail;
            cachedBuffer = newBuffer;
        }

        uploadBuffer = cachedBuffer;
        ++cursor;
    }
    else
    {
        uploadBuffer = CreateTextureUploadBuffer(size);
        if (!uploadBuffer)
            return EResult::Fail;
    }

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(uploadBuffer->Map(0, &readRange, &mapped)))
        return EResult::Fail;

    std::memcpy(mapped, data, size);
    uploadBuffer->Unmap(0, nullptr);

    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ID3D12GraphicsCommandList* copyList = nullptr;

    if (recordInFrame)
    {
        copyList = m_CommandList.Get();
    }
    else
    {
        if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))))
            return EResult::Fail;

        if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))))
            return EResult::Fail;

        if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
            return EResult::Fail;

        copyList = list.Get();
    }

    const D3D12_RESOURCE_STATES previousState = dxBuffer->GetCurrentState();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = destination;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    if (previousState != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        barrier.Transition.StateBefore = previousState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        copyList->ResourceBarrier(1, &barrier);
    }

    copyList->CopyBufferRegion(destination, offset, uploadBuffer.Get(), 0, size);

    D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_GENERIC_READ;
    if (dxBuffer->GetBufferType() == ERHIBufferType::Vertex)
        targetState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    else if (dxBuffer->GetBufferType() == ERHIBufferType::Index)
        targetState = D3D12_RESOURCE_STATE_INDEX_BUFFER;

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = targetState;
    copyList->ResourceBarrier(1, &barrier);

    if (recordInFrame)
    {
        dxBuffer->SetCurrentState(targetState);
        m_BuffersTransitionedThisFrame.insert(buffer);
        return EResult::Success;
    }

    if (FAILED(list->Close()))
        return EResult::Fail;

    ID3D12CommandList* lists[] = { list.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);

    if (FAILED(m_CommandQueue->Signal(fence.Get(), 1)))
    {
        ENGINE_LOG_ERROR("Buffer upload: CommandQueue signal failed.");
        return EResult::Fail;
    }
    if (FAILED(fence->SetEventOnCompletion(1, nullptr)))
    {
        ENGINE_LOG_ERROR("Buffer upload: Fence wait failed.");
        return EResult::Fail;
    }

    if (FAILED(m_Device->GetDeviceRemovedReason()))
        return EResult::Fail;

    dxBuffer->SetCurrentState(targetState);
    return EResult::Success;
}
#pragma endregion

#pragma region Texture
RHITexture* DirectX12RHI::CreateTextureFromFile(const char* filename)
{
    return nullptr;
}

RHITexture* DirectX12RHI::CreateTextureFromFile(const wchar* filename)
{
    return nullptr;
}

RHITexture* DirectX12RHI::CreateTexture(const RHITextureDesc& desc)
{
    DirectX12TextureDesc dxDesc{ desc };
    dxDesc.nativeHandle = nullptr; // Ensure native handle is null for new texture creation
    dxDesc.rtvHandle.ptr = 0; // Reset RTV handle
    dxDesc.initialState = D3D12_RESOURCE_STATE_COMMON; // Set initial state
    DirectX12Texture* texture = DirectX12Texture::Create(this, dxDesc);
    if (!texture)
        return nullptr;

    if (desc.data)
    {
        if (IsFailure(UploadTextureData(texture, desc)))
        {
            Safe_Release(texture);
            return nullptr;
        }
    }

    return texture;
}

RHITexture* DirectX12RHI::CreateTextureFromNativeHandle(void* nativeHandle)
{
    return nullptr;
}

ComPtr<ID3D12Resource> DirectX12RHI::CreateTextureUploadBuffer(UINT64 size)
{
    if (!m_Device || size == 0)
        return {};

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = size;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> buffer;

    const HRESULT result = m_Device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

    if (FAILED(result))
        return {};

    return buffer;
}

EResult DirectX12RHI::UploadTextureData(RHITexture* texture, const RHITextureDesc& desc, uint32 dstX, uint32 dstY)
{
    if (!texture || !desc.data || !m_Device || !m_CommandQueue)
        return EResult::InvalidArgument;

    const bool recordInFrame = m_FrameRecording;

    if (m_FrameFailed ||
        (recordInFrame && (!m_CommandList || m_CurrentRenderPass)))
        return EResult::Fail;

    const bool is3D = (desc.dimension == ETextureDimension::Texture3D);
    const uint32 depth = is3D ? desc.depth : 1;

    const bool isBlockCompressed = IsBlockCompressed(desc.format);
    const bool isSupportedFormat = (desc.format == ETextureFormat::R8G8B8A8_UNORM ||
                                    desc.format == ETextureFormat::R8G8B8A8_UNORM_SRGB ||
                                    desc.format == ETextureFormat::R16G16_FLOAT ||
                                    desc.format == ETextureFormat::R32_FLOAT ||
                                    isBlockCompressed);
    if (!isSupportedFormat ||
        (desc.dimension != ETextureDimension::Texture2D && !is3D) ||
        desc.mipLevels == 0 ||
        desc.arraySize != 1 ||
        depth == 0 ||
        desc.sampleCount != ETextureSampleCount::TextureSampleCount1 ||
        desc.width == 0 ||
        desc.height == 0)
    {
        return EResult::InvalidArgument;
    }

    if (is3D && (desc.mipLevels != 1 || isBlockCompressed))
    {
        return EResult::InvalidArgument;
    }

    // 부분 업로드는 단일 밉(mipLevels == 1)이며 비압축 포맷일 때만 지원
    if ((dstX != 0 || dstY != 0) && (desc.mipLevels > 1 || isBlockCompressed))
    {
        return EResult::InvalidArgument;
    }

    auto* dxTexture = static_cast<DirectX12Texture*>(texture);
    auto* destination = static_cast<ID3D12Resource*>(dxTexture->GetNativeHandle());

    if (!destination)
        return EResult::Fail;

    // 1. 목적지 범위를 검사하고 전송 영역의 업로드 배치를 계산합니다.
    const D3D12_RESOURCE_DESC resourceDesc = destination->GetDesc();
    const auto expectedDimension = is3D
        ? D3D12_RESOURCE_DIMENSION_TEXTURE3D
        : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    if (resourceDesc.Dimension != expectedDimension ||
        dxTexture->GetFormat() != desc.format ||
        resourceDesc.MipLevels != desc.mipLevels ||
        resourceDesc.SampleDesc.Count != 1 ||
        (!is3D && resourceDesc.DepthOrArraySize != 1) ||
        UINT64(dstX) + desc.width > resourceDesc.Width ||
        UINT64(dstY) + desc.height > resourceDesc.Height)
    {
        return EResult::InvalidArgument;
    }

    if (desc.mipLevels > 1 && (desc.width != resourceDesc.Width || desc.height != resourceDesc.Height))
    {
        return EResult::InvalidArgument;
    }

    // 이번 부분 업로드는 2D만 지원합니다. 기존 3D 전체 업로드는 유지합니다.
    if (is3D && (dstX != 0 || dstY != 0 ||
        desc.width != resourceDesc.Width ||
        desc.height != resourceDesc.Height ||
        depth != resourceDesc.DepthOrArraySize))
    {
        return EResult::InvalidArgument;
    }

    const UINT bytesPerPixel = isBlockCompressed ? 0 : GetUncompressedBytesPerPixel(desc.format);
    const UINT numSubresources = desc.mipLevels;

    D3D12_RESOURCE_DESC copyDesc = resourceDesc;
    copyDesc.Alignment = 0;
    copyDesc.Width = desc.width;
    copyDesc.Height = desc.height;
    copyDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(numSubresources);
    std::vector<UINT> numRows(numSubresources);
    std::vector<UINT64> rowSizesInBytes(numSubresources);
    UINT64 uploadBytes = 0;

    m_Device->GetCopyableFootprints(
        &copyDesc, 0, numSubresources, 0,
        footprints.data(), numRows.data(), rowSizesInBytes.data(), &uploadBytes);

    if (uploadBytes == 0 || uploadBytes == UINT64_MAX)
    {
        return EResult::Fail;
    }

    if (desc.mipLevels == 1)
    {
        UINT64 sourceRowBytes = 0;
        UINT expectedRows = 0;
        if (isBlockCompressed)
        {
            const UINT blockW = (desc.width + 3) / 4;
            const UINT blockH = (desc.height + 3) / 4;
            const UINT blockSize = GetBlockCompressedBlockSize(desc.format);
            sourceRowBytes = UINT64(blockW) * blockSize;
            expectedRows = blockH;
        }
        else
        {
            sourceRowBytes = UINT64(desc.width) * bytesPerPixel;
            expectedRows = desc.height;
        }

        if (numRows[0] != expectedRows ||
            rowSizesInBytes[0] != sourceRowBytes ||
            footprints[0].Footprint.Depth != depth)
        {
            return EResult::Fail;
        }
    }

    // 2. 프레임별 업로드 버퍼를 재사용합니다.
    ComPtr<ID3D12Resource> uploadBuffer;

    if (recordInFrame)
    {
        auto& buffers = m_TextureUploadBuffers[m_CurrentBackBufferIndex];
        auto& cursor = m_TextureUploadCursors[m_CurrentBackBufferIndex];

        if (cursor == buffers.size())
            buffers.emplace_back();

        auto& cachedBuffer = buffers[cursor];

        if (!cachedBuffer || cachedBuffer->GetDesc().Width < uploadBytes)
        {
            auto newBuffer = CreateTextureUploadBuffer(uploadBytes);
            if (!newBuffer)
                return EResult::Fail;

            cachedBuffer = newBuffer;
        }

        uploadBuffer = cachedBuffer;
        ++cursor;
    }
    else
    {
        uploadBuffer = CreateTextureUploadBuffer(uploadBytes);
        if (!uploadBuffer)
            return EResult::Fail;
    }

    // 3. CPU 픽셀 → 업로드 버퍼 (모든 밉 레벨 및 3D 슬라이스 순회)
    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };

    if (FAILED(uploadBuffer->Map(0, &readRange, &mapped)))
        return EResult::Fail;

    auto* mappedBase = static_cast<std::uint8_t*>(mapped);
    const auto* src = static_cast<const std::uint8_t*>(desc.data);
    UINT64 srcOffset = 0;

    for (UINT mip = 0; mip < desc.mipLevels; ++mip)
    {
        const UINT curW = (std::max)(1u, desc.width >> mip);
        const UINT curH = (std::max)(1u, desc.height >> mip);
        const UINT curDepth = is3D ? (std::max)(1u, depth >> mip) : 1;
        const auto& fp = footprints[mip];

        UINT64 sourceRowBytes = 0;
        UINT64 sourceRowCount = 0;
        if (isBlockCompressed)
        {
            const UINT blockW = (curW + 3) / 4;
            const UINT blockH = (curH + 3) / 4;
            const UINT blockSize = GetBlockCompressedBlockSize(desc.format);
            sourceRowBytes = UINT64(blockW) * blockSize;
            sourceRowCount = blockH;
        }
        else
        {
            sourceRowBytes = UINT64(curW) * bytesPerPixel;
            sourceRowCount = curH;
        }

        auto* dstSub = mappedBase + fp.Offset;
        const SIZE_T slicePitchDst = SIZE_T(fp.Footprint.RowPitch) * fp.Footprint.Height;
        const SIZE_T slicePitchSrc = SIZE_T(sourceRowBytes) * sourceRowCount;

        for (UINT z = 0; z < curDepth; ++z)
        {
            auto* dstSlice = dstSub + z * slicePitchDst;
            const auto* srcSlice = src + srcOffset + z * slicePitchSrc;

            for (UINT y = 0; y < numRows[mip]; ++y)
            {
                std::memcpy(
                    dstSlice + SIZE_T(y) * fp.Footprint.RowPitch,
                    srcSlice + SIZE_T(y) * sourceRowBytes,
                    static_cast<SIZE_T>(sourceRowBytes));
            }
        }

        srcOffset += slicePitchSrc * curDepth;
    }

    uploadBuffer->Unmap(0, nullptr);

    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ID3D12GraphicsCommandList* copyList = nullptr;

    if (recordInFrame)
    {
        copyList = m_CommandList.Get();
    }
    else
    {
        if (FAILED(m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))))
            return EResult::Fail;

        if (FAILED(m_Device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))))
            return EResult::Fail;

        if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
            return EResult::Fail;

        copyList = list.Get();
    }

    // 5. 텍스처를 복사 대상 상태로 전환
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = destination;
    barrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    const auto previousState = dxTexture->GetCurrentState();

    if (previousState != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        barrier.Transition.StateBefore = previousState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        copyList->ResourceBarrier(1, &barrier);
    }

    // 6. 업로드 버퍼 → 텍스처 (모든 서브리소스 복사)
    for (UINT mip = 0; mip < desc.mipLevels; ++mip)
    {
        D3D12_TEXTURE_COPY_LOCATION sourceLoc = {};
        sourceLoc.pResource = uploadBuffer.Get();
        sourceLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        sourceLoc.PlacedFootprint = footprints[mip];

        D3D12_TEXTURE_COPY_LOCATION targetLoc = {};
        targetLoc.pResource = destination;
        targetLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        targetLoc.SubresourceIndex = mip;

        copyList->CopyTextureRegion(&targetLoc, (mip == 0 ? dstX : 0), (mip == 0 ? dstY : 0), 0, &sourceLoc, nullptr);
    }

    // 7. 셰이더(VS/PS)에서 읽을 수 있도록 전환
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

    copyList->ResourceBarrier(1, &barrier);

    if (recordInFrame)
    {
        // 프레임별 목록이 업로드 버퍼를 소유합니다.
        dxTexture->SetCurrentState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        return EResult::Success;
    }

    if (FAILED(list->Close()))
        return EResult::Fail;

    // 8. 실행 및 완료 대기
    ID3D12CommandList* lists[] = { list.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);

    if (FAILED(m_CommandQueue->Signal(fence.Get(), 1)))
    {
        ENGINE_LOG_ERROR("Texture upload: CommandQueue signal failed.");
        return EResult::Fail;
    }
    if (FAILED(fence->SetEventOnCompletion(1, nullptr)))
    {
        ENGINE_LOG_ERROR("Texture upload: Fence wait failed.");
        return EResult::Fail;
    }

    if (FAILED(m_Device->GetDeviceRemovedReason()))
        return EResult::Fail;

    dxTexture->SetCurrentState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    return EResult::Success;
}
#pragma endregion

#pragma region Pipeline
RHIPipeline* DirectX12RHI::CreatePipeline(const RHIPipelineDesc& desc)
{
    return DirectX12Pipeline::Create(this, desc);
}
#pragma endregion

#pragma region Sampler
RHISampler* DirectX12RHI::CreateSampler(const SamplerDesc& desc)
{
    return DirectX12Sampler::Create(this, desc);
}
#pragma endregion

#pragma region Shader
RHIShader* DirectX12RHI::CreateShader(const RHIShaderDesc& desc)
{
    return DirectX12Shader::Create(this, desc);
}
#pragma endregion
#pragma endregion

#pragma region Bind Resources
EResult DirectX12RHI::BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil)
{
    if (!renderTarget)
    {
        return BindRenderTargets(0, &renderTarget, depthStencil);
    }
    return BindRenderTargets(1, &renderTarget, depthStencil);
}

EResult DirectX12RHI::BindDepthSlice(RHITexture* depthStencil, uint32 sliceIndex)
{
    if (!m_CommandList || !depthStencil) return EResult::Fail;
    DirectX12Texture* dxDepthStencil = static_cast<DirectX12Texture*>(depthStencil);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxDepthStencil->GetDSVHandle(sliceIndex);
    m_CommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
    return EResult::Success;
}

EResult DirectX12RHI::BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot)
{
    if (!m_CommandList || !texture || !sampler || slot >= MAX_TEXTURE_SLOTS)
        return EResult::InvalidArgument;

    if (IsFailure(BindTexture(texture, slot)))
        return EResult::Fail;

    auto* dxSampler = static_cast<DirectX12Sampler*>(sampler);
    const D3D12_GPU_DESCRIPTOR_HANDLE handle = dxSampler->GetGPUHandle();
    if (handle.ptr == 0) return EResult::Fail;

    const uint32 rootParameter = slot < MAX_MATERIAL_TEXTURE_SLOTS ? 2 : 8;
    m_CommandList->SetGraphicsRootDescriptorTable(rootParameter, handle);
    return EResult::Success;
}

EResult DirectX12RHI::BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil)
{
    if (!m_CommandList) return EResult::Fail;
    if (count > MAX_RENDER_TARGET_COUNT) return EResult::InvalidArgument;
    //Clear RenderTarget
    if (count == 0 || renderTargets == nullptr)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
        D3D12_CPU_DESCRIPTOR_HANDLE* pDsvHandle = nullptr;
        if (depthStencil)
        {
            DirectX12Texture* dxDepthStencil = static_cast<DirectX12Texture*>(depthStencil);
            pDsvHandle = &dsvHandle;
            dsvHandle = dxDepthStencil->GetDSVHandle();
        }
        m_CommandList->OMSetRenderTargets(0, nullptr, FALSE, pDsvHandle);
        return EResult::Success;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[MAX_RENDER_TARGET_COUNT] = {};
    for (uint32 i = 0; i < count; ++i)
    {
        DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(renderTargets[i]);
        rtvHandles[i] = dxTex->GetRTVHandle();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    if (depthStencil)
    {
        DirectX12Texture* dxDepthStencil = static_cast<DirectX12Texture*>(depthStencil);
        dsvHandle = dxDepthStencil->GetDSVHandle();
    }
    m_CommandList->OMSetRenderTargets(
        count,
        rtvHandles,
        FALSE,
        depthStencil ? &dsvHandle : nullptr);
    return EResult::Success;
}

EResult DirectX12RHI::BindShader(RHIShader* shader)
{
    return EResult::Success;
}
EResult DirectX12RHI::BindPipeline(RHIPipeline* pipeline)
{
    if (!m_CommandList || !pipeline)
        return EResult::Fail;

    const bool typeChanged = !m_CurrentPipeline || (m_CurrentPipeline->GetPipelineType() != pipeline->GetPipelineType());
    if (m_CurrentPipeline == pipeline && !typeChanged)
        return EResult::Success;

    ID3D12PipelineState* pipelineState = static_cast<ID3D12PipelineState*>(pipeline->GetNativeHandle());
    if (!pipelineState)
        return EResult::Fail;

    m_CommandList->SetPipelineState(pipelineState);

    if (pipeline->GetPipelineType() == EPipelineType::Compute)
    {
        m_CommandList->SetComputeRootSignature(m_GlobalRootSignature.Get());
    }
    else
    {
        m_CommandList->SetGraphicsRootSignature(m_GlobalRootSignature.Get());
        D3D_PRIMITIVE_TOPOLOGY dxTopo = ToD3D12PrimitiveTopology(pipeline->GetTopology());
        m_CommandList->IASetPrimitiveTopology(dxTopo);
    }

    m_CurrentPipeline = pipeline;
    return EResult::Success;
}
EResult DirectX12RHI::BindSampler(RHISampler* sampler)
{
    if (!m_CommandList || !sampler)
        return EResult::Fail;
    DirectX12Sampler* dxSampler = static_cast<DirectX12Sampler*>(sampler);
    m_CommandList->SetGraphicsRootDescriptorTable(8, dxSampler->GetGPUHandle());
    return EResult::Success;
}
EResult DirectX12RHI::BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size)
{
    return EResult();
}
EResult DirectX12RHI::BindInstanceBuffer(RHIBuffer* buffer, uint32 offset)
{
    if (!m_CommandList || !buffer) return EResult::InvalidArgument;

    auto* resource = static_cast<ID3D12Resource*>(buffer->GetNativeHandle());
    if (!resource) return EResult::Fail;

    const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = resource->GetGPUVirtualAddress() + offset;
    m_CommandList->SetGraphicsRootShaderResourceView(4, gpuAddress);
    return EResult::Success;
}
EResult DirectX12RHI::BindDynamicVertexBuffer(uint32 slot, const void* data, uint32 vertexCount, uint32 stride)
{
    if (slot >= MAX_BUFFER_SLOTS || !data || vertexCount == 0 || stride == 0)
        return EResult::InvalidArgument;

    RHIBuffer* ringBuffer = nullptr;
    uint32 offset = 0;
    const uint32 size = vertexCount * stride;
    if (const EResult result = CopyToDynamicRingBuffer(data, size, ringBuffer, offset); IsFailure(result))
        return result;

    Safe_Release(m_VertexBuffers[slot]);
    m_VertexBuffers[slot] = ringBuffer;
    Safe_AddRef(m_VertexBuffers[slot]);
    m_DynamicVertexBindings[slot] = { offset, size, stride, true };
    m_NumVertexBuffersBound = std::max(m_NumVertexBuffersBound, slot + 1);
    return EResult::Success;
}

D3D12_VERTEX_BUFFER_VIEW DirectX12RHI::GetBoundVertexBufferView(uint32 slot) const
{
    auto* buffer = static_cast<DirectX12Buffer*>(m_VertexBuffers[slot]);
    if (!buffer) return {};

    const DynamicVertexBinding& binding = m_DynamicVertexBindings[slot];
    if (!binding.isActive) return buffer->GetVertexBufferView();

    // 링 버퍼는 업로드 힙(GENERIC_READ)이므로 상태 전환 없이 정점 버퍼로 읽을 수 있다.
    auto* resource = static_cast<ID3D12Resource*>(buffer->GetNativeHandle());
    if (!resource) return {};
    return { resource->GetGPUVirtualAddress() + binding.offset, binding.size, binding.stride };
}
EResult DirectX12RHI::BindComputeStorageBuffer(RHIBuffer* buffer, uint32 slot)
{
    if (!m_CommandList || !m_FrameRecording || !buffer)
        return EResult::Fail;
    auto* resource = static_cast<ID3D12Resource*>(buffer->GetNativeHandle());
    if (!resource)
        return EResult::Fail;
    const uint32 rootParamIndex = 9 + slot;
    if (rootParamIndex > 10)
        return EResult::InvalidArgument;
    m_CommandList->SetComputeRootUnorderedAccessView(rootParamIndex, resource->GetGPUVirtualAddress());
    return EResult::Success;
}
EResult DirectX12RHI::BindComputeStorageTexture(RHITexture* texture, uint32 slot)
{
    if (!m_CommandList || !m_FrameRecording)
        return EResult::Fail;
    if (slot >= UAV_TABLE_SIZE)
        return EResult::InvalidArgument;
    if (texture && !HasFlag(texture->GetUsage(), ETextureUsage::ComputeReadWrite))
    {
        ENGINE_LOG_ERROR("Compute storage texture requires ETextureUsage::ComputeReadWrite.");
        return EResult::InvalidArgument;
    }
    m_ComputeStorageTextures[slot] = texture;
    return EResult::Success;
}
#pragma endregion

#pragma region RenderPass
EResult  DirectX12RHI::BeginRenderPass(RenderPass* renderPass)
{
    if (!m_CommandList || !renderPass) return EResult::Fail;
    for (uint32 slot = 0; slot < MAX_TEXTURE_SLOTS; ++slot)
    {
        if (IsFailure(BindTexture(nullptr, slot)))
            return EResult::Fail;
    }
    RHITexture* renderTargets[MAX_RENDER_TARGET_COUNT] = {};
    uint32 renderTargetCount = 0;
    RHITexture* depthTarget = nullptr;
    const uint32 requestedRenderTargetCount = renderPass->GetRenderTargetCount();
    if (requestedRenderTargetCount > MAX_RENDER_TARGET_COUNT)
        return EResult::InvalidArgument;
    if (requestedRenderTargetCount == 0 && renderPass->GetDepthStencilName().empty())
    {
        if (m_BackBuffer) renderTargets[renderTargetCount++] = m_BackBuffer;
    }
    else
    {
        for (uint32 i = 0; i < requestedRenderTargetCount; ++i)
        {
            RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i));
            if (rt && rt->GetTexture()) renderTargets[renderTargetCount++] = rt->GetTexture();
        }
    }
    if (!renderPass->GetDepthStencilName().empty())
    {
        RenderTarget* ds = RenderTargetManager::Get().GetRenderTarget(renderPass->GetDepthStencilName());
        if (ds && ds->GetTexture()) depthTarget = ds->GetTexture();
    }
    D3D12_RESOURCE_BARRIER barriers[MAX_RENDER_TARGET_COUNT + 1] = {};
    uint32 barrierCount = 0;
    for (uint32 i = 0; i < renderTargetCount; ++i)
    {
        DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(renderTargets[i]);
        if (dxTex->GetCurrentState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            D3D12_RESOURCE_BARRIER& barrier = barriers[barrierCount++];
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = static_cast<ID3D12Resource*>(dxTex->GetNativeHandle());
            barrier.Transition.StateBefore = dxTex->GetCurrentState();
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            dxTex->SetCurrentState(D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }
    if (depthTarget)
    {
        DirectX12Texture* dxDepth = static_cast<DirectX12Texture*>(depthTarget);
        if (dxDepth->GetCurrentState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        {
            D3D12_RESOURCE_BARRIER& barrier = barriers[barrierCount++];
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = static_cast<ID3D12Resource*>(dxDepth->GetNativeHandle());
            barrier.Transition.StateBefore = dxDepth->GetCurrentState();
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            dxDepth->SetCurrentState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }
    if (barrierCount > 0)
    {
        m_CommandList->ResourceBarrier(barrierCount, barriers);
    }
    const EResult bindResult = BindRenderTargets(renderTargetCount, renderTargets, depthTarget);
    if (IsFailure(bindResult))
    {
        ENGINE_LOG_ERROR("BeginRenderPass: BindRenderTargets failed for pass: {}", renderPass->GetID());
        return bindResult;
    }
    if (renderPass->GetLoadOperation() == ERenderPassLoadOperation::RPLO_Clear)
    {
        for (uint32 i = 0; i < renderTargetCount; ++i)
        {
            RHITexture* tex = renderTargets[i];
            vec4 clearColor = tex->GetClearColor();
            if (renderPass->HasOverrideClearColor())
                clearColor = renderPass->GetOverrideClearColor();
            ClearRenderTarget(tex, clearColor);
        }
    }
    if (depthTarget && renderPass->GetStencilLoadOperation() == ERenderPassLoadOperation::RPLO_Clear)
    {
        ClearDepthStencil(depthTarget, 1.0f, 0);
    }
    m_CurrentRenderPass = renderPass;
    return EResult::Success;
}
EResult DirectX12RHI::EndRenderPass()
{
    if (!m_CommandList) return EResult::Fail;
    if (!m_CurrentRenderPass) return EResult::Success;
    if (IsFailure(BindRenderTarget(nullptr, nullptr)))
        return EResult::Fail;
    D3D12_RESOURCE_BARRIER barriers[MAX_RENDER_TARGET_COUNT] = {};
    uint32 barrierCount = 0;
    const uint32 rtCount = std::min<uint32>(m_CurrentRenderPass->GetRenderTargetCount(), MAX_RENDER_TARGET_COUNT);
    for (uint32 i = 0; i < rtCount; ++i)
    {
        const wstring& name = m_CurrentRenderPass->GetRenderTargetName(i);
        RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(name);
        if (!rt || !rt->GetTexture()) return EResult::Fail;
        auto* texture = static_cast<DirectX12Texture*>(rt->GetTexture());
        if (!HasFlag(texture->GetUsage(), ETextureUsage::Sampler)) continue;
        const D3D12_RESOURCE_STATES before = texture->GetCurrentState();
        if ((before & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != 0) continue;
        D3D12_RESOURCE_BARRIER& barrier = barriers[barrierCount++];
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        texture->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (barrierCount > 0)
        m_CommandList->ResourceBarrier(barrierCount, barriers);
    m_CurrentRenderPass = nullptr;
    return EResult::Success;
}
EResult DirectX12RHI::ClearRenderPass()
{
    return EndRenderPass();
}
#pragma endregion

#pragma region Clear 
EResult DirectX12RHI::ClearRenderTarget(RHITexture* renderTarget, vec4 color)
{
    if (!m_CommandList || !renderTarget) return EResult::Fail;
    DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(renderTarget);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxTex->GetRTVHandle();
    if (rtvHandle.ptr == 0)
    {
        ENGINE_LOG_ERROR("ClearRenderTarget: RTV descriptor handle is null.");
        return EResult::Fail;
    }

    f32 clearColor[4] = { color.x, color.y, color.z, color.w };
    m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    return EResult::Success;
}

EResult DirectX12RHI::ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil)
{
    if (!m_CommandList || !depthStencil) return EResult::Fail;
    DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(depthStencil);
    const DXGI_FORMAT format = ToDXGIFormat(dxTex->GetFormat());
    const D3D12_CLEAR_FLAGS clearFlags = (format == DXGI_FORMAT_D24_UNORM_S8_UINT)
        ? (D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL)
        : D3D12_CLEAR_FLAG_DEPTH;

    if (dxTex->GetDimension() == ETextureDimension::TextureCube)
    {
        for (uint32 slice = 0; slice < 6; ++slice)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxTex->GetDSVHandle(slice);
            m_CommandList->ClearDepthStencilView(dsvHandle, clearFlags, depth, stencil, 0, nullptr);
        }
        return EResult::Success;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxTex->GetDSVHandle();
    m_CommandList->ClearDepthStencilView(dsvHandle, clearFlags, depth, stencil, 0, nullptr);
    return EResult::Success;
}

EResult DirectX12RHI::ClearDepthSlice(RHITexture* depthStencil, uint32 sliceIndex, f32 depth, uint8 stencil)
{
    if (!m_CommandList || !depthStencil) return EResult::Fail;
    DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(depthStencil);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxTex->GetDSVHandle(sliceIndex);
    const DXGI_FORMAT format = ToDXGIFormat(dxTex->GetFormat());
    const D3D12_CLEAR_FLAGS clearFlags = (format == DXGI_FORMAT_D24_UNORM_S8_UINT)
        ? (D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL)
        : D3D12_CLEAR_FLAG_DEPTH;
    m_CommandList->ClearDepthStencilView(dsvHandle, clearFlags, depth, stencil, 0, nullptr);
    return EResult::Success;
}
#pragma endregion

#pragma region MyRegion
void DirectX12RHI::DeferRelease(ID3D12Object* object)
{
    if (!object) return;

    m_DeferredReleases.emplace_back();
    auto& entry = m_DeferredReleases.back();
    entry.fenceValue = m_CurrentFenceValue + 1;
    entry.object = object;
}

void DirectX12RHI::DeferDescriptorFree(DirectX12DescriptorAllocator* allocator, uint32 index)
{
    if (!allocator || index == UINT32_MAX) return;

    m_DeferredReleases.emplace_back();
    auto& entry = m_DeferredReleases.back();
    entry.fenceValue = m_CurrentFenceValue + 1;
    entry.descriptorAllocator = allocator;
    entry.descriptorIndex = index;
}

EResult DirectX12RHI::SignalFence(uint64& fenceValue)
{
    if (!m_CommandQueue || !m_Fence) return EResult::Fail;

    const uint64 nextValue = m_CurrentFenceValue + 1;
    if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), nextValue)))
        return EResult::Fail;

    m_CurrentFenceValue = nextValue;
    fenceValue = nextValue;
    return EResult::Success;
}

EResult DirectX12RHI::WaitForFence(uint64 fenceValue)
{
    if (!m_Fence) return EResult::Fail;

    uint64 completedValue = m_Fence->GetCompletedValue();
    if (completedValue == UINT64_MAX) return EResult::Fail;

    if (completedValue < fenceValue)
    {
        if (FAILED(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent)))
            return EResult::Fail;

        if (m_FenceEvent && ::WaitForSingleObject(m_FenceEvent, INFINITE) != WAIT_OBJECT_0)
            return EResult::Fail;

        completedValue = m_Fence->GetCompletedValue();
        if (completedValue == UINT64_MAX || completedValue < fenceValue)
            return EResult::Fail;
    }

    CollectDeferredReleases();
    return EResult::Success;
}

EResult DirectX12RHI::WaitForGPU()
{
    uint64 fenceValue = 0;
    const EResult result = SignalFence(fenceValue);
    if (IsFailure(result)) return result;

    return WaitForFence(fenceValue);
}

void DirectX12RHI::CollectDeferredReleases()
{
    if (!m_Fence) return;

    const uint64 completedValue = m_Fence->GetCompletedValue();
    if (completedValue == UINT64_MAX) return;

    std::erase_if(m_DeferredReleases, [completedValue](const DeferredReleaseEntry& entry)
        {
            if (entry.fenceValue > completedValue) return false;

            if (entry.descriptorAllocator)
                entry.descriptorAllocator->Free(entry.descriptorIndex);

            return true;
        });
}
#pragma endregion

#pragma region Pipeline Library
void DirectX12RHI::InitializePipelineLibrary()
{
    if (!m_Device) return;
    ComPtr<ID3D12Device1> device1;
    if (FAILED(m_Device.As(&device1)) || !device1)
    {
        ENGINE_LOG_WARN("ID3D12Device1 not supported. Pipeline Library disabled.");
        return;
    }
    const auto cacheDir = PlatformPath::GetProjectRoot() / ".bam" / "cache" / "shaders";
    const auto cachePath = cacheDir / "pipeline.library";
    const auto namesPath = cacheDir / "pipeline.names";
    std::error_code ec;
    const bool hasLibrary = std::filesystem::exists(cachePath, ec);
    const bool hasNames = std::filesystem::exists(namesPath, ec);
    // 둘 중 하나만 존재하면 정합성이 깨진 것이므로 모두 삭제하고 새로 시작합니다.
    if (hasLibrary != hasNames)
    {
        std::filesystem::remove(cachePath, ec);
        std::filesystem::remove(namesPath, ec);
    }
    m_PipelineLibraryData.clear();
    m_PipelineNames.clear();
    if (hasLibrary && hasNames)
    {
        std::wifstream namesFile(namesPath);
        std::wstring line;
        while (std::getline(namesFile, line))
        {
            if (!line.empty()) m_PipelineNames.insert(line);
        }
        std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
        if (file.is_open())
        {
            const auto size = file.tellg();
            if (size > 0 && size < 128 * 1024 * 1024)
            {
                m_PipelineLibraryData.resize(static_cast<size_t>(size));
                file.seekg(0, std::ios::beg);
                file.read(reinterpret_cast<char*>(m_PipelineLibraryData.data()), size);
            }
        }
    }
    HRESULT hr = E_FAIL;
    if (!m_PipelineLibraryData.empty())
    {
        hr = device1->CreatePipelineLibrary(m_PipelineLibraryData.data(), m_PipelineLibraryData.size(), IID_PPV_ARGS(&m_PipelineLibrary));
        if (SUCCEEDED(hr))
        {
            ENGINE_LOG_INFO("Loaded D3D12 Pipeline Library ({} bytes, {} PSOs)", m_PipelineLibraryData.size(), m_PipelineNames.size());
            return;
        }
        ENGINE_LOG_WARN("Failed to load cached Pipeline Library: HRESULT=0x{:08X}. Resetting.", static_cast<uint32>(hr));
        std::filesystem::remove(cachePath, ec);
        std::filesystem::remove(namesPath, ec);
        m_PipelineLibraryData.clear();
        m_PipelineNames.clear();
    }
    hr = device1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_PipelineLibrary));
    if (FAILED(hr))
    {
        ENGINE_LOG_WARN("CreatePipelineLibrary not supported: HRESULT=0x{:08X}", static_cast<uint32>(hr));
        m_PipelineLibrary.Reset();
    }
}

void DirectX12RHI::SavePipelineLibrary()
{
    if (!m_PipelineLibrary || !m_PipelineLibraryDirty) return;

    if (m_Device && FAILED(m_Device->GetDeviceRemovedReason()))
    {
        ENGINE_LOG_WARN("Cannot serialize Pipeline Library: Device is removed (Reason=0x{:08X})",
            static_cast<uint32>(m_Device->GetDeviceRemovedReason()));
        return;
    }

    const size_t librarySize = m_PipelineLibrary->GetSerializedSize();
    if (librarySize == 0) return;

    std::vector<uint8_t> buffer(librarySize);
    const HRESULT hr = m_PipelineLibrary->Serialize(buffer.data(), librarySize);
    if (FAILED(hr))
    {
        const HRESULT reason = m_Device ? m_Device->GetDeviceRemovedReason() : E_FAIL;
        ENGINE_LOG_WARN("Failed to serialize Pipeline Library: HRESULT=0x{:08X}, DeviceRemovedReason=0x{:08X}",
            static_cast<uint32>(hr), static_cast<uint32>(reason));
        return;
    }

    const auto cacheDir = PlatformPath::GetProjectRoot() / ".bam" / "cache" / "shaders";
    std::error_code ec;
    std::filesystem::create_directories(cacheDir, ec);

    const auto cachePath = cacheDir / "pipeline.library";
    const auto tmpPath = cacheDir / "pipeline.library.tmp";
    const auto namesPath = cacheDir / "pipeline.names";
    const auto tmpNamesPath = cacheDir / "pipeline.names.tmp";

    // 바이너리 라이브러리 저장
    std::ofstream outFile(tmpPath, std::ios::binary | std::ios::trunc);
    if (outFile.is_open())
    {
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
        std::filesystem::rename(tmpPath, cachePath, ec);
    }

    // 파이프라인 이름 목록 저장 (임시 파일 거쳐 rename)
    std::wofstream namesFile(tmpNamesPath, std::ios::trunc);
    if (namesFile.is_open())
    {
        for (const auto& name : m_PipelineNames)
            namesFile << name << L"\n";
        namesFile.close();
        std::filesystem::rename(tmpNamesPath, namesPath, ec);
    }

    ENGINE_LOG_INFO("Saved D3D12 Pipeline Library ({} bytes, {} PSOs)", buffer.size(), m_PipelineNames.size());
    m_PipelineLibraryDirty = false;
}
#pragma endregion


#pragma region Draw
EResult DirectX12RHI::ApplyShaderResources()
{
    if (!m_CommandList) return EResult::Fail;

    const bool isCompute = m_CurrentPipeline && m_CurrentPipeline->GetPipelineType() == EPipelineType::Compute;
    static constexpr uint32 rootParameterMap[MAX_CONSTANT_BUFFER_SLOTS] = { 0, 3, 5, 6 };

    for (uint32 slot = 0; slot < MAX_CONSTANT_BUFFER_SLOTS; ++slot)
    {
        const ConstantBufferBinding& binding = m_ConstantBuffers[slot];
        if (!binding.buffer) continue;

        auto* resource = static_cast<ID3D12Resource*>(binding.buffer->GetNativeHandle());
        if (!resource) return EResult::Fail;

        D3D12_GPU_VIRTUAL_ADDRESS address = resource->GetGPUVirtualAddress();
        if (binding.isDynamic) address += binding.offset;

        if (isCompute)
        {
            if (rootParameterMap[slot] == 3) continue; // Vertex 전용 Object CBV는 Compute에서 바인딩 스킵
            m_CommandList->SetComputeRootConstantBufferView(rootParameterMap[slot], address);
        }
        else
        {
            m_CommandList->SetGraphicsRootConstantBufferView(rootParameterMap[slot], address);
        }
    }

    if (!m_CurrentRenderPass && !isCompute) return EResult::Fail;

    // Compute는 SRV 테이블 뒤에 쓰기 텍스처 UAV 테이블(u2-u5)을 이어서 기록한다.
    const uint32 requiredDescriptors = SRV_TABLE_SIZE + (isCompute ? UAV_TABLE_SIZE : 0);
    ID3D12Resource* resources[SRV_TABLE_SIZE] = {};
    DirectX12Texture* samplingTextures[SRV_TABLE_SIZE] = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC views[SRV_TABLE_SIZE] = {};

    const bool isLighting = isCompute || (m_CurrentRenderPass && m_CurrentRenderPass->GetPassType() == ERenderPassType::Lighting);

    for (uint32 slot = 0; slot < SRV_TABLE_SIZE; ++slot)
    {
        auto& view = views[slot];
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        // Compute의 TLAS SRV: 리소스 없이 GPU 주소로 지정한다(가속 구조는 상태 전환을 하지 않는다).
        if (isCompute && m_ComputeAccelerationStructure && slot == m_ComputeAccelerationStructureSlot)
        {
            if (m_CurrentTextures[slot])
            {
                ENGINE_LOG_ERROR("A texture and an acceleration structure are bound to the same compute SRV slot.");
                return EResult::InvalidArgument;
            }
            view.Format = DXGI_FORMAT_UNKNOWN;
            view.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            view.RaytracingAccelerationStructure.Location =
                static_cast<DirectX12AccelerationStructure*>(m_ComputeAccelerationStructure)->GetGPUAddress();
            continue;
        }

        // Lighting의 t6, space3은 Texture2D가 아니라 ByteAddressBuffer입니다.
        if (isLighting && slot == MAX_MATERIAL_TEXTURE_SLOTS + 6)
        {
            RHIBuffer* buffer = m_StorageBuffers[0];
            if (!buffer || !buffer->GetNativeHandle()) return EResult::Fail;

            resources[slot] = static_cast<ID3D12Resource*>(buffer->GetNativeHandle());
            const auto bufferDesc = resources[slot]->GetDesc();
            if (bufferDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) return EResult::Fail;
            if (bufferDesc.Width == 0 || bufferDesc.Width % 4 != 0) return EResult::Fail;
            if (bufferDesc.Width / 4 > 0xFFFFFFFFull) return EResult::Fail;

            view.Format = DXGI_FORMAT_R32_TYPELESS;
            view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            view.Buffer.NumElements = static_cast<UINT>(bufferDesc.Width / 4);
            view.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            continue;
        }

        // Lighting의 t8, space3: Tile Light Grid (StructuredBuffer<uint2>, stride 8)
        if (isLighting && slot == MAX_MATERIAL_TEXTURE_SLOTS + 8)
        {
            RHIBuffer* buffer = m_StorageBuffers[1];
            if (buffer && buffer->GetNativeHandle())
            {
                resources[slot] = static_cast<ID3D12Resource*>(buffer->GetNativeHandle());
                const auto bufferDesc = resources[slot]->GetDesc();
                if (bufferDesc.Width >= 8)
                {
                    view.Format = DXGI_FORMAT_UNKNOWN;
                    view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    view.Buffer.FirstElement = 0;
                    view.Buffer.NumElements = static_cast<UINT>(bufferDesc.Width / 8);
                    view.Buffer.StructureByteStride = 8;
                    view.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
                    continue;
                }
            }
        }

        // Lighting의 t9, space3: Tile Light Index List (StructuredBuffer<uint>, stride 4)
        if (isLighting && slot == MAX_MATERIAL_TEXTURE_SLOTS + 9)
        {
            RHIBuffer* buffer = m_StorageBuffers[2];
            if (buffer && buffer->GetNativeHandle())
            {
                resources[slot] = static_cast<ID3D12Resource*>(buffer->GetNativeHandle());
                const auto bufferDesc = resources[slot]->GetDesc();
                if (bufferDesc.Width >= 4)
                {
                    view.Format = DXGI_FORMAT_UNKNOWN;
                    view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    view.Buffer.FirstElement = 0;
                    view.Buffer.NumElements = static_cast<UINT>(bufferDesc.Width / 4);
                    view.Buffer.StructureByteStride = 4;
                    view.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
                    continue;
                }
            }
        }

        // 사용하지 않는 슬롯도 유효한 null SRV로 채웁니다.
        view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (isLighting && (slot == MAX_MATERIAL_TEXTURE_SLOTS + 7 || slot == MAX_MATERIAL_TEXTURE_SLOTS + 10 || slot == MAX_MATERIAL_TEXTURE_SLOTS + 11))
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view.TextureCube.MipLevels = 1;
            view.TextureCube.MostDetailedMip = 0;
        }
        else
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipLevels = 1;
        }

        RHITexture* source = m_CurrentTextures[slot];
        if (!source) continue;
        if (!HasFlag(source->GetUsage(), ETextureUsage::Sampler)) return EResult::Fail;

        auto* texture = static_cast<DirectX12Texture*>(source);
        auto* resource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        if (!resource) return EResult::Fail;

        const auto resourceDesc = resource->GetDesc();
        const bool isTexCube = (texture->GetDimension() == Engine::ETextureDimension::TextureCube)
            || (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resourceDesc.DepthOrArraySize == 6);
        const bool isTex2D = (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D
            && resourceDesc.DepthOrArraySize == 1 && resourceDesc.SampleDesc.Count == 1);
        const bool isTex3D = (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D && resourceDesc.SampleDesc.Count == 1);

        if (!isTex2D && !isTex3D && !isTexCube)
        {
            ENGINE_LOG_ERROR("SRV binding supports single-sample Texture2D, Texture3D, and TextureCube inputs.");
            return EResult::NotImplemented;
        }

        if (m_CurrentRenderPass)
        {
            for (uint32 i = 0; i < m_CurrentRenderPass->GetRenderTargetCount(); ++i)
            {
                auto* output = RenderTargetManager::Get().GetRenderTarget(m_CurrentRenderPass->GetRenderTargetName(i));
                if (output && output->GetTexture() == source) return EResult::InvalidArgument;
            }

            const wstring depthName = m_CurrentRenderPass->GetDepthStencilName();
            if (!depthName.empty())
            {
                auto* depth = RenderTargetManager::Get().GetRenderTarget(depthName);
                if (depth && depth->GetTexture() == source) return EResult::InvalidArgument;
            }
        }

        const DXGI_FORMAT format = ToDXGIFormat(texture->GetFormat());
        view.Format = IsDepthFormat(format) ? ToDepthSRVFormat(format) : resourceDesc.Format;
        if (isTex3D)
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            view.Texture3D.MipLevels = resourceDesc.MipLevels;
        }
        else if (isTexCube)
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view.TextureCube.MipLevels = resourceDesc.MipLevels;
            view.TextureCube.MostDetailedMip = 0;
            view.TextureCube.ResourceMinLODClamp = 0.0f;
        }
        else
        {
            view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MipLevels = resourceDesc.MipLevels;
        }

        resources[slot] = resource;
        samplingTextures[slot] = texture;
    }

    // Compute 쓰기 텍스처(u2-u5, space3) 검사. 빈 슬롯은 유효한 null UAV로 채운다.
    ID3D12Resource* uavResources[UAV_TABLE_SIZE] = {};
    DirectX12Texture* storageTextures[UAV_TABLE_SIZE] = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavViews[UAV_TABLE_SIZE] = {};
    if (isCompute)
    {
        for (uint32 slot = 0; slot < UAV_TABLE_SIZE; ++slot)
        {
            auto& uavView = uavViews[slot];
            uavView.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

            RHITexture* source = m_ComputeStorageTextures[slot];
            if (!source) continue;

            auto* texture = static_cast<DirectX12Texture*>(source);
            for (const DirectX12Texture* sampled : samplingTextures)
            {
                if (sampled == texture)
                {
                    ENGINE_LOG_ERROR("A texture cannot be bound as both a compute SRV and a storage UAV in one dispatch.");
                    return EResult::InvalidArgument;
                }
            }

            auto* resource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
            if (!resource) return EResult::Fail;

            const auto resourceDesc = resource->GetDesc();
            const bool isTex2D = resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D
                && resourceDesc.DepthOrArraySize == 1 && resourceDesc.SampleDesc.Count == 1;
            if (!isTex2D || IsDepthFormat(ToDXGIFormat(texture->GetFormat())) ||
                (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
            {
                ENGINE_LOG_ERROR("Compute storage texture must be a single-sample color Texture2D created with UAV access.");
                return EResult::NotImplemented;
            }

            uavView.Format = resourceDesc.Format;
            uavView.Texture2D.MipSlice = 0;
            uavResources[slot] = resource;
            storageTextures[slot] = texture;
        }
    }

    // Compute 테이블은 TLAS·UAV가 이어지고 Dispatch 수가 적어 매번 새로 기록한다.
    SRVTableKey tableKey;
    auto cachedTable = m_FrameSRVTables.end();
    if (!isCompute)
    {
        std::copy(std::begin(resources), std::end(resources), tableKey.Resources.begin());
        std::copy(std::begin(samplingTextures), std::end(samplingTextures), tableKey.Textures.begin());
        tableKey.IsLighting = isLighting;
        cachedTable = m_FrameSRVTables.find(tableKey);
    }
    const bool reuseTable = cachedTable != m_FrameSRVTables.end();
    if (!reuseTable && m_DynamicHeapCursor > FRAME_SRV_DESCRIPTOR_COUNT - requiredDescriptors)
    {
        ENGINE_LOG_ERROR("Frame SRV table capacity exceeded.");
        return EResult::Fail;
    }

    // 입력 검사가 끝난 뒤 필요한 상태 전환을 기록합니다.
    for (auto* texture : samplingTextures)
    {
        if (!texture) continue;

                const D3D12_RESOURCE_STATES targetState = isCompute
            ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        const D3D12_RESOURCE_STATES before = texture->GetCurrentState();
        if ((before & targetState) != 0) continue;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = targetState;

        m_CommandList->ResourceBarrier(1, &barrier);
        texture->SetCurrentState(targetState);
    }

    for (auto* texture : storageTextures)
    {
        if (!texture) continue;

        const D3D12_RESOURCE_STATES before = texture->GetCurrentState();
        if (before == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) continue;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        m_CommandList->ResourceBarrier(1, &barrier);
        texture->SetCurrentState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    const uint32 tableIndex = reuseTable ? cachedTable->second
        : m_SRVTableStartIndices[m_CurrentBackBufferIndex] + m_DynamicHeapCursor;
    const uint32 descriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE destination = m_SrvAllocator->GetHeap()->GetCPUDescriptorHandleForHeapStart();
    destination.ptr += static_cast<SIZE_T>(tableIndex) * descriptorSize;

    if (!reuseTable)
    {
        for (uint32 slot = 0; slot < SRV_TABLE_SIZE; ++slot)
        {
            m_Device->CreateShaderResourceView(resources[slot], &views[slot], destination);
            destination.ptr += descriptorSize;
        }
    }

    if (isCompute)
    {
        for (uint32 slot = 0; slot < UAV_TABLE_SIZE; ++slot)
        {
            m_Device->CreateUnorderedAccessView(uavResources[slot], nullptr, &uavViews[slot], destination);
            destination.ptr += descriptorSize;
        }

        m_CommandList->SetComputeRootDescriptorTable(1, m_SrvAllocator->GetGPUHandle(tableIndex));
        m_CommandList->SetComputeRootDescriptorTable(7, m_SrvAllocator->GetGPUHandle(tableIndex + MAX_MATERIAL_TEXTURE_SLOTS));
        m_CommandList->SetComputeRootDescriptorTable(COMPUTE_UAV_TABLE_ROOT_INDEX, m_SrvAllocator->GetGPUHandle(tableIndex + SRV_TABLE_SIZE));
    }
    else
    {
        m_CommandList->SetGraphicsRootDescriptorTable(1, m_SrvAllocator->GetGPUHandle(tableIndex));
        m_CommandList->SetGraphicsRootDescriptorTable(7, m_SrvAllocator->GetGPUHandle(tableIndex + MAX_MATERIAL_TEXTURE_SLOTS));
    }

    if (!reuseTable)
    {
        m_DynamicHeapCursor += requiredDescriptors;
        if (!isCompute)
            m_FrameSRVTables.emplace(tableKey, tableIndex);
    }

    return EResult::Success;
}
EResult DirectX12RHI::Draw(uint32 count)
{
    if(!m_CommandList) return EResult::Fail;

    if (m_VertexBuffers[0])
    {
        const D3D12_VERTEX_BUFFER_VIEW vbView = GetBoundVertexBufferView(0);
        m_CommandList->IASetVertexBuffers(0, 1, &vbView);
    }

    if (m_IndexBuffer)
    {
        DirectX12Buffer* dxBuffer = static_cast<DirectX12Buffer*>(m_IndexBuffer);
		D3D12_INDEX_BUFFER_VIEW ibView = dxBuffer->GetIndexBufferView();
		m_CommandList->IASetIndexBuffer(&ibView);
    }

    if (IsFailure(ApplyShaderResources()))
    {
		return EResult::Fail;
    }

    m_CommandList->DrawInstanced(count, 1, 0, 0);
    RecordDraw(count);
    return EResult::Success;
}

EResult DirectX12RHI::DrawIndexed(uint32 count)
{
    if(!m_CommandList) return EResult::Fail;

    if (m_NumVertexBuffersBound > MAX_BUFFER_SLOTS)
        return EResult::Fail;

    D3D12_VERTEX_BUFFER_VIEW views[MAX_BUFFER_SLOTS] = {};

    for (uint32 slot = 0; slot < m_NumVertexBuffersBound; ++slot)
    {
        if (m_VertexBuffers[slot])
            views[slot] = GetBoundVertexBufferView(slot);
    }

    if (m_NumVertexBuffersBound > 0)
    {
        m_CommandList->IASetVertexBuffers(
            0,
            m_NumVertexBuffersBound,
            views);
    }

    if (m_IndexBuffer)
    {
        DirectX12Buffer* dxBuffer = static_cast<DirectX12Buffer*>(m_IndexBuffer);
        D3D12_INDEX_BUFFER_VIEW ibView = dxBuffer->GetIndexBufferView();
        m_CommandList->IASetIndexBuffer(&ibView);
    }

    // CBV 슬롯 → 루트 파라미터 인덱스 매핑
    // slot 0 = Global CBV  → Root Param 0
    // slot 1 = Object CBV  → Root Param 3
    // slot 2 = Pass CBV #0 → Root Param 5
    // slot 3 = Pass CBV #1 → Root Param 6
    if (IsFailure(ApplyShaderResources()))
        return EResult::Fail;

    m_CommandList->DrawIndexedInstanced(count, 1, 0, 0, 0);
    RecordIndexedDraw(count);
    return EResult::Success;
}

EResult DirectX12RHI::DrawInstanced(uint32 vertexCountPerInstance, uint32 instanceCount, uint32 startVertexLocation, uint32 startInstanceLocation)
{
    if (!m_CommandList || instanceCount == 0 || vertexCountPerInstance == 0)
        return EResult::InvalidArgument;

    if (m_NumVertexBuffersBound > MAX_BUFFER_SLOTS)
        return EResult::Fail;

    D3D12_VERTEX_BUFFER_VIEW views[MAX_BUFFER_SLOTS] = {};
    for (uint32 slot = 0; slot < m_NumVertexBuffersBound; ++slot)
    {
        if (m_VertexBuffers[slot])
            views[slot] = GetBoundVertexBufferView(slot);
    }

    if (m_NumVertexBuffersBound > 0)
        m_CommandList->IASetVertexBuffers(0, m_NumVertexBuffersBound, views);

    if (IsFailure(ApplyShaderResources()))
        return EResult::Fail;

    m_CommandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    RecordDraw(vertexCountPerInstance * instanceCount);
    return EResult::Success;
}

EResult DirectX12RHI::DrawIndexedInstanced(uint32 indexCountPerInstance, uint32 instanceCount, uint32 startIndexLocation, int32 baseVertexLocation, uint32 startInstanceLocation)
{
    if (!m_CommandList || instanceCount == 0 || indexCountPerInstance == 0)
        return EResult::InvalidArgument;

    if (m_NumVertexBuffersBound > MAX_BUFFER_SLOTS)
        return EResult::Fail;

    D3D12_VERTEX_BUFFER_VIEW views[MAX_BUFFER_SLOTS] = {};
    for (uint32 slot = 0; slot < m_NumVertexBuffersBound; ++slot)
    {
        if (m_VertexBuffers[slot])
            views[slot] = GetBoundVertexBufferView(slot);
    }

    if (m_NumVertexBuffersBound > 0)
        m_CommandList->IASetVertexBuffers(0, m_NumVertexBuffersBound, views);

    if (m_IndexBuffer)
    {
        DirectX12Buffer* dxBuffer = static_cast<DirectX12Buffer*>(m_IndexBuffer);
        D3D12_INDEX_BUFFER_VIEW ibView = dxBuffer->GetIndexBufferView();
        m_CommandList->IASetIndexBuffer(&ibView);
    }

    if (IsFailure(ApplyShaderResources()))
        return EResult::Fail;

    m_CommandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    RecordIndexedDraw(indexCountPerInstance * instanceCount);
    return EResult::Success;
}

EResult DirectX12RHI::DrawTexture(RHITexture* texture)
{
    return EResult::Fail;
}
#pragma endregion

#pragma region Compute
EResult DirectX12RHI::Dispatch(uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ)
{
    if (!m_CommandList || !m_FrameRecording)
        return EResult::Fail;

    if (threadGroupCountX == 0 || threadGroupCountY == 0 || threadGroupCountZ == 0)
        return EResult::InvalidArgument;

    const EResult applyResult = ApplyShaderResources();
    if (IsSuccess(applyResult))
        m_CommandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);

    // 쓰기 텍스처·TLAS 바인딩은 이번 Dispatch에만 적용한다. 다음 Compute(예: 타일 컬링)가 이전 출력을 다시 쓰지 않게 한다.
    std::fill(std::begin(m_ComputeStorageTextures), std::end(m_ComputeStorageTextures), nullptr);
    m_ComputeAccelerationStructure = nullptr;
    m_ComputeAccelerationStructureSlot = UINT32_MAX;
    return IsSuccess(applyResult) ? EResult::Success : EResult::Fail;
}
RHIAccelerationStructure* DirectX12RHI::CreateBottomLevelAS(const RHIRaytracingGeometryDesc& desc)
{
    return DirectX12AccelerationStructure::CreateBottomLevel(this, desc);
}
EResult DirectX12RHI::BuildTopLevelAS(RHIAccelerationStructure*& topLevel, const RHIRaytracingInstanceDesc* instances, uint32 instanceCount)
{
    return DirectX12AccelerationStructure::BuildTopLevel(this, topLevel, instances, instanceCount);
}
EResult DirectX12RHI::BindAccelerationStructure(RHIAccelerationStructure* topLevel, uint32 slot)
{
    if (!m_CommandList || !m_FrameRecording)
        return EResult::Fail;
    if (slot < MAX_MATERIAL_TEXTURE_SLOTS || slot >= SRV_TABLE_SIZE)
        return EResult::InvalidArgument;
    if (topLevel && (topLevel->GetType() != ERHIAccelerationStructureType::TopLevel || !topLevel->GetNativeHandle()))
        return EResult::InvalidArgument;
    m_ComputeAccelerationStructure = topLevel;
    m_ComputeAccelerationStructureSlot = topLevel ? slot : UINT32_MAX;
    return EResult::Success;
}
EResult DirectX12RHI::ResourceBarrierUAV(RHITexture* texture)
{
    if (!m_CommandList || !m_FrameRecording || !texture)
        return EResult::Fail;
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
    m_CommandList->ResourceBarrier(1, &barrier);
    return EResult::Success;
}
EResult DirectX12RHI::ResourceBarrierUAV(RHIBuffer* buffer)
{
    if (!m_CommandList || !m_FrameRecording)
        return EResult::Fail;
    ID3D12Resource* resource = buffer ? static_cast<ID3D12Resource*>(buffer->GetNativeHandle()) : nullptr;
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    m_CommandList->ResourceBarrier(1, &barrier);
    return EResult::Success;
}
#pragma endregion

#pragma region Setter
EResult DirectX12RHI::SetClearColor(vec4 color)
{
    m_ClearColor = color;
    return EResult::Success;
}

EResult DirectX12RHI::SetViewport(int32 x, int32 y, uint32 width, uint32 height)
{
    if (!m_CommandList) return EResult::Fail;
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = static_cast<FLOAT>(x);
    viewport.TopLeftY = static_cast<FLOAT>(y);
    viewport.Width = static_cast<FLOAT>(width);
    viewport.Height = static_cast<FLOAT>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor = { x, y, x + (LONG)width, y + (LONG)height };
    m_CommandList->RSSetViewports(1, &viewport);
    m_CommandList->RSSetScissorRects(1, &scissor);
    return EResult::Success;
}

EResult DirectX12RHI::SetScissorRect(uint32 left, uint32 top, uint32 right, uint32 bottom)
{
    if (!m_CommandList || left >= right || top >= bottom || right > INT32_MAX || bottom > INT32_MAX)
        return EResult::InvalidArgument;
    const D3D12_RECT rect{ static_cast<LONG>(left), static_cast<LONG>(top), static_cast<LONG>(right),
        static_cast<LONG>(bottom) };
    m_CommandList->RSSetScissorRects(1, &rect);
    return EResult::Success;
}
#pragma endregion

#pragma region Frame Metrics
void DirectX12RHI::InitializeCapabilities()
{
    m_Capabilities.SupportsCompute = true;      // D3D12 기본 지원
    m_Capabilities.SupportsIndirectDraw = true; // ExecuteIndirect 기본 지원
    m_Capabilities.MaxComputeWorkGroupInvocations = 1024;

    // Mesh Shader 지원 여부
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
        m_Capabilities.SupportsMeshShaders = (options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1);

    // DXR (Raytracing) 지원 여부
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
    {
        m_Capabilities.SupportsRaytracing = (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);
        if (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1)
            m_Capabilities.RaytracingTier = 11;
        else if (m_Capabilities.SupportsRaytracing)
            m_Capabilities.RaytracingTier = 10;
    }

    // 최고 Shader Model: 런타임이 모르는 버전을 요청하면 실패하므로 높은 버전부터 낮춰 가며 조회한다.
    constexpr D3D_SHADER_MODEL shaderModelCandidates[] = {
        D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4,
        D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2, D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0 };
    for (const D3D_SHADER_MODEL candidate : shaderModelCandidates)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { candidate };
        if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
        {
            m_Capabilities.HighestShaderModel = static_cast<uint32>(shaderModel.HighestShaderModel);
            break;
        }
    }

    // 인라인 RayQuery: DXR Tier 1.1 + Shader Model 6.5 + 가속 구조 생성·기록용 인터페이스.
    m_Capabilities.SupportsInlineRaytracing = m_Device5 && m_CommandList4 && m_Capabilities.RaytracingTier >= 11 &&
        m_Capabilities.HighestShaderModel >= static_cast<uint32>(D3D_SHADER_MODEL_6_5);

    ENGINE_LOG_INFO("D3D12 raytracing tier: {}, highest shader model: 0x{:X}, inline raytracing: {}",
        m_Capabilities.RaytracingTier, m_Capabilities.HighestShaderModel, m_Capabilities.SupportsInlineRaytracing);

    // Bindless (Descriptor Indexing) 지원 여부
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
        m_Capabilities.SupportsDescriptorIndexing = (options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);
}

void DirectX12RHI::InitializeTimestampQueries()
{
    if (FAILED(m_CommandQueue->GetTimestampFrequency(&m_TimestampFrequency)))
        m_TimestampFrequency = 0;

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = MAX_TIMESTAMP_QUERIES;
    if (FAILED(m_Device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_TimestampQueryHeap))))
        return;

    const uint64 bufferSize = MAX_TIMESTAMP_QUERIES * sizeof(uint64);
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = bufferSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (uint32 i = 0; i < MAX_SWAPCHAIN_BUFFERS; ++i)
    {
        m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_TimestampReadbackBuffers[i]));
    }
}

void DirectX12RHI::BeginPassMetrics(const std::wstring& passName)
{
    RHI::BeginPassMetrics(passName);

    // 열린 구간마다 종료 쿼리 하나를 남겨 두어 중첩되어도 종료 시각을 기록할 수 있게 한다.
    uint32 openIndex = UINT32_MAX;
    if (m_CommandList && m_TimestampQueryHeap && (m_TimestampQueryCount + m_OpenTimestampCount + 2 <= MAX_TIMESTAMP_QUERIES))
    {
        const uint32 startIndex = m_TimestampQueryCount++;
        m_CommandList->EndQuery(m_TimestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startIndex);

        PassQueryRange range{};
        range.passName = passName;
        range.startIndex = startIndex;
        range.depth = static_cast<uint32>(m_OpenPassQueries.size());
        openIndex = static_cast<uint32>(m_ActivePassQueries.size());
        m_ActivePassQueries.push_back(range);
        ++m_OpenTimestampCount;
    }
    m_OpenPassQueries.push_back(openIndex);
}

void DirectX12RHI::EndPassMetrics()
{
    if (m_OpenPassQueries.empty())
        return;

    const uint32 openIndex = m_OpenPassQueries.back();
    m_OpenPassQueries.pop_back();
    if (openIndex != UINT32_MAX)
    {
        const uint32 endIndex = m_TimestampQueryCount++;
        m_CommandList->EndQuery(m_TimestampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endIndex);
        m_ActivePassQueries[openIndex].endIndex = endIndex;
        --m_OpenTimestampCount;
    }

    RHI::EndPassMetrics();

    // 직전 회수된 해당 패스의 GPU 시간 주입
    if (!m_CurrentFrameMetrics.PassMetrics.empty())
    {
        auto& metric = m_CurrentFrameMetrics.PassMetrics.back();
        if (auto it = m_LastGPUTimestampResults.find(metric.PassName); it != m_LastGPUTimestampResults.end())
            metric.GPUTimeMs = it->second.TimeMs;
    }
}

void DirectX12RHI::EndMetricsFrame(bool renderSucceeded)
{
    RHI::EndMetricsFrame(renderSucceeded);
    m_CurrentFrameMetrics.TotalGPUTimeMs = m_LastTotalGPUTimeMs;
    m_LastFrameMetrics = m_CurrentFrameMetrics;
}

RHIMemoryStats DirectX12RHI::QueryMemoryStats()
{
    RHIMemoryStats stats{};
    // 열거 순서의 첫 어댑터가 아니라 실제로 장치를 생성한 어댑터의 메모리를 조회한다.
    ComPtr<IDXGIAdapter3> adapter3;
    if (m_Adapter && SUCCEEDED(m_Adapter.As(&adapter3)))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO localInfo{};
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localInfo)))
        {
            stats.DedicatedVRAMBudgetMB = localInfo.Budget / (1024 * 1024);
            stats.DedicatedVRAMUsageMB = localInfo.CurrentUsage / (1024 * 1024);
        }
        DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalInfo{};
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalInfo)))
        {
            stats.SharedSystemMemoryUsageMB = nonLocalInfo.CurrentUsage / (1024 * 1024);
        }
    }

    if (m_RtvAllocator) stats.AllocatedDescriptors += m_RtvAllocator->GetAllocatedCount();
    if (m_DsvAllocator) stats.AllocatedDescriptors += m_DsvAllocator->GetAllocatedCount();
    if (m_SrvAllocator) stats.AllocatedDescriptors += m_SrvAllocator->GetAllocatedCount();
    if (m_SamplerAllocator) stats.AllocatedDescriptors += m_SamplerAllocator->GetAllocatedCount();

    return stats;
}
#pragma endregion
