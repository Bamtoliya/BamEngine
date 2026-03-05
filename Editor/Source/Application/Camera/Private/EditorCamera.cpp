#pragma once

#include "EditorCamera.h"
#include "InputManager.h"


#pragma region Constructor&Destructor
EResult EditorCamera::Initialize(void* arg)
{
	// Transform 컴포넌트 추가
	m_Transform = AddComponent<Transform>();
	Safe_AddRef(m_Transform);
	// Camera 컴포넌트 추가
	m_Camera = AddComponent<Camera>(arg);
	Safe_AddRef(m_Camera);
	return EResult::Success;
}

EditorCamera* EditorCamera::Create(void* arg)
{
	EditorCamera* instance = new EditorCamera();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void EditorCamera::Free()
{
	Safe_Release(m_Transform);
	Safe_Release(m_Camera);
	__super::Free();
}

void EditorCamera::FixedUpdate(f32 dt)
{
	__super::FixedUpdate(dt);
}

void EditorCamera::Update(f32 dt)
{
	__super::Update(dt);
}

void EditorCamera::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
}
void EditorCamera::HandleInput(f32 dt)
{
	InputManager& inputManager = InputManager::Get();
	f32 mouseX = inputManager.GetMouseDelta().x;
	f32 mouseY = inputManager.GetMouseDelta().y;
	vec2 mouseScrollDelta = inputManager.GetMouseScrollDelta();
	f32 rotationSpeed = 0.3f; // 마우스 회전 속도 조절
	f32 movementSpeed = m_CameraSpeed * dt; // 이동 속도 조절
	vec3 moveDirection = vec3(0.0f);

	if (KEY_PRESSED("LShift")) movementSpeed *= 2.0f; // Shift 키를 누르면 이동 속도 증가

	if (mouseScrollDelta.y != 0.f)
	{
		if (m_Camera->GetIsPerspective())
		{
			if (m_IsFOVZoom)
			{
				f32 currentFOV = m_Camera->GetFOV();
				f32 newFOV = currentFOV - (mouseScrollDelta.y * m_ZoomSpeed3D);

				newFOV = std::clamp(newFOV, 1.0f, 170.0f);

				m_Camera->SetFOV(newFOV);
			}
			else
			{
				vec3 pos = m_Transform->GetWorldPosition();
				vec3 forward = m_Transform->GetForward();

				// 마우스 스크롤 방향으로 전진/후진
				pos -= forward * (mouseScrollDelta.y * m_ZoomSpeed3D);
				m_Transform->SetPosition(pos);
			}
		}
		else
		{
			f32 currentOrthoSize = m_Camera->GetOrthoSize();
			f32 newOrthoSize = currentOrthoSize - (mouseScrollDelta.y * m_ZoomSpeed2D);

			newOrthoSize = std::max(newOrthoSize, 0.1f);

			m_Camera->SetOrthoSize(newOrthoSize);
		}
	}

	if (m_Camera->GetIsPerspective())
	{
		if (MOUSE_BUTTON_PRESSED("Right"))
		{
			vec3 currentRot = m_Transform->GetLocalRotationEuler();
			currentRot.y -= mouseX * rotationSpeed;
			currentRot.x -= mouseY * rotationSpeed;
			currentRot.x = std::clamp(currentRot.x, -89.0f, 89.0f); // 수직 회전 제한
			currentRot.z = 0.0f; // 롤 회전 방지
			m_Transform->SetRotation(currentRot);
		}

		if (KEY_PRESSED("W")) moveDirection -= vec3(0.f, 0.f, 1.f);
		if (KEY_PRESSED("S")) moveDirection += vec3(0.f, 0.f, 1.f);
		if (KEY_PRESSED("A")) moveDirection -= vec3(1.0f, 0.f, 0.f);
		if (KEY_PRESSED("D")) moveDirection += vec3(1.0f, 0.f, 0.f);

		if (KEY_PRESSED("Q")) moveDirection += vec3(0.0f, 1.f, 0.f);
		if (KEY_PRESSED("E")) moveDirection -= vec3(0.0f, 1.f, 0.f);
	}
	else
	{
		if (KEY_PRESSED("W"))
			moveDirection += vec3(0.0f, 1.0f, 0.0f);
		if (KEY_PRESSED("S"))
			moveDirection += vec3(0.0f, -1.0f, 0.0f);
		if (KEY_PRESSED(EKeyCode::A))
			moveDirection += vec3(-1.0f, 0.0f, 0.0f);
		if (KEY_PRESSED(EKeyCode::D))
			moveDirection += vec3(1.0f, 0.0f, 0.0f);
	}

	if(KEY_PRESSED("="))
		IncreaseCameraSpeed();
	if(KEY_PRESSED("-"))
		DecreaseCameraSpeed();

	if (glm::length2(moveDirection) > 0.001f)
	{
		moveDirection = glm::normalize(moveDirection);
		m_Transform->Translate(moveDirection * movementSpeed);
	}
}
#pragma endregion
