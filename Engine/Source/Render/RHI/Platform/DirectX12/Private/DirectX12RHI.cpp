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
    m_SrvAllocator = new DirectX12DescriptorAllocator(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);
	m_SamplerAllocator = new DirectX12DescriptorAllocator(m_Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128);

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

	m_CommandList->Close(); // 초기 상태에서는 Command List를 닫아둡니다.

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

    for (uint32 i = 0; i < m_SwapChainBufferCount; i++)
    {
        ComPtr<ID3D12Resource> backBuffer;
        if (FAILED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer))))
            return EResult::Fail;
        // RTV 서술자 생성


		DirectX12TextureDesc textureDesc = {};
		textureDesc.width = m_SwapChainWidth; 
		textureDesc.height = m_SwapChainHeight;
		textureDesc.nativeHandle = backBuffer.Get();
		textureDesc.initialState = D3D12_RESOURCE_STATE_PRESENT;

		uint32 rtvIndex = 0;
		m_RtvAllocator->Allocate(textureDesc.rtvHandle, rtvIndex);
        m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, textureDesc.rtvHandle);

        // 💡 래퍼 클래스를 씌워서 부모 배열에 영구 보관 (수명 안전성 확보)
        m_SwapChainBuffers[i] = DirectX12Texture::Create(this, textureDesc, false);
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

    return EResult::Success;
}

DirectX12RHI* DirectX12RHI::Create(void* arg)
{
	DirectX12RHI* instance = new DirectX12RHI();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		instance = nullptr;
		fmt::print(stderr, "DirectX12RHI Creation Failed\n");
	}
    return instance;
}

void DirectX12RHI::Free()
{
    for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
    {
        if (m_SwapChainBuffers[i])
        {
			Safe_Release(m_SwapChainBuffers[i]);
			m_SwapChainBuffers[i] = nullptr;
        }
    }

    delete m_RtvAllocator; m_RtvAllocator = nullptr;
    delete m_DsvAllocator; m_DsvAllocator = nullptr;
    delete m_SrvAllocator; m_SrvAllocator = nullptr;
	delete m_SamplerAllocator; m_SamplerAllocator = nullptr;

	if (m_FenceEvent)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}

    __super::Free();
}
#pragma endregion

EResult DirectX12RHI::Resize(uint32 width, uint32 height)
{
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
    RHITexture* texture = DirectX12Texture::Create(this, dxDesc);
    if (texture == nullptr) return nullptr;

    return texture;
}

RHITexture* DirectX12RHI::CreateTextureFromNativeHandle(void* nativeHandle)
{
    return nullptr;
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
	if (IsFailure(BindTexture(texture, slot)))
		return EResult::Fail;
	if (IsFailure(BindSampler(sampler)))
		return EResult::Fail;
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

    return EResult::Success;
}
EResult DirectX12RHI::EndRenderPass()
{
    if (!m_CommandList) return EResult::Fail;

	//DirectX12Texture* dxBackBuffer = static_cast<DirectX12Texture*>(m_BackBuffer);
 //   if(dxBackBuffer && dxBackBuffer->GetCurrentState() == D3D12_RESOURCE_STATE_RENDER_TARGET)
 //   {
 //       D3D12_RESOURCE_BARRIER barrier = {};
 //       barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 //       barrier.Transition.pResource = static_cast<ID3D12Resource*>(dxBackBuffer->GetNativeHandle());
 //       barrier.Transition.StateBefore = dxBackBuffer->GetCurrentState();
 //       barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
 //       barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 //       m_CommandList->ResourceBarrier(1, &barrier);
 //       dxBackBuffer->SetCurrentState(D3D12_RESOURCE_STATE_PRESENT);
 //   }

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

    m_CommandList->DrawInstanced(count, 1, 0, 0);
    return EResult();
}

EResult DirectX12RHI::DrawIndexed(uint32 count)
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

    // CBV 슬롯 → 루트 파라미터 인덱스 매핑
    // slot 0 = Global CBV  → Root Param 0
    // slot 1 = Object CBV  → Root Param 3
    // slot 2 = Pass CBV #0 → Root Param 5
    // slot 3 = Pass CBV #1 → Root Param 6
    static constexpr uint32 CBV_ROOT_PARAM_MAP[MAX_CONSTANT_BUFFER_SLOTS] = { 0, 3, 5, 6 };

    for (uint32 slot = 0; slot < MAX_CONSTANT_BUFFER_SLOTS; ++slot)
    {
        ConstantBufferBinding& cache = m_ConstantBuffers[slot];
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;

        if (cache.isDynamic && cache.buffer)
        {
            // 임시 데이터: 링 버퍼의 GPU 기본 주소 + 오프셋
            gpuAddr = static_cast<ID3D12Resource*>(cache.buffer->GetNativeHandle())->GetGPUVirtualAddress();
            gpuAddr += cache.offset;
        }
        else if (!cache.isDynamic && cache.buffer)
        {
            // 영구 객체: 버퍼의 GPU 주소를 직접 가져옴
            gpuAddr = static_cast<ID3D12Resource*>(cache.buffer->GetNativeHandle())->GetGPUVirtualAddress();
        }

        if (gpuAddr != 0)
        {
            m_CommandList->SetGraphicsRootConstantBufferView(CBV_ROOT_PARAM_MAP[slot], gpuAddr);
        }
    }

    // TODO: SRV(텍스처) 다이내믹 힙 복사 및 바인딩 로직 (추후 구현)

    m_CommandList->DrawIndexedInstanced(count, 1, 0, 0, 0);
    return EResult::Success;
}

EResult DirectX12RHI::DrawIndexedInstanced()
{
    return EResult();
}

EResult DirectX12RHI::DrawTexture(RHITexture* texture)
{
    return EResult();
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