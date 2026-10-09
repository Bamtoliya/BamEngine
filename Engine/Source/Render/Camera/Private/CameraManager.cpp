#pragma once

#include "CameraManager.h"
#include <algorithm>

IMPLEMENT_SINGLETON(CameraManager)

#pragma region Constructor&Destructor
EResult CameraManager::Initialize(void* arg)
{
	return EResult();
}
void CameraManager::Free()
{
	ClearCameras();
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
	if (!camera) return EResult::InvalidArgument;

	auto iter = std::find(m_Cameras.begin(), m_Cameras.end(), camera);
	if (iter == m_Cameras.end()) return EResult::Fail;

	if (m_MainCamera == camera)
		SetMainCamera(nullptr);

	Camera* removingCamera = *iter;
	m_Cameras.erase(iter);
	Safe_Release(removingCamera);

	return EResult::Success;
}
EResult CameraManager::ClearCameras()
{
	SetMainCamera(nullptr);

	vector<Camera*> cameras;
	cameras.swap(m_Cameras);

	for (auto& camera : cameras)
		Safe_Release(camera);

	return EResult::Success;
}
Camera* CameraManager::GetCameraByIndex(size_t index) const
{
	if (index < m_Cameras.size())
		return m_Cameras[index];
	return nullptr;
}
void CameraManager::SetMainCamera(Camera* camera)
{
	if (camera && std::find(m_Cameras.begin(), m_Cameras.end(), camera) == m_Cameras.end())
	{
		ENGINE_LOG_WARN("Cannot set an unregistered camera as the main camera.");
		return;
	}

	if (m_MainCamera == camera) return;

	Camera* previousCamera = m_MainCamera;
	m_MainCamera = camera;

	if (previousCamera)
		previousCamera->SetMainCamera(false);

	if (m_MainCamera)
		m_MainCamera->SetMainCamera(true);
}
#pragma endregion


