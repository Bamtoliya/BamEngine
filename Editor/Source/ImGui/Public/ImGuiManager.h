#pragma once

#include "ImGuiInterface.h"
#include "ToolBar.h"
#include <SDL3/SDL.h>
#include "EditorSettings.h"
#include "EditorDocumentController.h"

BEGIN(Editor)
struct EditorSettingsChangeResult;
struct ImGuiManagerDesc
{
	SDL_Window* Window = { nullptr };
	class RHI* RHI = { nullptr };
};

class ImGuiManager final : public Base
{
	DECLARE_SINGLETON(ImGuiManager)
	using DESC = ImGuiManagerDesc;
#pragma region Constructor&Destructor
private:
	ImGuiManager() {}
	virtual ~ImGuiManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free()override;
#pragma endregion

#pragma region Loop
public:
	void Update(f32 dt);
public:
	void Begin();
	void End();
	void ProcessEvent(const SDL_Event* event);
	void Draw();
#pragma endregion

#pragma region ImGui Management
private:
	EResult InitializeImGui();
public:
	EResult AddImGuiPanel(IImGui* panel);
	EResult RemoveImGuiPanel(IImGui* panel);
public:
	IImGui* GetImGuiPanel(const wstring& name);
	vector<IImGui*> GetImGuiPanels(const wstring& name);
	const vector<IImGui*>& GetImGuiPanels() const { return m_ImGuiPanels; }
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
public:
	void SetUIScale(float scale);
	[[nodiscard]] float GetUIScale() const { return m_UIScale; }
	void UpdateFontScale(float fontSize);
	void SetCustomStyle();
	void ApplyDefaultDarkStyle();
	void ApplyTheme(const std::string& theme);
	[[nodiscard]] bool SaveCustomStyle(const std::filesystem::path& path);
	[[nodiscard]] bool LoadCustomStyle(const std::filesystem::path& path);
	[[nodiscard]] std::filesystem::path GetCustomStylePath() const;
#pragma endregion

#pragma region Helper
public:
	ImTextureID GetImGuiTextureID(const class RHITexture* texture) const;
#pragma endregion

#pragma region Settings
public:
	void NotifySettingsChange(const EditorSettingsChangeResult& result);

#pragma endregion


#pragma region Workspace
public:
	void RestoreWorkspace(const EditorWorkspace& workspace);
	void CaptureWorkspace(EditorWorkspace& workspace);
#pragma endregion

#pragma region Document
public:
	bool RequestNewScene();
	bool RequestOpenScene(const std::filesystem::path& path);
	bool RequestCloseScene();
	bool RequestQuit();
	EditorDocumentController& GetDocumentController() { return *m_DocumentController; }
	bool PreparePendingSceneSave();
private:
	enum class ESceneDocumentAction
	{
		New,
		Open,
		Close
	};

	bool RequestSceneAction(ESceneDocumentAction action, std::filesystem::path path = {});
	void DrawDocumentConfirmation();
	void SynchronizeSceneDocumentAccess();
#pragma endregion


#pragma region Variables
private:
	SDL_Window* m_Window = { nullptr };
	class RHI* m_RHI = { nullptr };
	ERHIType m_RHIType = ERHIType::Unknown;
	ImGuiContext* m_ImGuiContext = nullptr;
	bool m_PlatformBackendInitialized = false;
	bool m_RendererBackendInitialized = false;
	std::unique_ptr<EditorDocumentController> m_DocumentController;
#pragma region DirectX12
	struct ID3D12DescriptorHeap* m_ImGuiSrvDescHeap = nullptr;
	uint32 m_ImGuiSrvDescriptorSize = 0;
	uint32 m_ImGuiSrvDescriptorCursor = 0;
	uint32 m_ImGuiSrvDescriptorCapacity = 1024;
	bool m_ImGuiOwnsSrvHeap = false; // 공유 heap이면 false
#pragma endregion

#pragma region ImGui
private:
	ToolBar m_ToolBar;
	vector<IImGui*> m_ImGuiPanels;
	vector<IImGui*> m_PendingAddPanels;
	vector<IImGui*> m_PendingRemovePanels;
	ImGuiStyle m_BaseStyle{};
	float m_UIScale = 1.0f;
	vector<IImGui*> m_WorkspacePanels;
#pragma endregion
#pragma endregion
};

END