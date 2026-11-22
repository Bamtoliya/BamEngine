#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "ModelImporter.h"
#include "PreviewViewportWidget.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include <atomic>
#include <filesystem>
#include <memory>
#include <vector>

BEGIN(Engine)
class Entity;
class MaterialInterface;
END

BEGIN(Editor)
struct PreparedModelData;
class ModelImportTask;

class ModelImportModal final : public IImGui
{
public:
	ModelImportModal();
	virtual ~ModelImportModal() override;

	static ModelImportModal* Get() { return s_Instance; }

	virtual void Free() override;
	virtual void Update(f32 dt) override;
	virtual void Draw() override;

	void Open(const std::filesystem::path& sourcePath, const std::filesystem::path& destDir = {});

	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Hidden; }
	// 닫힌 뒤에도 진행 중인 프리뷰 파싱이 끝나면 결과를 해제하기 위해 갱신을 유지한다
	bool ShouldUpdateWhenClosed() const override { return m_AsyncPreview != nullptr; }

private:
	struct AsyncPreviewState
	{
		std::unique_ptr<ModelImportTask> Task;
		std::atomic<EResult> Result{ Engine::EResult::Fail };
		std::atomic<bool> Completed{ false };
		// 모달 닫기·설정 변경으로 결과가 필요 없어졌음을 작업 스레드에 알린다
		std::atomic<bool> Cancelled{ false };
		bool FocusCamera = true;
	};

	void CancelAsyncPreview();

	struct AlbedoPreviewInfo
	{
		std::string MaterialName;
		std::string Reference;
		std::string ResolvedPath;
		const char* StatusKey = "ModelImport.TextureNotProcessed";
		bool Embedded = false;
	};

private:
	EResult OnInitialize(void* arg) override;
	void StartAsyncPreviewPrepare(bool focusCamera);
	bool BeginPreviewBuild(std::unique_ptr<ModelImportTask> completedTask, bool focusCamera);
	void StepPreviewBuild();
	bool BuildSinglePreviewMaterial(const PreparedModelData& data, size_t materialIndex);
	bool UpdatePreviewTransform();
	void ClearPreview();

	void DrawPreviewPane();
	void DrawTransformOptions();
	void DrawTextureReferences();
	void DrawAlbedoThumbnail(size_t materialIndex, float maxEdge);
	void ApplyDccPreset(EModelImportDccPreset preset);
	void LoadSavedImportOptions();

private:
	inline static ModelImportModal* s_Instance = nullptr;

	bool m_OpenRequested = false;
	bool m_PreviewReloadRequested = false;
	bool m_PreviewReady = false;
	bool m_PreviewBuilding = false;
	bool m_PendingFocusCamera = true;
	bool m_CloseRequested = false;
	bool m_ImportRequested = false;
	std::filesystem::path m_SourcePath;
	std::filesystem::path m_DestDir;
	ModelImportDesc m_Desc;

	std::thread m_PreviewThread;
	std::shared_ptr<AsyncPreviewState> m_AsyncPreview;
	std::unique_ptr<ModelImportTask> m_BuildingTask;
	Engine::ModelCreateDesc m_BuildingModelDesc;
	Engine::ResourceHandle<Engine::Material> m_BuildingBaseMaterial;
	size_t m_NextBuildMeshIndex = 0;
	size_t m_NextBuildMaterialIndex = 0;
	float m_PreviewProgress = 0.0f;

	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;
	std::vector<Engine::ResourceHandle<Engine::Mesh>> m_TempMeshes;
	std::vector<Engine::ResourceHandle<Engine::MaterialInterface>> m_TempMaterials;
	std::vector<Engine::ResourceHandle<Engine::Texture>> m_TempTextures;
	std::vector<AlbedoPreviewInfo> m_AlbedoPreviewInfo;

	float m_SplitterWidth = 380.0f;
};

END