#pragma once

#include "CameraManager.h"

IMPLEMENT_SINGLETON(CameraManager)

#pragma region Constructor&Destructor
EResult CameraManager::Initialize(void* arg)
{
	return EResult();
}
void CameraManager::Free()
{
	for (Camera* camera : m_Cameras)
	{
		Safe_Release(camera);
	}
	m_Cameras.clear();
	m_MainCamera = nullptr;
}
#pragma endregion

#pragma region Camera Management
EResult CameraManager::AddCamera(Camera* camera)
{
	if (!camera) return EResult::Fail;
		
	Safe_AddRef(camera);
	m_Cameras.push_back(camera);

	return EResult::Success;
}

EResult CameraManager::RemoveCamera(Camera* camera)
{
	if (!camera) return EResult::Fail;
	auto iter = std::find(m_Cameras.begin(), m_Cameras.end(), camera);
	if (iter != m_Cameras.end())
	{
		if (m_MainCamera == camera)
			m_MainCamera = nullptr;
		Safe_Release(*iter);
		m_Cameras.erase(iter);
		return EResult::Success;
	}
	return EResult::Fail;
}
#pragma endregion


