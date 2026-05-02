#pragma once
#include "InspectorInterface.h"

BEGIN(Editor)
class ShaderInspector final : public InspectorInterface
{
public:
	virtual ~ShaderInspector() = default;

	bool IsSupported(const std::filesystem::path& assetPath) override;
	bool OnInspectorGUI(const std::filesystem::path& assetPath) override;
};
END