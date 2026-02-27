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
	f32 mouseX = InputManager::Get().GetMouseDelta().x;
	f32 mouseY = InputManager::Get().GetMouseDelta().y;
	f32 rotationSpeed = 0.1f; // 마우스 회전 속도 조절
	f32 movementSpeed = m_CameraSpeed * dt; // 이동 속도 조절
	vec3 moveDirection = vec3(0.0f);

	if (KEY_PRESSED("LShift")) movementSpeed *= 2.0f; // Shift 키를 누르면 이동 속도 증가

	if (m_Camera->GetIsPerspective())
	{
		vec3 fowrard = m_Transform->GetForward();
		vec3 right = m_Transform->GetRight();
		vec3 up = m_Transform->GetUp();
		if (KEY_PRESSED("W")) moveDirection += fowrard;
		if (KEY_PRESSED("S")) moveDirection -= fowrard;
		if (KEY_PRESSED("A")) moveDirection -= right;
		if (KEY_PRESSED("D")) moveDirection += right;

		if (KEY_PRESSED("Q")) moveDirection -= up;
		if (KEY_PRESSED("E")) moveDirection += up;
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
