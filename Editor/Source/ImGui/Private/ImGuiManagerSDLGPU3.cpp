#include "ImGuiManager.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_sdlrenderer3.h"

#pragma region SDLGPU3 Helper
EResult ImGuiManager::InitializeImGuiSDLGPU3()
{

	SDL_GPUDevice* NativeRenderer = reinterpret_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

	if (!ImGui_ImplSDL3_InitForSDLGPU(m_Window))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForSDLGPU Failed\n");
		return EResult::Fail;
	}

	m_PlatformBackendInitialized = true;

	SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(NativeRenderer, m_Window);

	ImGui_ImplSDLGPU3_InitInfo initInfo = {};
	initInfo.Device = NativeRenderer;
	initInfo.ColorTargetFormat = swapchainFormat;
	initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
	initInfo.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
	// [Multi-Viewport 처리] 멀티 뷰포트 모드일 때 스왑체인 구성 설정
	initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

	if (!ImGui_ImplSDLGPU3_Init(&initInfo))
	{
		fmt::print(stderr, "ImGui_ImplSDLGPU3_Init Failed\n");
		return EResult::Fail;
	}
	m_RendererBackendInitialized = true;

	return EResult::Success;
}
void ImGuiManager::SDLGPU3Begin()
{
	ImGui_ImplSDLGPU3_NewFrame();
}
void ImGuiManager::SDLGPU3End()
{
	if (!m_RHI) return;

	SDL_GPUDevice* device = reinterpret_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());
	SDL_GPUCommandBuffer* commandBuffer = static_cast<SDL_GPUCommandBuffer*>(m_RHI->GetCurrentCommandBuffer());
	ImDrawData* drawData = ImGui::GetDrawData();
	SDL_GPUTexture* backBufferTexture = static_cast<SDL_GPUTexture*>(m_RHI->GetBackBuffer()->GetNativeHandle());

	ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);

	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = backBufferTexture;
	colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;   // 이미 그려진 화면 유지
	colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);

	ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, pass, nullptr);
	SDL_EndGPURenderPass(pass);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}
void ImGuiManager::ShutdownImGuiSDLGPU3()
{
	SDL_GPUDevice* device = reinterpret_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());
	SDL_WaitForGPUIdle(device);
	ImGui_ImplSDLGPU3_Shutdown();
	// [Multi-Viewport 처리] 모든 추가 창들을 닫음
}
#pragma endregion