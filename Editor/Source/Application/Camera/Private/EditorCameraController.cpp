#include "EditorCameraController.h"

#include "Scene.h"
#include "Entity.h"
#include "Components/Core/NameComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "TransformSystem.h"
#include "CameraSystem.h"
#include "InputManager.h"
#include "SelectionManager.h"

#include <glm/gtx/norm.hpp>

EditorCameraController* EditorCameraController::Create(const std::wstring& name, const Engine::CameraComponent& settings)
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
        m_ControlRotation = Engine::vec3(pitch, 0.f, 0.f);

        if (!transform.SetEulerRotation(m_ControlRotation))
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

void EditorCameraController::HandleInput(Engine::f32 dt)
{
    auto* transform = GetTransformComponent();
    auto* camera = GetCameraComponent();
    if (transform == nullptr || camera == nullptr)
        return;

    Engine::InputManager& inputManager = Engine::InputManager::Get();
    const Engine::f32 mouseX = inputManager.GetMouseDelta().x;
    const Engine::f32 mouseY = inputManager.GetMouseDelta().y;
    const Engine::vec2 mouseScrollDelta = inputManager.GetMouseScrollDelta();
    const Engine::f32 rotationSpeed = 0.3f;
    Engine::f32 movementSpeed = m_CameraSpeed * dt;
    Engine::vec3 moveDirection = Engine::vec3(0.0f);
    bool changed = false;

    if (KEY_PRESSED("LShift"))
        movementSpeed *= 2.0f;

    if (mouseScrollDelta.y != 0.0f)
    {
        if (IsPerspective())
        {
            if (m_IsFOVZoom)
            {
                const Engine::f32 newFOV = std::clamp(camera->FOV - (mouseScrollDelta.y * m_ZoomSpeed3D), 1.0f, 170.0f);
                if (camera->FOV != newFOV)
                {
                    camera->FOV = newFOV;
                    camera->InvalidateCache();
                    changed = true;
                }
            }
            else
            {
                const Engine::vec3 forward = transform->GetLocalForward();
                if (transform->SetLocalPosition(transform->position + forward * (mouseScrollDelta.y * m_ZoomSpeed3D)))
                    changed = true;
            }
        }
        else
        {
            const Engine::f32 newOrthoSize = std::max(camera->OrthographicSize - (mouseScrollDelta.y * m_ZoomSpeed2D), 0.1f);
            if (camera->OrthographicSize != newOrthoSize)
            {
                camera->OrthographicSize = newOrthoSize;
                camera->InvalidateCache();
                changed = true;
            }
        }
    }

    if (IsPerspective())
    {
        if (MOUSE_BUTTON_PRESSED("Right"))
        {
            m_ControlRotation.y += mouseX * rotationSpeed;
            m_ControlRotation.x = std::clamp(m_ControlRotation.x + mouseY * rotationSpeed, -89.0f, 89.0f);
            m_ControlRotation.z = 0.0f;

            if (transform->SetEulerRotation(m_ControlRotation))
                changed = true;
        }

        if (KEY_PRESSED("W")) moveDirection += Engine::vec3(0.0f, 0.0f, 1.0f);
        if (KEY_PRESSED("S")) moveDirection -= Engine::vec3(0.0f, 0.0f, 1.0f);
        if (KEY_PRESSED("A")) moveDirection -= Engine::vec3(1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED("D")) moveDirection += Engine::vec3(1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED("Q")) moveDirection += Engine::vec3(0.0f, 1.0f, 0.0f);
        if (KEY_PRESSED("E")) moveDirection -= Engine::vec3(0.0f, 1.0f, 0.0f);

        if (KEY_PRESSED(Engine::EKeyCode::F))
        {
            FocusEntity(SelectionManager::Get().GetPrimarySelectedEntity());
            changed = true;
        }
    }
    else
    {
        if (KEY_PRESSED("W")) moveDirection += Engine::vec3(0.0f, 1.0f, 0.0f);
        if (KEY_PRESSED("S")) moveDirection += Engine::vec3(0.0f, -1.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::A)) moveDirection += Engine::vec3(-1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::D)) moveDirection += Engine::vec3(1.0f, 0.0f, 0.0f);
    }

    if (KEY_PRESSED("=")) IncreaseCameraSpeed();
    if (KEY_PRESSED("-")) DecreaseCameraSpeed();

    if (glm::length2(moveDirection) > 0.001f)
    {
        moveDirection = glm::normalize(moveDirection);
        if (transform->TranslateLocal(moveDirection * movementSpeed))
            changed = true;
    }

    if (changed)
        RefreshMatrices();
}

void EditorCameraController::SetControlRotation(const Engine::vec3& eulerAngles)
{
    auto* transform = GetTransformComponent();
    if (transform == nullptr)
        return;

    m_ControlRotation = eulerAngles;
    m_ControlRotation.x = std::clamp(m_ControlRotation.x, -89.0f, 89.0f);
    m_ControlRotation.z = 0.0f;

    if (transform->SetEulerRotation(m_ControlRotation))
        RefreshMatrices();
}

void EditorCameraController::SyncControlRotationFromTransform()
{
    if (const auto* transform = GetTransformComponent())
    {
        m_ControlRotation = transform->eulerRotation;
        m_ControlRotation.x = std::clamp(m_ControlRotation.x, -89.0f, 89.0f);
        m_ControlRotation.z = 0.0f;
    }
}

void EditorCameraController::FocusEntity(Engine::Entity* targetEntity)
{
    if (targetEntity == nullptr)
        return;

    Engine::Scene* scene = targetEntity->GetScene();
    auto* transform = GetTransformComponent();
    if (scene == nullptr || transform == nullptr)
        return;

    auto& registry = scene->GetRegistry();
    const entt::entity handle = targetEntity->GetEntityHandle();
    if (!registry.valid(handle))
        return;

    glm::mat4 targetWorld(1.0f);
    if (const auto* worldTransform = registry.try_get<Engine::WorldTransformComponent>(handle))
        targetWorld = worldTransform->worldMatrix;
    else if (!Engine::TransformSystem::TryGetWorldMatrix(registry, handle, targetWorld))
        return;

    const Engine::vec3 targetPos = Engine::vec3(targetWorld[3]);
    Engine::vec3 cameraPos = transform->position;
    Engine::vec3 forwardDir = targetPos - cameraPos;
    const Engine::f32 distance = glm::length(forwardDir);

    if (distance > 1e-5f)
        forwardDir /= distance;
    else
        forwardDir = transform->GetLocalForward();

    if (distance >= 15.0f)
    {
        cameraPos = targetPos - forwardDir * 10.0f;
        static_cast<void>(transform->SetLocalPosition(cameraPos));
    }

    const Engine::f32 pitch = -glm::degrees(std::asin(std::clamp(forwardDir.y, -1.0f, 1.0f)));
    const Engine::f32 yaw = glm::degrees(std::atan2(forwardDir.x, forwardDir.z));
    SetControlRotation(Engine::vec3(std::clamp(pitch, -89.0f, 89.0f), yaw, 0.0f));
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

bool EditorCameraController::IsPerspective() const
{
    const auto* camera = GetCameraComponent();
    return camera != nullptr && camera->CameraType == Engine::ECameraType::Perspective;
}

bool EditorCameraController::IsOrthographic() const
{
    const auto* camera = GetCameraComponent();
    return camera != nullptr && camera->CameraType == Engine::ECameraType::Orthographic;
}

void EditorCameraController::SetPerspective(bool perspective)
{
    auto* camera = GetCameraComponent();
    if (camera == nullptr)
        return;

    const auto targetType = perspective ? Engine::ECameraType::Perspective : Engine::ECameraType::Orthographic;
    if (camera->CameraType != targetType)
    {
        camera->CameraType = targetType;
        camera->InvalidateCache();
        RefreshMatrices();
    }
}

void EditorCameraController::SetOrthographic(bool orthographic)
{
    SetPerspective(!orthographic);
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