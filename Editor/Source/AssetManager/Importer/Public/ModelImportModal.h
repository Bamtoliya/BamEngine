#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "ModelImporter.h"
#include "PreviewViewportWidget.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "Material.h"
#include <filesystem>
#include <vector>

BEGIN(Engine)
class Entity;
class Model;
END

BEGIN(Editor)

class ModelImportModal final : public IImGui
{
public:
	ModelImportModal();
	virtual ~ModelImportModal() override = default;

	static ModelImportModal* Get() { return s_Instance; }

	virtual EResult Initialize(void* arg = nullptr);
	virtual void Free() override;
	virtual void Update(f32 dt) override;
	virtual void Draw() override;

	void Open(const std::filesystem::path& sourcePath, const std::filesystem::path& destDir = {});

	virtual void CaptureState(EditorWorkspace& workspace) const override {}
	virtual void RestoreState(const EditorWorkspace& workspace) override {}

private:
	void LoadPreviewModel();
	void UpdatePreviewTransform();
	void ClearPreview();

private:
	inline static ModelImportModal* s_Instance = nullptr;

	bool m_OpenRequested = false;
	std::filesystem::path m_SourcePath;
	std::filesystem::path m_DestDir;
	ModelImportDesc m_Desc;

	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;
	std::vector<Engine::ResourceHandle<Engine::Mesh>> m_TempMeshes;

	float m_SplitterWidth = 380.0f;
};

END