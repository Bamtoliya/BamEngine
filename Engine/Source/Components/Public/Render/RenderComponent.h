#pragma once

#include "Component.h"
#include "RenderTypes.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "Mesh.h"
#include "RenderPass.h"

BEGIN(Engine)

CLASS()
class ENGINE_API RenderComponent : public Component
{
	REFLECT_CLASS(RenderComponent)
#pragma region Constructor&Destructor
protected:
	RenderComponent() {}
	virtual ~RenderComponent() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return __super::Initialize(arg); }
public:
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) PURE;
	virtual void Free() override
	{
		Component::Free();
		m_RenderPassID = { INVALID_PASS_ID };
		RELEASE_VECTOR(m_MaterialInstances);
	}
#pragma endregion
#pragma region Render
public:
	virtual void LateUpdate(f32 dt) override;
	virtual EResult Render(f32 dt, RenderPass* renderPass = nullptr) PURE;
#pragma endregion

#pragma region Management
public:
	void SetRenderPassID(RenderPassID passID) { m_RenderPassID = passID; }
	RenderPassID GetRenderPassID() const { return m_RenderPassID; }
#pragma endregion

#pragma region Material Management
public:
	// 공유 Material 설정 → MaterialInstance 자동 생성
	void SetMaterial(uint32 index, Material* material)
	{
		if (index >= m_MaterialInstances.size())
			m_MaterialInstances.resize(index + 1, nullptr);

		if (m_MaterialInstances[index])
		{
			m_MaterialInstances[index]->SetBaseMaterial(material);
		}
		else
		{
			m_MaterialInstances[index] = MaterialInstance::Create(material);
		}
	}

	void SetMaterial(Material* material)
	{
		SetMaterial(0, material);
	}

	// 원본 Material 접근
	Material* GetSharedMaterial(uint32 index = 0) const
	{
		if (index >= m_MaterialInstances.size() || !m_MaterialInstances[index])
			return nullptr;
		return m_MaterialInstances[index]->GetBaseMaterial();
	}

	// per-object 인스턴스 접근
	MaterialInstance* GetMaterialInstance(uint32 index = 0) const
	{
		if (index >= m_MaterialInstances.size())
			return nullptr;
		return m_MaterialInstances[index];
	}
#pragma endregion

#pragma region MyRegion
public:
	EResult BindPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass);
#pragma endregion




#pragma region Variable
protected:
	PROPERTY(CATEGORY("PROP_INFORMATION"))
	uint32 m_RenderPassID = { INVALID_PASS_ID };

	PROPERTY()
	vector<MaterialInstance*> m_MaterialInstances;
#pragma endregion
};
END