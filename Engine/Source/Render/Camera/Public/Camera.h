#pragma once

#include "Component.h"
#include "ReflectionMacro.h"

struct tagCameraBuffer
{
	mat4 viewMatrix;
	mat4 projMatrix;
	mat4 viewProjMatrix;
	vec3 cameraPosition;
	float time;
};

struct tagCameraDesc : public tagComponentDesc
{
	bool IsPerspective = { true };
	f32 FOV = { 60.f };
	f32 Near = { 0.1f };
	f32 Far = { 1000.f };
	f32 Aspect = { 1.777f };
	f32 OrthoSize = { 10.f };
};


BEGIN(Engine)
CLASS()
class ENGINE_API Camera final : public Component
{
	REFLECT_CLASS()
	using DESC = tagCameraDesc;
#pragma region Constructor&Destructor
private:
	Camera() : Component{} {}
	virtual ~Camera() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Camera* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
#pragma region Loop
public:
	virtual void	FixedUpdate(f32 dt) override;
	virtual void	Update(f32 dt) override;
	virtual void	LateUpdate(f32 dt) override;
	virtual EResult	Render(f32 dt, RenderPass* renderPass) override;
#pragma endregion

#pragma region Matrix
public:
	const mat4& GetViewMatrix() const { return m_ViewMatrix; }
	const mat4& GetProjMatrix() const { return m_ProjMatrix; }
	const mat4& GetViewMatrixInv() const { return m_ViewMatrixInv; }
	const mat4& GetProjMatrixInv() const { return m_ProjMatrixInv; }
public:
	tagCameraBuffer GetCameraBuffer() const;
private:
	void UpdateMatrix();
#pragma endregion

#pragma region Projection Space
public:
	bool GetIsPerspective() { return m_IsPerspective; }
	void SetPerspective(bool perspective = true) { m_IsPerspective = perspective; }
	void SetPerspective(f32 fov, f32 aspect, f32 near, f32 far);
	void SetOrthographic(f32 width, f32 height, f32 near, f32 far);
public:
	f32 GetFOV() const { return m_FOV; }
	f32 GetOrthoSize() const { return m_OrthoSize; }
public:
	void SetFOV(f32 fov) { m_FOV = fov; UpdateMatrix(); }
	void SetOrthoSize(f32 orthoSize) { m_OrthoSize = orthoSize; UpdateMatrix(); }
#pragma endregion

#pragma region Variables
private:

	PROPERTY(EDITCONDITION("m_IsPerspective"))
	f32 m_FOV = { 60.f };
	PROPERTY()
	f32 m_Near = { 0.1f };
	PROPERTY()
	f32 m_Far = { 1000.f };
	PROPERTY(EDITCONDITION("m_IsPerspective"))
	f32 m_Aspect = { 1.777f };
	PROPERTY(EDITCONDITION("!m_IsPerspective"))
	f32 m_OrthoSize = { 10.f };

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