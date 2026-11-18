#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class ShaderEditor final : public ResourceEditorInterface
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override
	{
		return PrepareTargetPathRelocation(relocation);
	}
	ShaderEditor() { m_Name = L"Shader Editor"; m_DisplayNameKey = "ResourceEditor.ShaderTitle"; }
	virtual ~ShaderEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
protected:
	void DrawCustomQuickAccess() override;
private:
	void DrawCustomContent() override;


};
END