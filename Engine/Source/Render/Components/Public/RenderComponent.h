#pragma once

#include "Component.h"
#include "RenderTypes.h"
#include "ResourceHandle.h"
#include "Structs.h"

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

#pragma region Bounds
public:
	virtual std::optional<AABB> GetLocalBounds() const { return std::nullopt; }
#pragma endregion

#pragma region Management
public:
	void SetRenderPassID(RenderPassID passID) { m_RenderPassID = passID; }
	RenderPassID GetRenderPassID() const { return m_RenderPassID; }
#pragma endregion

#pragma region Material Management
public:
	void SetMaterial(const ResourceHandle<class MaterialInterface>& material, uint32 index = 0);	
	class MaterialInterface* GetMaterial(uint32 index = 0) const;
	uint32 GetMaterialSlotCount() const { return static_cast<uint32>(m_Materials.size()); }
	MaterialInterface* GetSharedMaterial(uint32 index = 0) const;
	class MaterialInstance* GetDynamicMaterialInstance(uint32 index = 0) const;
	MaterialInterface* GetEditableMaterial(uint32 index = 0, bool forceDynamicInstance = true);
public:
	EResult CreateDynamicMaterialInstance(uint32 index = 0);
	bool HasDynamicMaterialInstance(uint32 index = 0) const;
#pragma endregion

#pragma region Bind
public:
	EResult BindPipeline(class Mesh* mesh, class MaterialInterface* material, class RenderPass* renderPass);
#pragma endregion

#pragma region Save&Load
public:
	void Serialize(Archive& ar);
#pragma endregion


#pragma region Variable
protected:
	PROPERTY(EDITABLE)
	bool m_DrawShadow = { false };
	PROPERTY(EDITABLE)
	bool m_ReceiveShadow = { false };
	PROPERTY(EDITABLE, CATEGORY("PROP_INFORMATION"))
	uint32 m_RenderPassID = { INVALID_PASS_ID };

	PROPERTY(EDITABLE)
	vector<ResourceHandle<MaterialInterface>> m_Materials;

private:
	unordered_map<uint32, class MaterialInstance*> m_DynamicInstances;
#pragma endregion
};
END