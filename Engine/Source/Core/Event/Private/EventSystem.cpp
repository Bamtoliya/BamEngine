#pragma once

#include "EventSystem.h"
#include "BaseRaycaster.h"
#include "InputManager.h"
#include "GameObject.h"
#include "ComponentRegistry.h"
#include "SceneManager.h"

REGISTER_COMPONENT(EventSystem)

EventSystem* EventSystem::s_Current = nullptr;

#pragma region Constructor&Destructor
EResult EventSystem::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	s_Current = this;

	if (SceneManager::Get().GetCurrentScene())
		SceneManager::Get().GetCurrentScene()->SetEventSystem(this);

	return EResult::Success;
}

EventSystem* EventSystem::Create(void* arg)
{
	EventSystem* instance = new EventSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* EventSystem::Clone(GameObject* owner, void* arg)
{
	EventSystem* clone = Create(arg);
	if (clone) clone->SetOwner(owner);
	return clone;
}

void EventSystem::Free()
{
	if (s_Current == this)
		s_Current = nullptr;

	m_Raycasters.clear();
	__super::Free();
}
#pragma endregion


#pragma region Loop
void EventSystem::FixedUpdate(f32 dt)
{
}

void EventSystem::Update(f32 dt)
{
	if (m_Raycasters.empty())
		return;

	m_CurrentEventData.Position = InputManager::Get().GetLogicalMousePosition();
	m_CurrentEventData.Delta = InputManager::Get().GetMouseDelta();
	// 2. 모든 Raycaster에게 충돌 검사 요청
	vector<RaycastResult> results;
	for (BaseRaycaster* raycaster : m_Raycasters)
	{
		if (raycaster->IsActive())
			raycaster->Raycast(m_CurrentEventData, results);
	}
	// 3. 우선순위에 따라 정렬 (가장 앞에 있는 객체 찾기)
	std::sort(results.begin(), results.end());
	// 4. 유효한 최상위 타겟 선정
	GameObject* target = nullptr;
	for (const auto& res : results)
	{
		if (res.IsValid && res.Target != nullptr)
		{
			target = res.Target;
			break;
		}
	}
	m_CurrentEventData.PointerEnter = target;
	// 5. Hover (Enter / Exit) 처리
	if (m_CurrentHover != target)
	{
		if (m_CurrentHover != nullptr)
		{
			for (auto comp : m_CurrentHover->GetAllComponents())
			{
				if (auto handler = dynamic_cast<IPointerExitHandler*>(comp))
					handler->OnPointerExit(m_CurrentEventData);
			}
		}

		m_CurrentHover = target;

		if (m_CurrentHover != nullptr)
		{
			for (auto comp : m_CurrentHover->GetAllComponents())
			{
				if (auto handler = dynamic_cast<IPointerEnterHandler*>(comp))
					handler->OnPointerEnter(m_CurrentEventData);
			}
		}
	}
	// 6. Pointer Down 처리 (마우스 왼쪽 버튼)
	if (InputManager::Get().IsMouseButtonDown(EMouseButton::Left))
	{
		m_CurrentPress = target;
		m_CurrentEventData.PointerPress = target;
		if (m_CurrentPress != nullptr)
		{
			for (auto comp : m_CurrentPress->GetAllComponents())
			{
				if (auto handler = dynamic_cast<IPointerDownHandler*>(comp))
					handler->OnPointerDown(m_CurrentEventData);
			}
		}
	}
	// 7. Pointer Up 및 Click 처리
	if (InputManager::Get().IsMouseButtonUp(EMouseButton::Left))
	{
		if (m_CurrentPress != nullptr)
		{
			for (auto comp : m_CurrentPress->GetAllComponents())
			{
				if (auto handler = dynamic_cast<IPointerUpHandler*>(comp))
					handler->OnPointerUp(m_CurrentEventData);

				// Down했던 객체와 현재 Up된 객체가 같으면 'Click'으로 인정
				if (target == m_CurrentPress)
				{
					if (auto clickHandler = dynamic_cast<IPointerClickHandler*>(comp))
						clickHandler->OnPointerClick(m_CurrentEventData);
				}
			}
		}
		m_CurrentPress = nullptr;
		m_CurrentEventData.PointerPress = nullptr;
	}
}

void EventSystem::LateUpdate(f32 dt)
{
}

#pragma endregion

#pragma region Raycaster Management
void EventSystem::AddRaycaster(BaseRaycaster* raycaster)
{
	auto it = std::find(m_Raycasters.begin(), m_Raycasters.end(), raycaster);
	if (it == m_Raycasters.end())
		m_Raycasters.push_back(raycaster);
}
void EventSystem::RemoveRaycaster(BaseRaycaster* raycaster)
{
	auto it = std::find(m_Raycasters.begin(), m_Raycasters.end(), raycaster);
	if (it != m_Raycasters.end())
	{
		m_Raycasters.erase(it);
	}
}
#pragma endregion