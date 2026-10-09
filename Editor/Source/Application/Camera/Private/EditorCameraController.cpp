#include "EditorCameraController.h"

#include "Scene.h"
#include "Entity.h"
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Components/WorldTransformComponent.h"
#include "TransformSystem.h"
#include "CameraSystem.h"

#include <cmath>

namespace Editor
{
    EditorCameraController* EditorCameraController::Create(
        const std::wstring& name, const Engine::CameraComponent& settings)
    {
        auto* instance = new EditorCameraController();

        if (!instance->Initialize(name, settings))
        {
            instance->Release();
            return nullptr;
        }

        return instance;
    }

    bool EditorCameraController::Initialize(const std::wstring& name, const Engine::CameraComponent& settings)
    {
        Engine::SceneCreateDesc sceneDesc;
        sceneDesc.name = name + L"_EditorScene";

        m_EditorScene = Engine::Scene::Create(&sceneDesc);

        if (m_EditorScene == nullptr)
            return false;

        Engine::Entity& entity = m_EditorScene->CreateEntity();
        m_CameraEntity = entity.GetEntityHandle();

        auto& nameComponent = m_EditorScene->AddComponent<Engine::NameComponent>(entity);
        nameComponent.name = name;

        auto& transform = m_EditorScene->AddComponent<Engine::TransformComponent>(entity);
        m_EditorScene->AddComponent<Engine::WorldTransformComponent>(entity);

        auto& camera = m_EditorScene->AddComponent<Engine::CameraComponent>(entity);
        camera = settings;
        camera.IsMainCamera = false;
        camera.InvalidateCache();

        if (camera.CameraType == Engine::ECameraType::Perspective)
        {
            if (!transform.SetLocalPosition(Engine::vec3(0.f, 4.f, -8.f)))
                return false;

            const Engine::f32 pitch = glm::degrees(std::atan2(4.f, 8.f));

            if (!transform.SetEulerRotation(Engine::vec3(pitch, 0.f, 0.f)))
                return false;
        }

        RefreshMatrices();
        return GetCameraBuffer().has_value();
    }

    void EditorCameraController::Free()
    {
        m_CameraEntity = entt::null;

        if (m_EditorScene != nullptr)
        {
            m_EditorScene->Release();
            m_EditorScene = nullptr;
        }

        Engine::Base::Free();
    }

    Engine::Entity* EditorCameraController::GetEntity() const
    {
        if (m_EditorScene == nullptr)
            return nullptr;

        return m_EditorScene->FindEntity(m_CameraEntity);
    }

    Engine::TransformComponent* EditorCameraController::GetTransformComponent() const
    {
        if (GetEntity() == nullptr)
            return nullptr;

        return m_EditorScene->GetRegistry().try_get<Engine::TransformComponent>(m_CameraEntity);
    }

    Engine::CameraComponent* EditorCameraController::GetCameraComponent() const
    {
        if (GetEntity() == nullptr)
            return nullptr;

        return m_EditorScene->GetRegistry().try_get<Engine::CameraComponent>(m_CameraEntity);
    }

    std::optional<CameraBuffer> EditorCameraController::GetCameraBuffer() const
    {
        const auto* camera = GetCameraComponent();

        if (camera == nullptr)
            return std::nullopt;

        return camera->GetCameraBuffer();
    }

    bool EditorCameraController::SetAspectRatio(Engine::f32 aspectRatio)
    {
        if (!std::isfinite(aspectRatio) || aspectRatio <= 0.f)
            return false;

        auto* camera = GetCameraComponent();

        if (camera == nullptr)
            return false;

        if (camera->AspectRatio != aspectRatio)
        {
            camera->AspectRatio = aspectRatio;
            camera->InvalidateCache();
        }

        return true;
    }

    void EditorCameraController::RefreshMatrices()
    {
        if (GetEntity() == nullptr)
            return;

        auto& registry = m_EditorScene->GetRegistry();

        Engine::TransformSystem::UpdateHierarchy(registry);
        Engine::CameraSystem::UpdateCameras(registry);
    }
}