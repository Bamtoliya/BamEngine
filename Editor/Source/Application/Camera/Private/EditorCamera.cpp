#pragma once
#include "EditorCamera.h"
#include "InputManager.h"
#include "CameraManager.h"
#include "SelectionManager.h"

BEGIN(Editor)

#pragma region Constructor&Destructor
Engine::EResult EditorCamera::Initialize(void* arg)
{
    m_Transform = AddComponent<Engine::Transform>();
    Safe_AddRef(m_Transform);

    m_Camera = AddComponent<Engine::Camera>(arg);
    Safe_AddRef(m_Camera);

	CameraManager::Get().RemoveCamera(m_Camera);

    return Engine::EResult::Success;
}

EditorCamera* EditorCamera::Create(void* arg)
{
    EditorCamera* instance = new EditorCamera();
    if (IsFailure(instance->Initialize(arg)))
    {
        Safe_Release(instance);
        return nullptr;
    }

    return instance;
}

void EditorCamera::Free()
{
    Safe_Release(m_Transform);
    Safe_Release(m_Camera);
    __super::Free();
}

void EditorCamera::FixedUpdate(Engine::f32 dt)
{
    __super::FixedUpdate(dt);
}

void EditorCamera::Update(Engine::f32 dt)
{
    __super::Update(dt);
}

void EditorCamera::LateUpdate(Engine::f32 dt)
{
    __super::LateUpdate(dt);
}

void EditorCamera::HandleInput(Engine::f32 dt)
{
    Engine::InputManager& inputManager = Engine::InputManager::Get();
    Engine::f32 mouseX = inputManager.GetMouseDelta().x;
    Engine::f32 mouseY = inputManager.GetMouseDelta().y;
    Engine::vec2 mouseScrollDelta = inputManager.GetMouseScrollDelta();
    Engine::f32 rotationSpeed = 0.3f;
    Engine::f32 movementSpeed = m_CameraSpeed * dt;
    Engine::vec3 moveDirection = Engine::vec3(0.0f);

    if (KEY_PRESSED("LShift"))
    {
        movementSpeed *= 2.0f;
    }

    if (mouseScrollDelta.y != 0.0f)
    {
        if (m_Camera->GetIsPerspective())
        {
            if (m_IsFOVZoom)
            {
                Engine::f32 currentFOV = m_Camera->GetFOV();
                Engine::f32 newFOV = currentFOV - (mouseScrollDelta.y * m_ZoomSpeed3D);
                newFOV = std::clamp(newFOV, 1.0f, 170.0f);
                m_Camera->SetFOV(newFOV);
            }
            else
            {
                Engine::vec3 pos = m_Transform->GetWorldPosition();
                Engine::vec3 forward = m_Transform->GetForward();
                pos -= forward * (mouseScrollDelta.y * m_ZoomSpeed3D);
                m_Transform->SetPosition(pos);
            }
        }
        else
        {
            Engine::f32 currentOrthoSize = m_Camera->GetOrthoSize();
            Engine::f32 newOrthoSize = currentOrthoSize - (mouseScrollDelta.y * m_ZoomSpeed2D);
            newOrthoSize = std::max(newOrthoSize, 0.1f);
            m_Camera->SetOrthoSize(newOrthoSize);
        }
    }

    if (m_Camera->GetIsPerspective())
    {
        if (MOUSE_BUTTON_PRESSED("Right"))
        {
            m_ControlRotation.y += mouseX * rotationSpeed; // Yaw
            m_ControlRotation.x += mouseY * rotationSpeed; // Pitch

            // 짐벌락 및 화면 뒤집힘 완벽 차단
            m_ControlRotation.x = std::clamp(m_ControlRotation.x, -89.0f, 89.0f);

            // Transform에 Quat으로 변환하여 주입
            Engine::quat newRot = glm::quat(glm::radians(m_ControlRotation));
            m_Transform->SetRotation(newRot);
            //Engine::vec3 currentRot = m_Transform->GetLocalRotationEuler();
            //currentRot.y += mouseX * rotationSpeed;
            //currentRot.x += mouseY * rotationSpeed;
            //currentRot.x = std::clamp(currentRot.x, -89.0f, 89.0f);
            //currentRot.z = 0.0f;
            //m_Transform->SetRotation(currentRot);
        }

        if (KEY_PRESSED("W")) moveDirection += Engine::vec3(0.0f, 0.0f, 1.0f);
        if (KEY_PRESSED("S")) moveDirection -= Engine::vec3(0.0f, 0.0f, 1.0f);
        if (KEY_PRESSED("A")) moveDirection -= Engine::vec3(1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED("D")) moveDirection += Engine::vec3(1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED("Q")) moveDirection += Engine::vec3(0.0f, 1.0f, 0.0f);
        if (KEY_PRESSED("E")) moveDirection -= Engine::vec3(0.0f, 1.0f, 0.0f);

        if (KEY_PRESSED(EKeyCode::F))
        {
            FocusGameObject(SelectionManager::Get().GetPrimarySelection());
        }
    }
    else
    {
        if (KEY_PRESSED("W")) moveDirection += Engine::vec3(0.0f, 1.0f, 0.0f);
        if (KEY_PRESSED("S")) moveDirection += Engine::vec3(0.0f, -1.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::A)) moveDirection += Engine::vec3(-1.0f, 0.0f, 0.0f);
        if (KEY_PRESSED(Engine::EKeyCode::D)) moveDirection += Engine::vec3(1.0f, 0.0f, 0.0f);
    }

    if (KEY_PRESSED("="))
    {
        IncreaseCameraSpeed();
    }

    if (KEY_PRESSED("-"))
    {
        DecreaseCameraSpeed();
    }

    if (glm::length2(moveDirection) > 0.001f)
    {
        moveDirection = glm::normalize(moveDirection);
        m_Transform->Translate(moveDirection * movementSpeed);
    }
}
void EditorCamera::SetControlRotation(const Engine::vec3& eulerAngles)
{
    m_ControlRotation = eulerAngles;
    m_ControlRotation.x = std::clamp(m_ControlRotation.x, -89.0f, 89.0f);

    Engine::quat newRot = glm::quat(glm::radians(m_ControlRotation));
    m_Transform->SetRotation(newRot);
}
void EditorCamera::FocusGameObject(GameObject* gameObject)
{
    if (!gameObject) return;
    Engine::Transform* targetTransform = gameObject->GetTransform();
    if (!targetTransform) return;

    Engine::vec3 targetPos = targetTransform->GetWorldPosition();
    Engine::vec3 cameraPos = m_Transform->GetWorldPosition();
    Engine::vec3 forwardDir = targetPos - cameraPos;
    Engine::f32 distance = glm::length(forwardDir);

    if (distance > 1e-5f)
        forwardDir /= distance;

    if (distance >= 15.f)
    {
        cameraPos = targetPos - forwardDir * 10.0f;
        m_Transform->SetPosition(cameraPos);
    }

    Engine::f32 pitch = -glm::degrees(std::asin(forwardDir.y));
    Engine::f32 yaw = glm::degrees(std::atan2(forwardDir.x, forwardDir.z));

    Engine::vec3 newEuler(std::clamp(pitch, -89.0f, 89.0f), yaw, 0.0f);

    SetControlRotation(newEuler);
}
#pragma endregion

END