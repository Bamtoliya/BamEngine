#include "EditorCameraController.h"

#include "Scene.h"
#include "Entity.h"
#include "Components/Core/NameComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "TransformSystem.h"
#include "CameraSystem.h"
#include "InputManager.h"

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

void EditorCameraController::HandleInput(Engine::f32 dt, bool allowScroll)
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

    const bool isAlt = KEY_PRESSED(Engine::EKeyCode::LAlt) || KEY_PRESSED(Engine::EKeyCode::RAlt);
    const bool isLMB = MOUSE_BUTTON_PRESSED(Engine::EMouseButton::Left);
    const bool isMMB = MOUSE_BUTTON_PRESSED(Engine::EMouseButton::Middle);
    const bool isRMB = MOUSE_BUTTON_PRESSED(Engine::EMouseButton::Right);

    // 카메라와 피벗 간의 거리
    Engine::vec3 toCamera = transform->position - m_PivotPoint;
    float dist = glm::length(toCamera);
    if (dist < 0.1f) dist = 0.1f;

    // 1. 마우스 휠 줌 (뷰포트에 호버되어 있을 때만 허용)
    if (allowScroll && mouseScrollDelta.y != 0.0f)
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
        // 2. [Alt + LMB] 타깃 중심 궤도 회전 (Orbit)
        if (isAlt && isLMB)
        {
            m_ControlRotation.y += mouseX * rotationSpeed;
            m_ControlRotation.x = std::clamp(m_ControlRotation.x + mouseY * rotationSpeed, -89.0f, 89.0f);
            m_ControlRotation.z = 0.0f;

            const float pitchRad = glm::radians(m_ControlRotation.x);
            const float yawRad = glm::radians(m_ControlRotation.y);
            Engine::vec3 forward;
            forward.x = sinf(yawRad) * cosf(pitchRad);
            forward.y = -sinf(pitchRad);
            forward.z = cosf(yawRad) * cosf(pitchRad);

            (void)transform->SetLocalPosition(m_PivotPoint - forward * dist);
            (void)transform->SetEulerRotation(m_ControlRotation);
            changed = true;
        }
        // 3. [Alt + MMB] 또는 [MMB 단독] 시점 평면 패닝 (Pan)
        else if (isMMB)
        {
            const float panFactor = dist * 0.002f;
            const Engine::vec3 right = transform->GetLocalRight();
            const Engine::vec3 up = transform->GetLocalUp();
            const Engine::vec3 panDelta = (-right * mouseX + up * mouseY) * panFactor;

            (void)transform->SetLocalPosition(transform->position + panDelta);
            m_PivotPoint += panDelta;
            changed = true;
        }
        // 4. [Alt + RMB] 부드러운 줌 드래그
        else if (isAlt && isRMB)
        {
            const float zoomDelta = (mouseX - mouseY) * 0.02f * dist;
            dist = std::max(dist + zoomDelta, 0.2f);
            (void)transform->SetLocalPosition(m_PivotPoint - transform->GetLocalForward() * dist);
            changed = true;
        }
        // 5. [RMB 단독] 1인칭 프리 플라이 회전
        else if (isRMB)
        {
            m_ControlRotation.y += mouseX * rotationSpeed;
            m_ControlRotation.x = std::clamp(m_ControlRotation.x + mouseY * rotationSpeed, -89.0f, 89.0f);
            m_ControlRotation.z = 0.0f;

            if (transform->SetEulerRotation(m_ControlRotation))
                changed = true;

            m_PivotPoint = transform->position + transform->GetLocalForward() * dist;
        }

        // WASDQE 1인칭 이동
        if (KEY_PRESSED(Engine::EKeyCode::W)) moveDirection += Engine::vec3(0.0f, 0.0f, 1.0f);
        if (KEY_PRESSED(Engine::EKeyCode::S)) moveDirection -= Engine::vec3(0.0f, 0.0f, 1.0f);
        if (KEY_PRESSED(Engine::EKeyCode::A)) moveDirection -= Engine::vec3(1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::D)) moveDirection += Engine::vec3(1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::Q)) moveDirection += Engine::vec3(0.0f, 1.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::E)) moveDirection -= Engine::vec3(0.0f, 1.0f, 0.0f);
    }

    if (glm::length2(moveDirection) > 0.001f)
    {
        moveDirection = glm::normalize(moveDirection);
        if (transform->TranslateLocal(moveDirection * movementSpeed))
        {
            m_PivotPoint += moveDirection * movementSpeed;
            changed = true;
        }
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

// Editor/Source/Application/Camera/Private/EditorCameraController.cpp 203~243행 교체

void EditorCameraController::FocusEntity(Engine::Entity* targetEntity)
{
    if (targetEntity == nullptr) return;
    Engine::Scene* scene = targetEntity->GetScene();
    auto* transform = GetTransformComponent();
    if (scene == nullptr || transform == nullptr) return;
    auto& registry = scene->GetRegistry();
    const entt::entity handle = targetEntity->GetEntityHandle();
    if (!registry.valid(handle)) return;
    glm::mat4 targetWorld(1.0f);
    if (const auto* worldTransform = registry.try_get<Engine::WorldTransformComponent>(handle))
        targetWorld = worldTransform->worldMatrix;
    else if (!Engine::TransformSystem::TryGetWorldMatrix(registry, handle, targetWorld))
        return;
    const Engine::vec3 targetPos = Engine::vec3(targetWorld[3]);
    Engine::vec3 cameraPos = transform->position;
    if (IsOrthographic())
    {
        cameraPos.x = targetPos.x;
        cameraPos.y = targetPos.y;
        (void)transform->SetLocalPosition(cameraPos);
    }
    else
    {
        Engine::vec3 forwardDir = targetPos - cameraPos;
        const Engine::f32 distance = glm::length(forwardDir);
        forwardDir = (distance > 1e-5f) ? (forwardDir / distance) : transform->GetLocalForward();
        // 거리 제한 없이 항상 타깃 앞 적정 거리(8.0f)로 카메라 배치
        const Engine::f32 focusDistance = 8.0f;
        cameraPos = targetPos - forwardDir * focusDistance;
        (void)transform->SetLocalPosition(cameraPos);
        const Engine::f32 pitch = -glm::degrees(std::asin(std::clamp(forwardDir.y, -1.0f, 1.0f)));
        const Engine::f32 yaw = glm::degrees(std::atan2(forwardDir.x, forwardDir.z));
        m_ControlRotation = Engine::vec3(std::clamp(pitch, -89.0f, 89.0f), yaw, 0.0f);
        (void)transform->SetEulerRotation(m_ControlRotation);
    }
    RefreshMatrices(); // 항상 뷰/투영 행렬 즉시 갱신
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