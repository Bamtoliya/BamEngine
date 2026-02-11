#pragma once

#include "Component.h"
#include "ReflectionMacro.h"

struct tagCameraDesc
{
	bool IsPerspective = { true };
	f32 FOV = { 60.f };
	f32 Near = { 0.1f };
	f32 Far = { 1000.f };
	f32 Aspect = { 1.777f };
	f32 Width = { 10.f };
	f32 Height = { 10.f };
};

BEGIN(Engine)

CLASS()
class ENGINE_API Camera final : public Component
{
	REFLECT_CLASS(Camera)
	using DESC = tagCameraDesc;
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
	virtual void	FixedUpdate(f32 dt) override;
	virtual void	Update(f32 dt) override;
	virtual void	LateUpdate(f32 dt) override;
	virtual EResult	Render(f32 dt) override;
#pragma endregion

#pragma region MyRegion
public:
	const mat4& GetViewMatrix() const { return m_ViewMatrix; }
	const mat4& GetProjMatrix() const { return m_ProjMatrix; }
	const mat4& GetViewMatrixInv() const { return m_ViewMatrixInv; }
	const mat4& GetProjMatrixInv() const { return m_ProjMatrixInv; }
#pragma endregion

#pragma region MyRegion
public:
	void SetPerspective(f32 fov, f32 aspect, f32 near, f32 far);
	void SetOrthographic(f32 width, f32 height, f32 near, f32 far);
#pragma endregion

#pragma region Matrix
private:
	void UpdateMatrix();
#pragma endregion


#pragma region Variables
private:

	PROPERTY()
	f32 m_FOV = { 60.f };
	PROPERTY()
	f32 m_Near = { 0.1f };
	PROPERTY()
	f32 m_Far = { 1000.f };
	PROPERTY()
	f32 m_Aspect = { 1.777f };
	PROPERTY()
	f32 m_Width = { 10.f };
	PROPERTY()
	f32 m_Height = { 10.f };


	PROPERTY()
	bool m_IsPerspective = { true };

	PROPERTY(READONLY)
	glm::mat4 m_ProjMatrix = glm::identity<mat4>();
	glm::mat4 m_ProjMatrixInv = glm::identity<mat4>();
	PROPERTY(READONLY)
	glm::mat4 m_ViewMatrix = glm::identity<mat4>();
	glm::mat4 m_ViewMatrixInv = glm::identity<mat4>();
#pragma endregion	
};
END