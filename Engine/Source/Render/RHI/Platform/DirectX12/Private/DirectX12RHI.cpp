#pragma once
#include "DirectX12RHI.h"
#include "DirectX12Buffer.h"
#include "DirectX12Shader.h"
#include "DirectX12Sampler.h"
#include "DirectX12Texture.h"
#include "DirectX12Pipeline.h"
#include "RenderTargetManager.h"
#include "RenderPass.h"

#pragma region Constructor&Destructor
EResult DirectX12RHI::Initialize(void* arg)
{
    if (!arg)
        return EResult::InvalidArgument;

    CAST_DESC;
	m_WindowHandle = static_cast<HWND>(desc->windowHandle);
	m_SwapChainWidth = desc->width;
	m_SwapChainHeight = desc->height;
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

    // 디바이스 생성 (D3D_FEATURE_LEVEL_11_0 이상 하드웨어 지원)
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device))))
        return EResult::Fail;

#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_Device.As(&infoQueue)))
    {
        infoQueue->SetBreakOnID(
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            TRUE);
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
    if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator))))
        return EResult::Fail;

	// Command List 생성
    if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList))))
        return EResult::Fail;

    if (FAILED(m_CommandList->Close()))
        return EResult::Fail;

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
    m_CurrentFenceValue = 1;
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

        CD3DX12_ROOT_PARAMETER rootParameters[9];

        // [0] Global CBV (b0, space0) — Root Descriptor (직접 GPU 주소 바인딩)
        rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        // [1] Material SRV Table (t0-t15, space1) — Descriptor Table
        rootParameters[1].InitAsDescriptorTable(1, &srvRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        // [2] Material Sampler Table (s0-s7, space1) — Descriptor Table
        rootParameters[2].InitAsDescriptorTable(1, &srvRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);

        // [3] Object CBV (b0, space2) — Root Descriptor
        rootParameters[3].InitAsConstantBufferView(0, 2, D3D12_SHADER_VISIBILITY_VERTEX);

        // [4] Object SRV Table (t0, space2) — Descriptor Table
        rootParameters[4].InitAsDescriptorTable(1, &srvRanges[2], D3D12_SHADER_VISIBILITY_VERTEX);

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

    if (IsFailure(InitializeConstantBuffers()))
    {
        ENGINE_LOG_ERROR(
            "Failed to initialize dynamic constant buffers.");

        return EResult::Fail;
    }

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
    // 백버퍼 배열 원소를 가리키는 비소유 포인터입니다.
    m_BackBuffer = nullptr;

    // 바인딩된 리소스를 allocator가 살아 있는 동안 해제합니다.
    RHI::Free();

    for (auto& backBuffer : m_SwapChainBuffers)
    {
        Safe_Release(backBuffer);
        backBuffer = nullptr;
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

    if (!m_CommandAllocator || !m_CommandList || m_CurrentRenderPass)
        return EResult::Fail;

    if (m_SwapChainBufferCount == 0 || m_SwapChainBufferCount > MAX_SWAPCHAIN_BUFFERS)
        return EResult::Fail;

    if (width == m_SwapChainWidth && height == m_SwapChainHeight)
        return EResult::Success;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    if (FAILED(m_SwapChain->GetDesc1(&swapChainDesc)))
        return EResult::Fail;

    // 기존 백버퍼를 사용하는 GPU 작업이 끝날 때까지 대기합니다.
    const uint64 fenceValue = ++m_CurrentFenceValue;
    if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), fenceValue)))
        return EResult::Fail;

    if (m_Fence->GetCompletedValue() < fenceValue)
    {
        if (FAILED(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent)))
            return EResult::Fail;

        if (::WaitForSingleObject(m_FenceEvent, INFINITE) != WAIT_OBJECT_0)
            return EResult::Fail;
    }

    // 완료된 이전 프레임의 명령 기록도 비웁니다.
    if (FAILED(m_CommandAllocator->Reset()))
        return EResult::Fail;

    if (FAILED(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr)))
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
	m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	m_BackBuffer = m_SwapChainBuffers[m_CurrentBackBufferIndex];
	m_DynamicHeapCursor = 0;

	// 다이내믹 CBV 링 버퍼 커서 초기화 (화이트보드 되감기)
	m_DynamicBufferCursor = 0;
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

    if (FAILED(m_CommandAllocator->Reset()))
        return EResult::Fail;

    if (FAILED(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr)))
        return EResult::Fail;

    m_CommandList->SetGraphicsRootSignature(m_GlobalRootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = { m_SrvAllocator->GetHeap(), m_SamplerAllocator->GetHeap() };

    m_CommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    return EResult::Success;
}

EResult DirectX12RHI::EndFrame()
{

	DirectX12Texture* dxBackBuffer = static_cast<DirectX12Texture*>(m_BackBuffer);
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

    if (FAILED(m_CommandList->Close()))
        return EResult::Fail;

    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    if (FAILED(m_SwapChain->Present(1, 0)))
        return EResult::Fail;


    TODO("Fence를 이용한 GPU 동기화 구현 필요. 현재는 CPU가 GPU보다 빨리 진행될 수 있음. Fence를 사용하여 GPU가 작업을 완료할 때까지 기다리도록 구현해야 함.");
    TODO("멀티스레드 성능을 높이기 위해 비동기로 짜야함");
    const uint64 fenceValue = ++m_CurrentFenceValue;
    if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), fenceValue)))
        return EResult::Fail;

    if (m_Fence->GetCompletedValue() < fenceValue)
    {
        if (FAILED(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent)))
            return EResult::Fail;
        ::WaitForSingleObject(m_FenceEvent, INFINITE);
    }

    return EResult::Success;
}
#pragma endregion

#pragma region Create Resources

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

EResult DirectX12RHI::UploadTextureData(RHITexture* texture, const RHITextureDesc& desc)
{
    if (!texture || !desc.data || !m_Device || !m_CommandQueue)
        return EResult::InvalidArgument;

    if (desc.format != ETextureFormat::R8G8B8A8_UNORM ||
        desc.dimension != ETextureDimension::Texture2D ||
        desc.mipLevels != 1 ||
        desc.arraySize != 1 ||
        desc.depth != 1 ||
        desc.sampleCount != ETextureSampleCount::TextureSampleCount1 ||
        desc.width == 0 ||
        desc.height == 0)
    {
        return EResult::InvalidArgument;
    }

    const UINT64 sourceRowBytes = UINT64(desc.width) * 4;

    if (sourceRowBytes > UINT64(desc.dataSize) / desc.height)
        return EResult::InvalidArgument;

    auto* dxTexture = static_cast<DirectX12Texture*>(texture);
    auto* destination =
        static_cast<ID3D12Resource*>(dxTexture->GetNativeHandle());

    if (!destination)
        return EResult::Fail;

    // 1. 업로드 버퍼에 필요한 행 간격과 전체 크기
    const D3D12_RESOURCE_DESC resourceDesc = destination->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT rowCount = 0;
    UINT64 rowBytes = 0;
    UINT64 uploadBytes = 0;

    m_Device->GetCopyableFootprints(
        &resourceDesc, 0, 1, 0,
        &footprint, &rowCount, &rowBytes, &uploadBytes);

    if (rowCount != desc.height ||
        rowBytes != sourceRowBytes ||
        uploadBytes == 0)
    {
        return EResult::Fail;
    }

    // 2. CPU 쓰기가 가능한 업로드 버퍼
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = uploadBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> uploadBuffer;

    if (FAILED(m_Device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer))))
    {
        return EResult::Fail;
    }

    // 3. CPU 픽셀 → 업로드 버퍼
    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };

    if (FAILED(uploadBuffer->Map(0, &readRange, &mapped)))
        return EResult::Fail;

    auto* dst = static_cast<std::uint8_t*>(mapped) + footprint.Offset;
    const auto* src = static_cast<const std::uint8_t*>(desc.data);

    for (UINT y = 0; y < rowCount; ++y)
    {
        std::memcpy(
            dst + SIZE_T(y) * footprint.Footprint.RowPitch,
            src + SIZE_T(y) * static_cast<SIZE_T>(sourceRowBytes),
            static_cast<SIZE_T>(sourceRowBytes));
    }

    uploadBuffer->Unmap(0, nullptr);

    // 4. 프레임용 command list와 독립된 복사 명령 준비
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;

    if (FAILED(m_Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator))))
    {
        return EResult::Fail;
    }

    if (FAILED(m_Device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&list))))
    {
        return EResult::Fail;
    }

    if (FAILED(m_Device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
    {
        return EResult::Fail;
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
        list->ResourceBarrier(1, &barrier);
    }

    // 6. 업로드 버퍼 → 텍스처
    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource = uploadBuffer.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION target = {};
    target.pResource = destination;
    target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    target.SubresourceIndex = 0;

    list->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);

    // 7. ImGui가 픽셀 셰이더에서 읽을 수 있도록 전환
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    list->ResourceBarrier(1, &barrier);

    if (FAILED(list->Close()))
        return EResult::Fail;

    // 8. 실행 및 완료 대기
    ID3D12CommandList* lists[] = { list.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);

    // 제출 후 동기화 실패 시 자원을 해제하고 계속 진행하면 안 됩니다.
    // 현재 예제는 치명적 오류로 처리합니다.
    if (FAILED(m_CommandQueue->Signal(fence.Get(), 1)))
    {
        OutputDebugStringA("Texture upload: Signal failed.\n");
        std::terminate();
    }

    // nullptr 이벤트: fence 값에 도달할 때까지 동기 대기
    if (FAILED(fence->SetEventOnCompletion(1, nullptr)))
    {
        OutputDebugStringA("Texture upload: fence wait failed.\n");
        std::terminate();
    }

    if (FAILED(m_Device->GetDeviceRemovedReason()))
        return EResult::Fail;

    dxTexture->SetCurrentState(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // GPU 완료 후 로컬 ComPtr들이 업로드 자원을 해제합니다.
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

	vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles(count);
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
        rtvHandles.data(),
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
    if(!m_CommandList || !pipeline)
        return EResult::Fail;

	ID3D12PipelineState* pipelineState = static_cast<ID3D12PipelineState*>(pipeline->GetNativeHandle());
    m_CommandList->SetPipelineState(pipelineState);

	D3D_PRIMITIVE_TOPOLOGY dxTopo = ToD3D12PrimitiveTopology(pipeline->GetTopology());
    m_CommandList->IASetPrimitiveTopology(dxTopo);

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

    vector<RHITexture*> renderTargets;
	RHITexture* depthTarget = nullptr;
    const uint32 requestedRenderTargetCount = renderPass->GetRenderTargetCount();

    if (requestedRenderTargetCount == 0 && renderPass->GetDepthStencilName().empty())
    {
        renderTargets.push_back(m_BackBuffer);
    }
    else
    {
		for (uint32 i = 0; i < requestedRenderTargetCount; ++i)
		{
			RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i));
			if (rt && rt->GetTexture()) renderTargets.push_back(rt->GetTexture());
		}
    }

    if (!renderPass->GetDepthStencilName().empty())
    {
		RenderTarget* ds = RenderTargetManager::Get().GetRenderTarget(renderPass->GetDepthStencilName());
        if(ds && ds->GetTexture()) depthTarget = ds->GetTexture();
    }

	vector<D3D12_RESOURCE_BARRIER> barriers;
    for (RHITexture* tex : renderTargets)
    {
        DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(tex);
        if (dxTex->GetCurrentState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = static_cast<ID3D12Resource*>(dxTex->GetNativeHandle());
            barrier.Transition.StateBefore = dxTex->GetCurrentState();
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers.push_back(barrier);
            dxTex->SetCurrentState(D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }

    if (depthTarget)
    {
        DirectX12Texture* dxDepth = static_cast<DirectX12Texture*>(depthTarget);
        if (dxDepth->GetCurrentState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = static_cast<ID3D12Resource*>(dxDepth->GetNativeHandle());
            barrier.Transition.StateBefore = dxDepth->GetCurrentState();
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers.push_back(barrier);
            dxDepth->SetCurrentState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }

	if (!barriers.empty())
	{
		m_CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
	}

	BindRenderTargets(static_cast<uint32>(renderTargets.size()), renderTargets.data(), depthTarget);

    if (renderPass->GetLoadOperation() == ERenderPassLoadOperation::RPLO_Clear)
    {
		for (uint32 i = 0; i < renderTargets.size(); ++i)
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

    vector<D3D12_RESOURCE_BARRIER> barriers;

    for (uint32 i = 0; i < m_CurrentRenderPass->GetRenderTargetCount(); ++i)
    {
        const wstring& name = m_CurrentRenderPass->GetRenderTargetName(i);
        RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(name);
        if (!rt || !rt->GetTexture()) return EResult::Fail;

        auto* texture = static_cast<DirectX12Texture*>(rt->GetTexture());
        if (!HasFlag(texture->GetUsage(), ETextureUsage::Sampler)) continue;

        const D3D12_RESOURCE_STATES before = texture->GetCurrentState();
        if ((before & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != 0) continue;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers.push_back(barrier);

        texture->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    if (!barriers.empty())
        m_CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

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

	f32 clearColor[4] = { color.x, color.y, color.z, color.w };
	m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	return EResult::Success;
}

EResult DirectX12RHI::ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil)
{
    if (!m_CommandList || !depthStencil) return EResult::Fail;
    DirectX12Texture* dxTex = static_cast<DirectX12Texture*>(depthStencil);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxTex->GetDSVHandle();
    m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
    return EResult::Success;
}
#pragma endregion

#pragma region Draw Call
EResult DirectX12RHI::ApplyShaderResources()
{
    if (!m_CommandList) return EResult::Fail;

    static constexpr uint32 rootParameterMap[MAX_CONSTANT_BUFFER_SLOTS] = { 0, 3, 5, 6 };

    for (uint32 slot = 0; slot < MAX_CONSTANT_BUFFER_SLOTS; ++slot)
    {
        const ConstantBufferBinding& binding = m_ConstantBuffers[slot];
        if (!binding.buffer) continue;

        auto* resource = static_cast<ID3D12Resource*>(binding.buffer->GetNativeHandle());
        if (!resource) return EResult::Fail;

        D3D12_GPU_VIRTUAL_ADDRESS address = resource->GetGPUVirtualAddress();
        if (binding.isDynamic) address += binding.offset;

        m_CommandList->SetGraphicsRootConstantBufferView(rootParameterMap[slot], address);
    }

    if (!m_CurrentRenderPass) return EResult::Fail;

    if (m_DynamicHeapCursor > FRAME_SRV_DESCRIPTOR_COUNT - SRV_TABLE_SIZE)
    {
        ENGINE_LOG_ERROR("Frame SRV table capacity exceeded.");
        return EResult::Fail;
    }

    ID3D12Resource* resources[SRV_TABLE_SIZE] = {};
    DirectX12Texture* samplingTextures[SRV_TABLE_SIZE] = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC views[SRV_TABLE_SIZE] = {};

    const bool isLighting = m_CurrentRenderPass->GetPassType() == ERenderPassType::Lighting;

    for (uint32 slot = 0; slot < SRV_TABLE_SIZE; ++slot)
    {
        auto& view = views[slot];
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

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

        // 사용하지 않는 슬롯도 유효한 null SRV로 채웁니다.
        view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;

        RHITexture* source = m_CurrentTextures[slot];
        if (!source) continue;
        if (!HasFlag(source->GetUsage(), ETextureUsage::Sampler)) return EResult::Fail;

        auto* texture = static_cast<DirectX12Texture*>(source);
        auto* resource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        if (!resource) return EResult::Fail;

        const auto resourceDesc = resource->GetDesc();
        if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            resourceDesc.DepthOrArraySize != 1 || resourceDesc.SampleDesc.Count != 1)
        {
            ENGINE_LOG_ERROR("SRV binding currently supports single-sample Texture2D inputs.");
            return EResult::NotImplemented;
        }

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

        const DXGI_FORMAT format = ToDXGIFormat(texture->GetFormat());
        view.Format = IsDepthFormat(format) ? ToDepthSRVFormat(format) : resourceDesc.Format;
        view.Texture2D.MipLevels = resourceDesc.MipLevels;

        resources[slot] = resource;
        samplingTextures[slot] = texture;
    }

    // 입력 검사가 끝난 뒤 필요한 상태 전환을 기록합니다.
    for (auto* texture : samplingTextures)
    {
        if (!texture) continue;

        const D3D12_RESOURCE_STATES before = texture->GetCurrentState();
        if ((before & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != 0) continue;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = static_cast<ID3D12Resource*>(texture->GetNativeHandle());
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        m_CommandList->ResourceBarrier(1, &barrier);
        texture->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    const uint32 tableIndex = m_SRVTableStartIndices[m_CurrentBackBufferIndex] + m_DynamicHeapCursor;
    const uint32 descriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE destination = m_SrvAllocator->GetHeap()->GetCPUDescriptorHandleForHeapStart();
    destination.ptr += static_cast<SIZE_T>(tableIndex) * descriptorSize;

    for (uint32 slot = 0; slot < SRV_TABLE_SIZE; ++slot)
    {
        m_Device->CreateShaderResourceView(resources[slot], &views[slot], destination);
        destination.ptr += descriptorSize;
    }

    m_CommandList->SetGraphicsRootDescriptorTable(1, m_SrvAllocator->GetGPUHandle(tableIndex));
    m_CommandList->SetGraphicsRootDescriptorTable(
        7, m_SrvAllocator->GetGPUHandle(tableIndex + MAX_MATERIAL_TEXTURE_SLOTS));

    m_DynamicHeapCursor += SRV_TABLE_SIZE;

    return EResult::Success;
}
EResult DirectX12RHI::Draw(uint32 count)
{
    if(!m_CommandList) return EResult::Fail;

	if (m_VertexBuffers[0])
	{
		DirectX12Buffer* dxBuffer = static_cast<DirectX12Buffer*>(m_VertexBuffers[0]);
		D3D12_VERTEX_BUFFER_VIEW vbView = dxBuffer->GetVertexBufferView();
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
        if (!m_VertexBuffers[slot])
            continue;

        DirectX12Buffer* buffer =
            static_cast<DirectX12Buffer*>(m_VertexBuffers[slot]);

        views[slot] = buffer->GetVertexBufferView();
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

    // TODO: SRV(텍스처) 다이내믹 힙 복사 및 바인딩 로직 (추후 구현)

    m_CommandList->DrawIndexedInstanced(count, 1, 0, 0, 0);
    RecordIndexedDraw(count);
    return EResult::Success;
}

EResult DirectX12RHI::DrawIndexedInstanced()
{
    return EResult::Fail;
}

EResult DirectX12RHI::DrawTexture(RHITexture* texture)
{
    return EResult::Fail;
}
#pragma endregion

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