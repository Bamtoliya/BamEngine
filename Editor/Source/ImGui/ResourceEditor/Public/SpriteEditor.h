#pragma once

#include "ResourceEditorInterface.h"
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
	SpriteEditor();
	virtual ~SpriteEditor() = default;

public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
	virtual bool HasUnsavedResourceChanges() const override { return m_IsDirty; }

private:
	void DrawCustomToolbar() override;
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
	void LoadTextureAndSlices(const std::filesystem::path& path);
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
	bool m_IsDirty = false;

	// 모달 팝업 상태
	bool m_ShowSliceModal = false;
	bool m_ShowCreateAnimModal = false;
	SpriteSliceParameters m_SliceParams;
	SpriteAutoAnimParameters m_AnimParams;
};

END
