#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceDocumentHistory.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include "Sprite.h"
#include "SpriteAnimation.h"
#include "SpriteSlicingService.h"
#include <optional>
#include <vector>
#include <string>

BEGIN(Editor)

enum class EAutoAnimGenerationMode : uint8
{
	SingleClip,
	RowByRowClips
};

struct SpriteAutoAnimParameters
{
	EAutoAnimGenerationMode mode = EAutoAnimGenerationMode::SingleClip;
	std::string singleClipName = "Animation";
	f32 fps = 12.0f;
	Engine::ESpriteAnimationPlayMode playMode = Engine::ESpriteAnimationPlayMode::Loop;
	std::string rowNames = "Idle, Walk, Attack, Die";
};

class SpriteEditor final : public ResourceEditorInterface
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override;
	std::vector<std::filesystem::path> GetRetainedResourcePaths() const override;
	SpriteEditor();
	virtual ~SpriteEditor() = default;

public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
	virtual bool HasUnsavedResourceChanges() const override { return m_History.IsDirty(); }
	void SaveResource() override { ApplySlices(); }
	bool CanSaveDocument() const;
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::SpriteEditor; }
	void OpenInSpriteMode(const std::filesystem::path& path);

private:
	void DrawCustomToolbar() override;
	void ReleaseResourceDocument() override;
	const char* GetSaveActionId() const override { return "SpriteEditor.Save"; }
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(900.0f, 650.0f); }
	void DrawCanvas(float availWidth, float availHeight);
	void DrawSidebar(float availWidth, float availHeight);
	void DrawSliceModal();
	void DrawCreateAnimationModal();

	void ExecuteSlice(const SpriteSliceParameters& params);
	void ApplySlices();
	std::optional<std::vector<ResourceHandle<Engine::Sprite>>> SaveSlices();
	bool GenerateAnimation(const SpriteAutoAnimParameters& params);
	void OpenTextureDialog(); // 파일 대화상자로 텍스처를 골라 현재 문서를 교체한다. 확인은 호출부가 RunWithDiscardConfirmation으로 처리한다.
	void FitView(float canvasW, float canvasH, float texW, float texH);

private:
	ResourceHandle<Engine::Texture> m_TextureHandle;
	std::vector<SpriteSliceInfo> m_Slices;
	int m_SelectedSliceIndex = -1;

	// 캔버스 뷰 상태
	float m_Zoom = 1.0f;
	ImVec2 m_PanOffset = { 0.0f, 0.0f };
	float m_SidebarWidth = 280.0f;

	// 변경 사항 플래그
	ResourceDocumentHistory<std::vector<SpriteSliceInfo>> m_History;
	bool CommitDocumentEdit();
	bool ApplyDocumentSnapshot(const std::vector<SpriteSliceInfo>& snapshot);
	bool CanUndoDocument() const override;
	bool CanRedoDocument() const override;
	void UndoDocument() override;
	void RedoDocument() override;

	// 모달 팝업 상태
	bool m_ShowSliceModal = false;
	bool m_ShowCreateAnimModal = false;
	SpriteSliceParameters m_SliceParams;
	SpriteAutoAnimParameters m_AnimParams;
	char m_SliceSearch[64] = {};
};

END
