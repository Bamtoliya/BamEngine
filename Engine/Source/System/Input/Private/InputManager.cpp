#pragma once

#include "InputManager.h"
#include "SDL3/SDL.h"

IMPLEMENT_SINGLETON(InputManager)

#pragma region Constructor&Destructor
EResult InputManager::Initialize(void* arg)
{
	return EResult::Success;
}
void InputManager::Free()
{
	__super::Free();
}
#pragma endregion

#pragma region Loop
void InputManager::Update(f32 dt)
{
	m_CurrentTime += dt;
	m_PreviousKeyStates = m_CurrentKeyStates;
	m_PreviousMouseButtonStates = m_CurrentMouseButtonStates;

	int32 sdlNumKeys;
	const bool* sdlKeyStates = SDL_GetKeyboardState(&sdlNumKeys);
	if (sdlKeyStates)
	{
		for (int32 i = 0; i < sdlNumKeys; ++i)
		{
			EKeyCode engineKey = SDLKeyToEngineKeyCode(static_cast<SDL_Scancode>(i));
			if (engineKey != EKeyCode::Unknown)
			{
				uint32 engineKeyIndex = static_cast<uint32>(engineKey);
				if (engineKeyIndex < static_cast<uint32>(EKeyCode::Count))
				{
					m_CurrentKeyStates[engineKeyIndex] = sdlKeyStates[i] ? 1 : 0;
				}
			}
		}
	}

	f32 mouseX, mouseY;
	uint32 mouseMask = SDL_GetMouseState(&mouseX, &mouseY);
	vec2 newMousePosition = vec2(mouseX, mouseY);

	for (uint32 i = 1; i < static_cast<uint32>(EMouseButton::Count); ++i)
	{
		EMouseButton engineButton = SDLButtonToEngineMouseButton(i);
		if (engineButton != EMouseButton::Count)
		{
			uint32 engineButtonIndex = static_cast<uint32>(engineButton);
			m_CurrentMouseButtonStates[engineButtonIndex] = (mouseMask & SDL_BUTTON_MASK(i)) != 0;
		}
	}

	for (uint32 i = 0; i < static_cast<uint32>(EMouseButton::Count); ++i)
	{
		m_CurrentDoubleClicks[i] = false; // 매 프레임 초기화

		bool isDownNow = m_CurrentMouseButtonStates[i] && !m_PreviousMouseButtonStates[i];
		bool isUpNow = !m_CurrentMouseButtonStates[i] && m_PreviousMouseButtonStates[i];
		bool isPressed = m_CurrentMouseButtonStates[i];

		if (isDownNow)
		{
			// [더블클릭 체크]
			f32 timeSinceLastDown = m_CurrentTime - m_MouseButtonStatesData[i].LastDownTime;
			if (m_MouseButtonStatesData[i].LastDownTime >= 0.0f && timeSinceLastDown <= m_DoubleClickThreshold)
			{
				m_CurrentDoubleClicks[i] = true;
				m_MouseButtonStatesData[i].LastDownTime = -1.0f; // 3번 연속 클릭 방지용 초기화
			}
			else
			{
				m_MouseButtonStatesData[i].LastDownTime = m_CurrentTime;
			}

			// 눌린 위치 기록 (드래그 기준점)
			m_MouseButtonStatesData[i].DownPosition = newMousePosition;
		}

		// [드래그 체크]
		if (isPressed)
		{
			if (!m_MouseButtonStatesData[i].IsDragging)
			{
				// 누른 위치로부터 임계값 이상 멀어졌다면 드래그 시작
				if (glm::distance(newMousePosition, m_MouseButtonStatesData[i].DownPosition) >= m_DragThreshold)
				{
					m_MouseButtonStatesData[i].IsDragging = true;
				}
			}
		}
		else if (isUpNow)
		{
			// 버튼을 떼면 드래그 상태 해제
			m_MouseButtonStatesData[i].IsDragging = false;
		}
	}

	m_MouseDelta = newMousePosition - m_MousePosition;
	m_MousePosition = newMousePosition;
	m_MouseScrollDelta = m_AccmulatedMouseScrollDelta;
	m_AccmulatedMouseScrollDelta = vec2(0.f);
}
#pragma endregion

#pragma region Keyboard
bool InputManager::IsKeyButtonDown(EKeyCode key)
{
	uint32 keyIndex = static_cast<uint32>(key);
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return m_CurrentKeyStates[keyIndex] && !m_PreviousKeyStates[keyIndex];
}
bool InputManager::IsKeyButtonUp(EKeyCode key)
{
	uint32 keyIndex = static_cast<uint32>(key);
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return !m_CurrentKeyStates[keyIndex] && m_PreviousKeyStates[keyIndex];
}
bool InputManager::IsKeyButtonPressed(EKeyCode key)
{
	uint32 keyIndex = static_cast<uint32>(key);
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return m_CurrentKeyStates[keyIndex];
}

bool InputManager::IsKeyButtonDown(const string& key)
{
	uint32 keyIndex = static_cast<uint32>(StringToEngineKeyCode(key));
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return m_CurrentKeyStates[keyIndex] && !m_PreviousKeyStates[keyIndex];
}

bool InputManager::IsKeyButtonUp(const string& key)
{
	uint32 keyIndex = static_cast<uint32>(StringToEngineKeyCode(key));
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return !m_CurrentKeyStates[keyIndex] && m_PreviousKeyStates[keyIndex];
}

bool InputManager::IsKeyButtonPressed(const string& key)
{
	uint32 keyIndex = static_cast<uint32>(StringToEngineKeyCode(key));
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return m_CurrentKeyStates[keyIndex];
}

#pragma endregion

#pragma region Mouse
bool InputManager::IsMouseButtonDown(EMouseButton button)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count))
	{
		return false;
	}
	return m_CurrentMouseButtonStates[buttonIndex] && !m_PreviousMouseButtonStates[buttonIndex];
}
bool InputManager::IsMouseButtonUp(EMouseButton button)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count))
	{
		return false;
	}
	return !m_CurrentMouseButtonStates[buttonIndex] && m_PreviousMouseButtonStates[buttonIndex];
}
bool InputManager::IsMouseButtonPressed(EMouseButton button)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count))
	{
		return false;
	}
	return m_CurrentMouseButtonStates[buttonIndex];
}
bool InputManager::IsMouseButtonDoubleClicked(EMouseButton button)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count)) return false;
	return m_CurrentDoubleClicks[buttonIndex];
}
bool InputManager::IsMouseButtonDragging(EMouseButton button)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count)) return false;
	return m_MouseButtonStatesData[buttonIndex].IsDragging;
}
bool InputManager::IsMouseButtonDown(const string& key)
{
	return IsMouseButtonDown(StringToEngineMouseButton(key));
}
bool InputManager::IsMouseButtonUp(const string& key)
{
	return IsMouseButtonUp(StringToEngineMouseButton(key));
}
bool InputManager::IsMouseButtonPressed(const string& key)
{
	return IsMouseButtonPressed(StringToEngineMouseButton(key));
}
bool InputManager::IsMouseButtonDoubleClicked(const string& key)
{
	return IsMouseButtonDoubleClicked(StringToEngineMouseButton(key));
}
bool InputManager::IsMouseButtonDragging(const string& key)
{
	return IsMouseButtonDragging(StringToEngineMouseButton(key));
}
void InputManager::ProcessEvent(const SDL_Event& event)
{
	if (event.type == SDL_EVENT_MOUSE_WHEEL)
	{
		m_AccmulatedMouseScrollDelta.x += static_cast<f32>(event.wheel.x);
		m_AccmulatedMouseScrollDelta.y += static_cast<f32>(event.wheel.y);
	}
}
#pragma endregion
