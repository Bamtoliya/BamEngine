#pragma once
#include "Camera.h"
#include "GameObject.h"
#include "Transform.h"
#include "CameraManager.h"

REGISTER_COMPONENT(Camera);

#pragma region Constructor&Destructor
EResult Camera::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_IsPerspective = desc->IsPerspective;
		m_FOV = desc->FOV;
		m_Near = desc->Near;
		m_Far = desc->Far;
		m_Aspect = desc->Aspect;
	}
	UpdateMatrix();
	return EResult::Success;
}

Component* Camera::Create(void* arg)
{
	Camera* instance = new Camera();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* Camera::Clone(GameObject* owner, void* arg)
{
	return nullptr;
}

void Camera::Free()
{
	CameraManager::Get().RemoveCamera(this);
	__super::Free();
}
#pragma endregion

#pragma region Loop
void Camera::FixedUpdate(f32 dt)
{
	UpdateMatrix();
}
void Camera::Update(f32 dt)
{
	
}

void Camera::LateUpdate(f32 dt)
{
}

EResult Camera::Render(f32 dt, RenderPass* renderPass)
{
	return EResult();
}

#pragma endregion

#pragma region ProjectionMatrix
void Camera::SetPerspective(f32 fov, f32 aspect, f32 near, f32 far)
{
	m_ProjMatrix = glm::perspective(fov, aspect, near, far);
	m_ProjMatrixInv = glm::inverse(m_ProjMatrix);
}
void Camera::SetOrthographic(f32 width, f32 height, f32 near, f32 far)
{
	m_ProjMatrix = glm::ortho(
		-width * 0.5f,  // Left
		width * 0.5f,  // Right
		-height * 0.5f, // Bottom
		height * 0.5f, // Top
		near,          // zNear
		far            // zFar
	);
	m_ProjMatrixInv = glm::inverse(m_ProjMatrix);
}
#pragma endregion

void Camera::UpdateMatrix()
{
	if (!m_Owner) return;
	Transform* transform = m_Owner->GetComponent<Transform>();
	if (transform)
	{
		m_ViewMatrix = glm::inverse(transform->GetWorldMatrix());
		m_ViewMatrixInv = transform->GetWorldMatrix();
		if (m_IsPerspective)
			SetPerspective(glm::radians(m_FOV), m_Aspect, m_Near, m_Far);
		else
			SetOrthographic(m_Width, m_Height, m_Near, m_Far);
	}
}

#pragma region Matrix
tagCameraBuffer Camera::GetCameraBuffer() const
{
	tagCameraBuffer buffer;
	buffer.viewMatrix = m_ViewMatrix;
	buffer.projMatrix = m_ProjMatrix;
	buffer.viewProjMatrix = m_ProjMatrix * m_ViewMatrix;
	if (m_Owner)
		buffer.cameraPosition = m_Owner->GetComponent<Transform>()->GetWorldPosition();
	return buffer;
}
#pragma endregion
