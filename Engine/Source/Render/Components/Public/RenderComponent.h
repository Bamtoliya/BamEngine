#pragma once

#include "Component.h"
#include "RenderTypes.h"
#include "ResourceHandle.h"

BEGIN(Engine)

CLASS()
class ENGINE_API RenderComponent : public Component
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
protected:
	RenderComponent() {}
	virtual ~RenderComponent() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	virtual void Free() override;
#pragma endregion
#pragma region Render
public:
	virtual void LateUpdate(f32 dt) override;
	virtual EResult Render(f32 dt, class RenderPass* renderPass = nullptr) { return EResult::NotImplemented; }
#pragma endregion

#pragma region Management
public:
	void SetRenderPassID(RenderPassID passID) { m_RenderPassID = passID; }
	RenderPassID GetRenderPassID() const { return m_RenderPassID; }
#pragma endregion

#pragma region Material Management
public:
	void SetMaterial(const ResourceHandle<class Material>& material, uint32 index = 0);	

	class Material* GetSharedMaterial(uint32 index = 0) const;
	class MaterialInstance* GetMaterialInstance(uint32 index = 0) const;
#pragma endregion

#pragma region Bind
public:
	EResult BindPipeline(class Mesh* mesh, class MaterialInterface* material, class RenderPass* renderPass);
#pragma endregion

#pragma region Variable
protected:
	PROPERTY(EDITABLE, CATEGORY("PROP_INFORMATION"))
	uint32 m_RenderPassID = { INVALID_PASS_ID };

	PROPERTY(EDITABLE)
	vector<ResourceHandle<class MaterialInstance>> m_MaterialInstances;
#pragma endregion
};
END