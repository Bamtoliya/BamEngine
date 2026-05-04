#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class ShaderEditor final : public ResourceEditorInterface
{
public:
	ShaderEditor() { m_Name = L"Shader Editor"; }
	virtual ~ShaderEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void Draw() override;


};
END