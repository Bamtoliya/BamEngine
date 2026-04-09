#pragma once
#include "InspectorInterface.h"

BEGIN(Editor)
class TextureInspector final : public InspectorInterface
{
public:
	virtual ~TextureInspector() = default;

	bool IsSupported(const std::filesystem::path& assetPath) override;
	bool OnInspectorGUI(const std::filesystem::path& assetPath) override;
};
END