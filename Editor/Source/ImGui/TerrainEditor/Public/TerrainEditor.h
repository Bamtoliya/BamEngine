#pragma once

#include "ImGuiInterface.h"
#include "TerrainUtility.h"
#include "Entity.h"

BEGIN(Editor)

// 뷰포트 브러시가 하는 일: 높이 조형, 스플랫 레이어 페인트, 속성 맵(차단·물) 페인트
enum class ETerrainTool : uint8 { Sculpt, Paint, Attribute };

class TerrainEditor : public IImGui
{
public:
	TerrainEditor();
	virtual ~TerrainEditor() override;

	virtual void Draw() override;

	static TerrainEditor* Get() { return s_Instance; }
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::SceneEditing; }
	// 브러시가 켜져 있으면 뷰포트 좌클릭이 선택 대신 지형 편집이 된다(도구와 무관).
	bool IsSculptingActive() const { return m_Open && m_IsSculpting; }
	const Engine::TerrainBrushParams& GetBrushParams() const { return m_BrushParams; }
	Engine::Entity* GetTargetTerrainEntity() const { return m_TargetEntity; }
	void SetTargetTerrainEntity(Engine::Entity* entity) { m_TargetEntity = entity; }

	ETerrainTool GetTool() const { return m_Tool; }
	uint32 GetPaintLayer() const { return m_PaintLayer; }
	uint8 GetPaintAttribute() const { return m_PaintAttribute; }
	// 1초 동안 누르고 있을 때 더해지는 가중치 비율(0~1). 프레임마다 dt를 곱해 쓴다.
	float GetPaintStrength() const { return m_PaintStrength; }

	// 단축키(ShortcutManager)와 패널 UI가 함께 쓰는 조작
	bool CanUseLayerShortcuts() const { return IsSculptingActive() && m_Tool == ETerrainTool::Paint; }
	void SelectPaintLayer(uint32 layer);
	void AdjustBrushRadius(float factor);

private:
	void DrawPalette(const Engine::TerrainComponent& terrain);

	static TerrainEditor* s_Instance;
	bool m_IsSculpting = false;
	Engine::TerrainBrushParams m_BrushParams{};
	Engine::Entity* m_TargetEntity = nullptr;
	ETerrainTool m_Tool = ETerrainTool::Sculpt;
	uint32 m_PaintLayer = 0;
	uint8 m_PaintAttribute = 1; // ETerrainAttribute::Blocked
	float m_PaintStrength = 1.0f;
};
END