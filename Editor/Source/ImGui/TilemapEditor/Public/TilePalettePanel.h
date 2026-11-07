#pragma once

#include "ImGuiInterface.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include "Texture.h"
#include <vector>
#include <filesystem>

BEGIN(Editor)

enum class ETileBrushMode : uint8
{
	Paint,
	Erase,
	BoxFill,
	Picker
};

class TilePalettePanel final : public IImGui
{
public:
	TilePalettePanel();
	virtual ~TilePalettePanel() = default;

	virtual void Draw() override;
	virtual EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }

public:
	void AddAssetToPalette(const std::filesystem::path& assetPath);

	void SetSelectedSprite(ResourceHandle<Engine::Sprite> sprite) { m_SelectedSprite = sprite; }
	ResourceHandle<Engine::Sprite> GetSelectedSprite() const { return m_SelectedSprite; }

	ETileBrushMode GetBrushMode() const { return m_BrushMode; }
	void SetBrushMode(ETileBrushMode mode) { m_BrushMode = mode; }

private:
	void DrawToolbar();
	void DrawPaletteGrid();

private:
	ResourceHandle<Engine::Texture> m_PaletteTexture;
	std::vector<ResourceHandle<Engine::Sprite>> m_PaletteSprites;
	ResourceHandle<Engine::Sprite> m_SelectedSprite;

	ETileBrushMode m_BrushMode = ETileBrushMode::Paint;
	float m_TileThumbnailSize = 48.0f;
};

END
