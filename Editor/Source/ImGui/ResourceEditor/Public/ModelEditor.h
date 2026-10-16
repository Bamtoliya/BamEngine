#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class ModelEditor final : public ResourceEditorInterface
{
public:
	ModelEditor() { m_Name = L"Model Editor"; m_DisplayNameKey = "ResourceEditor.ModelTitle"; }
	virtual ~ModelEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void Draw() override;


};
END