#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class SpriteEditor final : public ResourceEditorInterface
{
public:
	SpriteEditor() { m_Name = L"Sprite Editor"; }
	virtual ~SpriteEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void Draw() override;


};
END