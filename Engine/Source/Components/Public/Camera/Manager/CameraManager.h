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
public:
	Camera* GetCameraByIndex(size_t index) const;
public:
	Camera* GetMainCamera() const { return m_MainCamera; }
	void SetMainCamera(Camera* camera) { m_MainCamera = camera; };
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