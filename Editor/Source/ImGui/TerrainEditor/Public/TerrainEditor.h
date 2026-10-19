#pragma once

#include "ImGuiInterface.h"
#include "TerrainUtility.h"
#include "Entity.h"

BEGIN(Editor)

class TerrainEditor : public ImGuiInterface
{
public:
	TerrainEditor();
	virtual ~TerrainEditor() override;

	virtual void Draw() override;

	static TerrainEditor* Get() { return s_Instance; }

	bool IsSculptingActive() const { return m_Open && m_IsSculpting; }
	const Engine::TerrainBrushParams& GetBrushParams() const { return m_BrushParams; }
	Engine::Entity* GetTargetTerrainEntity() const { return m_TargetEntity; }
	void SetTargetTerrainEntity(Engine::Entity* entity) { m_TargetEntity = entity; }

private:
	static TerrainEditor* s_Instance;
	bool m_IsSculpting = false;
	Engine::TerrainBrushParams m_BrushParams{};
	Engine::Entity* m_TargetEntity = nullptr;
};
END