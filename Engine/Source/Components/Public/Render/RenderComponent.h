#pragma once

#include "Component.h"
#include "RenderTypes.h"

BEGIN(Engine)
class ENGINE_API RenderComponent : public Component
{
#pragma region Constructor&Destructor
protected:
	RenderComponent() {}
	virtual ~RenderComponent() = default;
	virtual EResult Initialize(void* arg = nullptr) override { return EResult::Success; }
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
	virtual EResult Render(f32 dt) PURE;
#pragma endregion

#pragma region Management
public:
	void SetRenderPassID(RenderPassID passID) { m_RenderPassID = passID; }
	RenderPassID GetRenderPassID() const { return m_RenderPassID; }

	const mat4& GetWorldMatrix() const;
#pragma endregion


#pragma region Variable
protected:
	RenderPassID m_RenderPassID = { INVALID_PASS_ID };
#pragma endregion
};
END