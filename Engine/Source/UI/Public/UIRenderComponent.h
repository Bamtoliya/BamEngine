#pragma once

#include "UIComponent.h"
#include "ResourceHandle.h"
#include "MaterialInterface.h"

struct tagUIUBO
{
	mat4 worldMatrix;
	vec4 uvRegion;
	vec2 tiling;
	vec2 offset;
};

BEGIN(Engine)
CLASS()
class ENGINE_API UIRenderComponent : public UIComponent
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
protected:
	UIRenderComponent() {}
	virtual ~UIRenderComponent() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static UIRenderComponent* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void LateUpdate(f32 dt) override;
	EResult Render(f32 dt, class RenderPass* renderPass = nullptr);
#pragma endregion

#pragma region Bind
public:
	EResult BindPipeline(class Mesh* mesh, class MaterialInterface* material, class RenderPass* renderPass);
#pragma endregion

#pragma region Material
public:
	ResourceHandle<MaterialInterface> GetMaterial() const { return m_Material; }
	MaterialInterface* GetMaterialInstance() const { return m_Material.Get(); }
	void SetMaterial(const ResourceHandle<MaterialInterface>& material) { m_Material = material; }
#pragma endregion

#pragma region Member Variable
protected:
	PROPERTY(EDITABLE, COLOR())
	vec4 m_Color = { 1.f, 1.f, 1.f, 1.f };
	PROPERTY(EDITABLE, CATEGORY("PROP_RENDER"))
	uint32 m_VisibilityChannel = { 1 << 0 };
	PROPERTY(EDITABLE)
	ResourceHandle<MaterialInterface> m_Material;
#pragma endregion

};
END