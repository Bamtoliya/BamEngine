#pragma once

#include "Camera.h"

BEGIN(Engine)
class ENGINE_API CameraManager final : public Base
{
	DECLARE_SINGLETON(CameraManager)
#pragma region Constructor&Destructor
private:
	CameraManager() {}
	virtual ~CameraManager() {}
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Camera Management
public:
	EResult AddCamera(Camera* camera);
	EResult RemoveCamera(Camera* camera);
	EResult ClearCameras();
public:
	Camera* GetCameraByIndex(size_t index) const;
public:
	Camera* GetMainCamera() const
	{
		if (m_MainCamera) return m_MainCamera;
		else return m_Cameras.empty() ? nullptr : m_Cameras[0];
	}
	void SetMainCamera(Camera* camera)
	{
		
		if(m_MainCamera != camera)
		{
			if(m_MainCamera)
				m_MainCamera->SetMainCamera(false);
			m_MainCamera = camera;
			if(m_MainCamera)
				m_MainCamera->SetMainCamera(true);
		}
	}
public:
	const vector<Camera*>& GetCameras() const { return m_Cameras; }

#pragma endregion

#pragma region Member Variable
private:
	vector<Camera*> m_Cameras;
	Camera* m_MainCamera = { nullptr };
#pragma endregion

};
END