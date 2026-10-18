#pragma once

#include "Editor_Includes.h"
#include "SceneEditHistory.h"

BEGIN(Editor)

struct ApplicationCreateInfo
{
	// 필요한 초기화 정보를 여기에 추가
	uint32 width = 1280;
	uint32 height = 720;
	const char* AppName = "BamEngine Editor";
};

class Application final : public Base
{
	DECLARE_SINGLETON(Application)
#pragma region Constructor&Destructor
private:
	Application() {}
	virtual ~Application() = default;
	EResult Initialize(void* arg);
public:
	void Free() override;
#pragma endregion

#pragma region Initialize
private:
	EResult InitializeWindow(const ApplicationCreateInfo& createInfo);
	EResult InitializeRuntime(const ApplicationCreateInfo& createInfo);
	EResult InitializeSystem();
	EResult InitializeImGui();
	EResult InitializeQt(const ApplicationCreateInfo& createInfo);
	EResult InitializeLogger();
#pragma endregion

#pragma region Localization
private:
	void InitializeLocalization();
#pragma endregion

#pragma region Test Prepare
private:
	void InitializeResources();
	void InitializeFiles();
	void InitializeShaders();
	void InitializeMeshes();
	void InitializeMaterials();
#pragma endregion

#pragma region Rendering
private:
	void IntializeRenderer();
	void SubmitRenderPasses();
#pragma endregion

#pragma region Command History
public:
	CommandHistory& GetCommandHistory() { return m_EditHistory.Commands(); }
	const CommandHistory& GetCommandHistory() const { return m_EditHistory.Commands(); }
	SceneEditHistory& GetEditHistory() { return m_EditHistory; }
#pragma endregion


public:
	void Run(int argc, char* argv[]);
	void Shutdown();
public:
	void SetResolution(uint32 width, uint32 height, bool fullscreen);
	void GetWindowSize(int32* w, int32* h) const;
private:
	void UpdateTitle(f32 dt);
private:
	class Runtime* m_Runtime = { nullptr };
	class ImGuiManager* m_ImGuiManager = { nullptr };
	class SelectionManager* m_SelectionManager = { nullptr };
	class PIEManager* m_PIEManager = { nullptr };
	struct SDL_Window* m_Window = { nullptr };
	class RHI* m_RHI = { nullptr };
private:
	class AssetManager* m_AssetManager = { nullptr };
private:
	f32 m_FPSTimer = { 0.f };

private:
	RenderPassID m_LightingPassID		= INVALID_PASS_ID;
	RenderPassID m_GemetryPassID		= INVALID_PASS_ID;
	RenderPassID m_FowardPassID			= INVALID_PASS_ID;
	RenderPassID m_PostProcessPassID	= INVALID_PASS_ID;
private:
	SceneEditHistory m_EditHistory;
	std::vector<std::pair<Engine::Scene*, uint64>> m_HistorySelection;
private:
	bool m_SDLInitialized = false;
	bool m_LoggerInitialized = false;
};

END
