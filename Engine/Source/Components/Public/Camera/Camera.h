#pragma once

#include "Component.h"

BEGIN(Engine)
class ENGINE_API Camera final : public Component
{
#pragma region Constructor&Destructor
	private:
	Camera() : Component{} {}
	virtual ~Camera() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Component* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
#pragma region Loop
	public:
	virtual void	Update(f32 dt) override;
	virtual EResult	Render(f32 dt) override;
#pragma endregion


#pragma region Variables
private:
	f32 m_FOV = { 60.f };
	f32 m_Near = { 0.1f };
	f32 m_Far = { 1000.f };

	glm::mat4 m_ProjMatrix = glm::identity<mat4>();
	glm::mat4 m_ViewMatrix = glm::identity<mat4>();
#pragma endregion	
}

END