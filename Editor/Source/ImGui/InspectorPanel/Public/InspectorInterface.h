#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class InspectorInterface
{
public:
	virtual ~InspectorInterface() = default;
	virtual bool IsSupported(const std::filesystem::path& assetPath) { return false; }
	virtual bool OnInspectorGUI(const std::filesystem::path& assetPath) { return false; }
};
END