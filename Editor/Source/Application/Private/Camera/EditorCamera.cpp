#pragma once

#include "EditorCamera.h"


#pragma region Constructor&Destructor
EResult EditorCamera::Initialize(void* arg)
{
	// Transform 컴포넌트 추가
	m_Transform = AddComponent<Transform>();
	Safe_AddRef(m_Transform);
	// Camera 컴포넌트 추가
	m_Camera = AddComponent<Camera>();
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
#pragma endregion
