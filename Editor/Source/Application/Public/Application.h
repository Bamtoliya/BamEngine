#pragma once

#include "Editor_Includes.h"
#include "SceneEditHistory.h"
#include "Config.h"
#include "EditorSettings.h"
#include <reflection/Registry.h>
#include "EditorEditContext.h"
#include "EditorDocumentManager.h"
#include "TerrainDataFile.h"
#include <memory>

namespace archive { class JsonArchive; }

BEGIN(Editor)
class SceneDocument;
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
	using DESC = ApplicationCreateInfo;
#pragma region Constructor&Destructor
private:
	Application() {}
	virtual ~Application();
	EResult Initialize(void* arg);
public:
	void Free() override;
#pragma endregion

#pragma region Initialize
private:
	EResult InitializeWindow(const ApplicationCreateInfo& createInfo);
	EResult InitializeRuntime(const ApplicationCreateInfo& createInfo);
	EResult IntializeRenderer();
	EResult InitializeSystem();
	EResult InitializeImGui();
	//EResult InitializeQt(const ApplicationCreateInfo& createInfo);
	EResult InitializeLogger();
	EResult InitializeLocalization();
#pragma endregion

#pragma region Resource
private:
	bool InitializeResources();
	void InitializeMeshes();
#pragma endregion

#pragma region Settings
public:
	[[nodiscard]] EditorApplicationState CaptureApplicationState() const;

	void GetWindowSize(int32* w, int32* h) const { if (m_Window) SDL_GetWindowSize(m_Window, w, h); }
	bool IsFullscreen() const { return (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) != 0; }

	void RememberEditedScene(const std::filesystem::path& path);
	// 프로젝트 허브를 취소했을 때 기본 프로젝트의 마지막 씬을 불러온다.
	void RestoreLastEditedScene();
	void CaptureAndSaveEditorWorkspace();

private:
	EResult SubmitWindowRequest(const EditorWindowRequest& request);
	void CaptureEditorWindowState();
#pragma endregion

#pragma region Command History
public:
	CommandHistory& GetCommandHistory() { return m_EditContext.History().Commands(); }
	const CommandHistory& GetCommandHistory() const { return m_EditContext.History().Commands(); }
	SceneEditHistory& GetEditHistory() { return m_EditContext.History(); }
#pragma endregion

#pragma region Documents
public:
	EditorDocumentManager& GetDocuments() { return m_Documents; }
	EditorDocumentId GetSceneDocumentId() const { return m_SceneDocumentId; }
	SceneDocument* GetSceneDocument();

	void AdoptSceneDocument(const std::filesystem::path& path, bool rememberScene = true);
	void RebindSceneDocumentAfterPlay();
	void CompleteQuitRequest() { m_QuitApproved = true; }
#pragma endregion

#pragma region Module Hot Reload
private:
	void ReleaseSceneForModuleUnload();
	void RestoreSceneAfterModuleReload();
#pragma endregion

#pragma region Loop
public:
	void Run(int argc, char* argv[]);
private:
	void UpdateTitle(f32 dt);
#pragma endregion

#pragma region Member Variables
private:	
	struct SDL_Window* m_Window = { nullptr };
private: //Caching pointers to managers for easy access
	class Runtime* m_Runtime = { nullptr };
	class SelectionManager* m_SelectionManager = { nullptr };
	class PIEManager* m_PIEManager = { nullptr };
	class ImGuiManager* m_ImGuiManager = { nullptr };
	class HotReloadManager* m_HotReloadManager = { nullptr };
private:
	EditorEditContext m_EditContext;
	EditorDocumentManager m_Documents;
	EditorDocumentId m_SceneDocumentId = InvalidEditorDocumentId;
	bool m_QuitApproved = false;
private:
	// 게임 모듈 핫 리로드 동안 보관하는 편집 씬(저장하지 않은 편집 포함). 새 모듈 로드 후 복원하고 비운다.
	std::unique_ptr<archive::JsonArchive> m_ModuleReloadScene;
	Engine::TerrainDataFile::MemorySnapshot m_ModuleReloadTerrain;
	std::filesystem::path m_ModuleReloadScenePath;
	std::filesystem::path m_ModuleReloadProjectRoot;
	bool m_ModuleReloadSceneModified = false;
private:
	bool m_SDLInitialized = false;
	bool m_LoggerInitialized = false;
private:
	bool m_InitializationCompleted = false;
	bool m_ExitBeforeLaunch = false;
#pragma endregion
};

END
