#pragma once

#include "ImGuiInterface.h"
#include "ToolBar.h"
#include <SDL3/SDL.h>

BEGIN(Editor)

struct tagImGuiManagerDesc
{
	SDL_Window* Window = { nullptr };
	class RHI* RHI = { nullptr };
};

class ImGuiManager final : public Base
{
	DECLARE_SINGLETON(ImGuiManager)
	using DESC = tagImGuiManagerDesc;
#pragma region Constructor&Destructor
private:
	ImGuiManager() {}
	virtual ~ImGuiManager() = default;
	EResult Initialize(void* arg = nullptr);
private:
	EResult InitializeImGui();
public:
	virtual void Free()override;
#pragma endregion

#pragma region Update
public:
	void Update(f32 dt);
#pragma endregion


public:
	void Begin();
	void End();
	void ProcessEvent(const SDL_Event* event);
	void Draw();


#pragma region ImGui Management
public:
	EResult AddImGuiPanel(ImGuiInterface* panel);
	EResult RemoveImGuiPanel(ImGuiInterface* panel);
public:
	ImGuiInterface* GetImGuiPanel(const wstring& name);
	vector<ImGuiInterface*> GetImGuiPanels(const wstring& name);
	const vector<ImGuiInterface*>& GetImGuiPanels() const { return m_ImGuiPanels; }
private:
	EResult CreateDefaultPanels();
private:
	EResult CreateResourceEditors();
#pragma endregion


#pragma region SDLGPU3
private:
	EResult InitializeImGuiSDLGPU3();
	void SDLGPU3Begin();
	void SDLGPU3End();
	void ShutdownImGuiSDLGPU3();
#pragma endregion

#pragma region DirectX12
private:
	EResult InitializeImGuiDirectX12();
	void DirectX12Begin();
	void DirectX12End();
	void ShutdownImGuiDirectX12();
#pragma endregion

#pragma region Vulkan
private:
	EResult InitializeImGuiVulkan();
	void VulkanBegin();
	void VulkanEnd();
	void ShutdownImGuiVulkan();
#pragma endregion

#pragma region Metal
private:
	EResult InitializeImGuiMetal();
	void MetalBegin();
	void MetalEnd();
	void ShutdownImGuiMetal();
#pragma endregion

#pragma region DrawUI
private:
	void MainDockspace();
#pragma endregion

#pragma region Style
private:
	void SetCustomStyle();
#pragma endregion

#pragma region Variables
private:
	SDL_Window* m_Window = { nullptr };
	class RHI* m_RHI = { nullptr };
	ERHIType m_RHIType = ERHIType::Unknown;
#pragma region ImGui
private:
	ToolBar m_ToolBar;
	vector<ImGuiInterface*> m_ImGuiPanels;
	vector<ImGuiInterface*> m_PendingAddPanels;
	vector<ImGuiInterface*> m_PendingRemovePanels;
#pragma endregion
#pragma endregion



};

END