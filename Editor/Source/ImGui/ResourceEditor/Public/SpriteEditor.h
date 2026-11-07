#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include "Sprite.h"
#include "SpriteAnimation.h"
#include <vector>
#include <string>

BEGIN(Editor)

struct SpriteSliceInfo
{
	std::string name;                          // 슬라이스 식별자 (예: "Player_0")
	Engine::Rect region = { 0.f, 0.f, 0.f, 0.f }; // 픽셀 영역 (left, top, width, height)
	vec2 pivot = { 0.5f, 0.5f };               // 피벗 (0.0 ~ 1.0)
	vec4 border = { 0.f, 0.f, 0.f, 0.f };      // 9-Slice 인셋 (L, R, T, B)
};

enum class ESpriteSliceMethod : uint8
{
	GridByCellSize,
	GridByCellCount,
	Automatic
};

struct SpriteSliceParameters
{
	ESpriteSliceMethod method = ESpriteSliceMethod::GridByCellSize;
	int cellSizeX = 32;
	int cellSizeY = 32;
	int cellCountCols = 4;
	int cellCountRows = 4;
	int offsetX = 0;
	int offsetY = 0;
	int paddingX = 0;
	int paddingY = 0;
	vec2 defaultPivot = { 0.5f, 0.5f };
};

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
	virtual void Draw() override;

private:
	void DrawMenuBar();
	void DrawToolbar();
	void DrawCanvas(float availWidth, float availHeight);
	void DrawSidebar(float availWidth, float availHeight);
	void DrawSliceModal();
	void DrawCreateAnimationModal();

	void ExecuteSlice(const SpriteSliceParameters& params);
	void SaveSlices();
	void GenerateAnimation(const SpriteAutoAnimParameters& params);
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
