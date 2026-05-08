#pragma once

#include "Editor_Includes.h"
#include "Camera.h"
#include "Transform.h"
#include "GameObject.h"
#include "RenderPass.h"

BEGIN(Editor)

class EditorCamera : public Engine::GameObject
{
private:
    EditorCamera() {}
    virtual ~EditorCamera() = default;
    Engine::EResult Initialize(void* arg = nullptr) override;

public:
    static EditorCamera* Create(void* arg = nullptr);
    virtual void Free() override;

public:
    virtual void FixedUpdate(Engine::f32 dt) override;
    virtual void Update(Engine::f32 dt) override;
    virtual void LateUpdate(Engine::f32 dt) override;
    void HandleInput(Engine::f32 dt);

public:
    Engine::Camera* GetCamera() const { return m_Camera; }
    Engine::Transform* GetTransform() const { return m_Transform; }

public:
    void IncreaseCameraSpeed() { m_CameraSpeed += 1.0f; }
    void DecreaseCameraSpeed() { m_CameraSpeed = std::max(m_CameraSpeed - 1.0f, 1.0f); }

public:
    void SetZoomSpeed3D(Engine::f32 speed) { m_ZoomSpeed3D = speed; }
    void SetZoomSpeed2D(Engine::f32 speed) { m_ZoomSpeed2D = speed; }
    void SetFOVZoom(bool zoomType) { m_IsFOVZoom = zoomType; }

private:
    void FocusGameObject(GameObject* gameObject);

private:
    Engine::Transform* m_Transform = nullptr;
    Engine::Camera* m_Camera = nullptr;

private:
    Engine::f32 m_CameraSpeed = 100.0f;
    bool m_IsFOVZoom = false;
    Engine::f32 m_ZoomSpeed3D = 2.0f;
    Engine::f32 m_ZoomSpeed2D = 0.5f;
};

END