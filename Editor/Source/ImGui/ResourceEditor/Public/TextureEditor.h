#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class TextureEditor final : public ResourceEditorInterface
{
public:
	TextureEditor() { m_Name = L"Texture Editor"; }
	virtual ~TextureEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void Draw() override;


};
END