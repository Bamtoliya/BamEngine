#pragma once

#include "ResourceEditorInterface.h"

BEGIN(Editor)
class MaterialEditor final : public ResourceEditorInterface
{
public:
	MaterialEditor() { m_Name = L"Material Editor"; }
	virtual ~MaterialEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void Draw() override;

	void SetRenderComponentContext(class RenderComponent* renderComponent, uint32 slot)
	{
		m_ContextRenderComponent = renderComponent;
		m_ContextSlot = slot;
	}

private:
	class RenderComponent* m_ContextRenderComponent = nullptr;
	uint32 m_ContextSlot = 0;
	bool m_EditDynamicInstance = true;
};
END