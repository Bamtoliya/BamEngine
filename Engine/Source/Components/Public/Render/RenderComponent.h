#pragma once

#include "Component.h"
#include "RenderTypes.h"

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
	}
#pragma endregion
#pragma region Render
public:
	virtual void LateUpdate(f32 dt) override;
	virtual EResult Render(f32 dt) PURE;
#pragma endregion

#pragma region Management
public:
	void SetRenderPassID(RenderPassID passID) { m_RenderPassID = passID; }
	RenderPassID GetRenderPassID() const { return m_RenderPassID; }
#pragma endregion


#pragma region Variable
protected:
	PROPERTY(CATEGORY("PROP_INFORMATION"))
	uint32 m_RenderPassID = { INVALID_PASS_ID };
#pragma endregion
};
END