#pragma once

#include "ResourceEditorInterface.h"
#include "Model.h"

BEGIN(Editor)

class ModelEditor final : public ResourceEditorInterface
{
public:
	ModelEditor() { m_Name = L"Model Editor"; m_DisplayNameKey = "ResourceEditor.ModelTitle"; }
	virtual ~ModelEditor() = default;

public:
	bool IsSupported(const std::filesystem::path& assetPath) const override;
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	void Draw() override;

private:
	void InstantiateModel();
	void DrawMaterialSlots(Engine::Model& model, bool canEdit);
	void SaveModel();

private:
	Engine::ResourceHandle<Engine::Model> m_ModelHandle;
	string m_StatusKey;
};

END