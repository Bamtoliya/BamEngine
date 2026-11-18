#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class TextureEditor final : public ResourceEditorInterface
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override
	{
		return PrepareTargetPathRelocation(relocation);
	}
	TextureEditor() { m_Name = L"Texture Editor"; m_DisplayNameKey = "ResourceEditor.TextureTitle"; }
	virtual ~TextureEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
protected:
	void DrawCustomQuickAccess() override;
private:
	void DrawCustomContent() override;
};
END