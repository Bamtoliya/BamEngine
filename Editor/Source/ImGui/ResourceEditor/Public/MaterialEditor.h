#pragma once

#include "ResourceEditorInterface.h"
#include "RenderPassManager.h"

BEGIN(Engine)
class RenderTarget;
class Camera;
END

BEGIN(Editor)
class MaterialEditor final : public ResourceEditorInterface
{
public:
	MaterialEditor() { m_Name = L"Material Editor"; }
	void Initialize();
	virtual ~MaterialEditor() = default;
	virtual void Free() override;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void Update(f32 dt) override;
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

	// --- 렌더링 미리보기(Preview)용 추가 변수 ---
	Engine::RenderTarget* m_FinalColorRT = nullptr;
	Engine::RenderPassID m_GeometryPassID = 0;
	Engine::RenderPassID m_LightingPassID = 0;
	class Engine::RHIPipeline* m_LightingPipeline = nullptr;

	std::wstring m_GBufferNames[5];
	std::wstring m_DepthName;
	std::wstring m_FinalColorName;
	std::wstring m_ShadowDepthName;
};
END