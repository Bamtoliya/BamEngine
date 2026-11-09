#pragma once

#include "Editor_Includes.h"
#include "CameraComponent.h"
#include <entt/entt.hpp>
#include "InputMap.h"

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

    void HandleInput(Engine::f32 dt, bool allowScroll = true);
    void HandleInput(Engine::f32 dt, bool allowScroll, std::optional<Engine::EMouseButton> dragButton);
    void SetControlRotation(const Engine::vec3& eulerAngles);
    void SyncControlRotationFromTransform();
    void FocusEntity(Engine::Entity* targetEntity);
    void SetPivotPoint(const Engine::vec3& pivot) { m_PivotPoint = pivot; }
    [[nodiscard]] Engine::vec3 GetPivotPoint() const { return m_PivotPoint; }

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

    Engine::f32 GetCameraSpeed() const { return m_CameraSpeed; }
    void SetCameraSpeed(Engine::f32 speed) { m_CameraSpeed = std::max(speed, 1.0f); }
    void IncreaseCameraSpeed() { m_CameraSpeed += 1.0f; }
    void DecreaseCameraSpeed() { m_CameraSpeed = std::max(m_CameraSpeed - 1.0f, 1.0f); }
    void SetZoomSpeed3D(Engine::f32 speed) { m_ZoomSpeed3D = speed; }
    void SetZoomSpeed2D(Engine::f32 speed) { m_ZoomSpeed2D = speed; }
    void SetFOVZoom(bool zoomType) { m_IsFOVZoom = zoomType; }
private:
    void ApplyInput(Engine::f32 dt, bool allowScroll, bool isLMB, bool isMMB, bool isRMB);
private:
    Engine::Scene* m_EditorScene = nullptr;
    entt::entity m_CameraEntity = entt::null;
    Engine::f32 m_CameraSpeed = 100.0f;
    bool m_IsFOVZoom = false;
    Engine::f32 m_ZoomSpeed3D = 2.0f;
    Engine::f32 m_ZoomSpeed2D = 0.5f;
    Engine::vec3 m_ControlRotation = Engine::vec3(0.0f);
    Engine::vec3 m_PivotPoint = Engine::vec3(0.0f, 0.5f, 0.0f);
};
END