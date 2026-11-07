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
class MaterialInterface;
struct ModelCreateDesc;
END

BEGIN(Editor)
struct PreparedModelData;
class ModelImportModal final : public IImGui
{
public:
	ModelImportModal();
	virtual ~ModelImportModal() override = default;

	static ModelImportModal* Get() { return s_Instance; }

	virtual void Free() override;
	virtual void Update(f32 dt) override;
	virtual void Draw() override;

	void Open(const std::filesystem::path& sourcePath, const std::filesystem::path& destDir = {});

	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Hidden; }
private:
	EResult OnInitialize(void* arg) override;
	void LoadPreviewModel(bool focusCamera);
	bool UpdatePreviewTransform();
	void ClearPreview();
	bool CreatePreviewMaterials(const PreparedModelData& data, Engine::ModelCreateDesc& desc, const Engine::ResourceHandle<Engine::Material>& baseMaterial);

private:
	struct AlbedoPreviewInfo
	{
		std::string MaterialName;
		std::string Reference;
		std::string ResolvedPath;
		const char* StatusKey = "ModelImport.TextureNotProcessed";
		bool Embedded = false;
	};

	void DrawTransformOptions();
	void DrawTextureReferences();
	void DrawAlbedoThumbnail(size_t materialIndex, float maxEdge);

private:
	inline static ModelImportModal* s_Instance = nullptr;

	bool m_OpenRequested = false;
	bool m_PreviewReloadRequested = false;
	bool m_PreviewReady = false;
	bool m_CloseRequested = false;
	bool m_ImportRequested = false;
	std::filesystem::path m_SourcePath;
	std::filesystem::path m_DestDir;
	ModelImportDesc m_Desc;

	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;
	std::vector<Engine::ResourceHandle<Engine::Mesh>> m_TempMeshes;
	std::vector<Engine::ResourceHandle<Engine::MaterialInterface>> m_TempMaterials;
	std::vector<Engine::ResourceHandle<Engine::Texture>> m_TempTextures;
	std::vector<AlbedoPreviewInfo> m_AlbedoPreviewInfo;

	float m_SplitterWidth = 380.0f;
};

END
