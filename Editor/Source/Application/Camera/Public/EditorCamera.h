#pragma once

#include "Camera.h"
#include "Transform.h"
#include "GameObject.h"
#include "RenderPass.h"

BEGIN(Editor)
class EditorCamera : public GameObject
{
private:
	EditorCamera() {}
	virtual ~EditorCamera() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static EditorCamera* Create(void* arg = nullptr);
	virtual void Free() override;
public:
	virtual void FixedUpdate(f32 dt) override;
	virtual void Update(f32 dt) override;
	virtual void LateUpdate(f32 dt) override;
	void HandleInput(f32 dt);

public:
	Camera* GetCamera() const { return m_Camera; }
	Transform* GetTransform() const { return m_Transform; }

public:
	void IncreaseCameraSpeed() { m_CameraSpeed += 1.0f; }
	void DecreaseCameraSpeed() { m_CameraSpeed = std::max(m_CameraSpeed - 1.0f, 1.0f); }
public:
	void SetZoomSpeed3D(f32 speed) { m_ZoomSpeed3D = speed; }
	void SetZoomSpeed2D(f32 speed) { m_ZoomSpeed2D = speed; }
public:
	void SetFOVZoom(bool zoomType) { m_IsFOVZoom = zoomType; }
private:
	Transform* m_Transform = { nullptr };
	Camera* m_Camera = { nullptr };
private:
	f32 m_CameraSpeed = { 100.0f };
	bool m_IsFOVZoom = { false };
	f32 m_ZoomSpeed3D = { 2.0f };
	f32 m_ZoomSpeed2D = { 0.5f };
};
END