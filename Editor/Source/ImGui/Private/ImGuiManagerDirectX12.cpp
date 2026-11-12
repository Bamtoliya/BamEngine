#include "ImGuiManager.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_sdl3.h"
#include "DirectX12RHI.h"
#include "DirectX12Texture.h"

#pragma region DirectX12 Helper
EResult ImGuiManager::InitializeImGuiDirectX12()
{
	if (!m_RHI) return EResult::Fail;

	DirectX12RHI* dx12RHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* device = static_cast<ID3D12Device*>(dx12RHI->GetNativeRHI());

	if (!ImGui_ImplSDL3_InitForD3D(m_Window))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForD3D Failed\n");
		return EResult::Fail;
	}

	m_PlatformBackendInitialized = true;

	// [핵심] ImGui 전용 heap 생성 금지, 엔진 SRV heap 공유
	auto* srvAllocator = dx12RHI->GetSRVAllocator();
	if (!srvAllocator || !srvAllocator->GetHeap())
	{
		fmt::print(stderr, "DX12 SRV allocator/heap is null\n");
		return EResult::Fail;
	}

	m_ImGuiSrvDescHeap = srvAllocator->GetHeap();
	m_ImGuiOwnsSrvHeap = false;
	m_ImGuiSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	IM_ASSERT(m_ImGuiSrvDescriptorSize > 0);

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = device;
	initInfo.CommandQueue = dx12RHI->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(m_RHI->GetFrameResourceCount());;
	initInfo.SrvDescriptorHeap = m_ImGuiSrvDescHeap;
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.UserData = this;

	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
		{
			ImGuiManager* self = static_cast<ImGuiManager*>(info->UserData);
			IM_ASSERT(self && self->m_RHI);

			auto* dx12RHI = static_cast<DirectX12RHI*>(self->m_RHI);
			auto* allocator = dx12RHI->GetSRVAllocator();
			IM_ASSERT(allocator);

			uint32 index = 0;
			if (!allocator->Allocate(*out_cpu, index))
			{
				IM_ASSERT(false && "SRV allocator is out of descriptors.");
				*out_cpu = {};
				*out_gpu = {};
				return;
			}

			*out_gpu = allocator->GetGPUHandle(index);
		};

	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			ImGuiManager* self = static_cast<ImGuiManager*>(info->UserData);
			IM_ASSERT(self && self->m_RHI);

			auto* dx12RHI = static_cast<DirectX12RHI*>(self->m_RHI);
			auto* allocator = dx12RHI->GetSRVAllocator();
			IM_ASSERT(allocator);

			auto* heap = allocator->GetHeap();
			D3D12_CPU_DESCRIPTOR_HANDLE start = heap->GetCPUDescriptorHandleForHeapStart();

			if (cpu_desc.ptr < start.ptr || self->m_ImGuiSrvDescriptorSize == 0)
				return;

			SIZE_T delta = cpu_desc.ptr - start.ptr;
			uint32 index = static_cast<uint32>(delta / self->m_ImGuiSrvDescriptorSize);
			dx12RHI->DeferDescriptorFree(allocator, index);
		};

	if (!ImGui_ImplDX12_Init(&initInfo))
	{
		fmt::print(stderr, "ImGui_ImplDX12_Init Failed\n");
		return EResult::Fail;
	}

	m_RendererBackendInitialized = true;

	return EResult::Success;
}
void ImGuiManager::DirectX12Begin()
{
	ImGui_ImplDX12_NewFrame();
}
void ImGuiManager::DirectX12End()
{
	DirectX12RHI* dx12RHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12GraphicsCommandList* commandList = dx12RHI->GetCommandList();

	DirectX12Texture* backBuffer = static_cast<DirectX12Texture*>(m_RHI->GetBackBuffer());
	if (backBuffer && backBuffer->GetCurrentState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = static_cast<ID3D12Resource*>(backBuffer->GetNativeHandle());
		barrier.Transition.StateBefore = backBuffer->GetCurrentState();
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		backBuffer->SetCurrentState(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = backBuffer->GetRTVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// ImGui용 Descriptor Heap 바인딩
	ID3D12DescriptorHeap* heaps[] = { m_ImGuiSrvDescHeap };
	commandList->SetDescriptorHeaps(1, heaps);
	// ImGui 그리기 명령을 커맨드 리스트에 기록
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	// 다중 뷰포트(도킹 창 분리) 처리를 위한 업데이트 
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, (void*)commandList);
	}
}
void ImGuiManager::ShutdownImGuiDirectX12()
{
	auto* dxRHI = static_cast<Engine::DirectX12RHI*>(m_RHI);
	if (!dxRHI || Engine::IsFailure(dxRHI->WaitForGPU()))
	{
		fmt::print(stderr, "GPU synchronization failed before ImGui DX12 shutdown.\n");
		std::terminate();
	}

	ImGui_ImplDX12_Shutdown();
}
#pragma endregion