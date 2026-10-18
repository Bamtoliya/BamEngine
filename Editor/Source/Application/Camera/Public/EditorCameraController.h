#pragma once

#include "Editor_Includes.h"
#include "Components/Render/CameraComponent.h"

#include <entt/entt.hpp>
#include <string>

BEGIN(Engine)
class Scene;
class Entity;
struct TransformComponent;
END

BEGIN(Editor)
class EditorCameraController final : public Engine::Base
{
private:
    EditorCameraController() = default;
    ~EditorCameraController() override = default;

    bool Initialize(const std::wstring& name, const Engine::CameraComponent& settings);

public:
    static EditorCameraController* Create(const std::wstring& name, const Engine::CameraComponent& settings);
    void Free() override;

    void HandleInput(Engine::f32 dt);
    void SetControlRotation(const Engine::vec3& eulerAngles);
    void SyncControlRotationFromTransform();
    void FocusEntity(Engine::Entity* targetEntity);

    Engine::Entity* GetEntity() const;
    Engine::TransformComponent* GetTransformComponent() const;
    Engine::CameraComponent* GetCameraComponent() const;
    std::optional<CameraBuffer> GetCameraBuffer() const;

    bool IsPerspective() const;
    bool IsOrthographic() const;
    void SetPerspective(bool perspective = true);
    void SetOrthographic(bool orthographic = true);

    bool SetAspectRatio(Engine::f32 aspectRatio);
    void RefreshMatrices();

    void IncreaseCameraSpeed() { m_CameraSpeed += 1.0f; }
    void DecreaseCameraSpeed() { m_CameraSpeed = std::max(m_CameraSpeed - 1.0f, 1.0f); }
    void SetZoomSpeed3D(Engine::f32 speed) { m_ZoomSpeed3D = speed; }
    void SetZoomSpeed2D(Engine::f32 speed) { m_ZoomSpeed2D = speed; }
    void SetFOVZoom(bool zoomType) { m_IsFOVZoom = zoomType; }

private:
    Engine::Scene* m_EditorScene = nullptr;
    entt::entity m_CameraEntity = entt::null;
    Engine::f32 m_CameraSpeed = 100.0f;
    bool m_IsFOVZoom = false;
    Engine::f32 m_ZoomSpeed3D = 2.0f;
    Engine::f32 m_ZoomSpeed2D = 0.5f;
    Engine::vec3 m_ControlRotation = Engine::vec3(0.0f);
};
END