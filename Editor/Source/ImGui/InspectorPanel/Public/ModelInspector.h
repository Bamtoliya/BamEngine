#pragma once
#include "InspectorInterface.h"
#include "ModelImporter.h"

BEGIN(Editor)
class ModelInspector final : public InspectorInterface
{
public:
	virtual ~ModelInspector() = default;

	bool IsSupported(const std::filesystem::path& assetPath) override;
	bool OnInspectorGUI(const std::filesystem::path& assetPath) override;
private:
	tagModelImportDesc m_ImportDesc;
};
END