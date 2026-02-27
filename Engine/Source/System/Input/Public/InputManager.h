#pragma once

#include "Base.h"
#include "InputMap.h"

#define KEY_DOWN(key) InputManager::Get().IsKeyButtonDown(key)
#define KEY_UP(key) InputManager::Get().IsKeyButtonUp(key)
#define KEY_PRESSED(key) InputManager::Get().IsKeyButtonPressed(key)

#define MOUSE_BUTTON_DOWN(button) InputManager::Get().IsMouseButtonDown(button)
#define MOUSE_BUTTON_UP(button) InputManager::Get().IsMouseButtonUp(button)
#define MOUSE_BUTTON_PRESSED(button) InputManager::Get().IsMouseButtonPressed(button)

BEGIN(Engine)
class ENGINE_API InputManager : public Base
{
	DECLARE_SINGLETON(InputManager)
#pragma region Constructor&Destructor
	private:
	InputManager() {}
	virtual ~InputManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void Update(f32 dt);
#pragma endregion

#pragma region Keyboard
public:
	bool IsKeyButtonDown(EKeyCode key);
	bool IsKeyButtonUp(EKeyCode key);
	bool IsKeyButtonPressed(EKeyCode key);
public:
	bool IsKeyButtonDown(const string& key);
	bool IsKeyButtonUp(const string& key);
	bool IsKeyButtonPressed(const string& key);
#pragma endregion

#pragma region Mouse
public:
	bool IsMouseButtonDown(EMouseButton button);
	bool IsMouseButtonUp(EMouseButton button);	
	bool IsMouseButtonPressed(EMouseButton button);
public:
	bool IsMouseButtonDown(const string& key);
	bool IsMouseButtonUp(const string& key);
	bool IsMouseButtonPressed(const string& key);
public:
	vec2 GetMousePosition() const { return m_MousePosition; }
	vec2 GetMouseDelta() const { return m_MouseDelta; }
#pragma endregion

#pragma region Member Variables
public:
	array<uint8, SDL_SCANCODE_COUNT> m_CurrentKeyStates;
	array<uint8, SDL_SCANCODE_COUNT> m_PreviousKeyStates;

	array<uint8, static_cast<uint32>(EMouseButton::Count)> m_CurrentMouseButtonStates;
	array<uint8, static_cast<uint32>(EMouseButton::Count)> m_PreviousMouseButtonStates;

	vec2 m_MousePosition = vec2(0.f);
	vec2 m_MouseDelta = vec2(0.f);
#pragma endregion
};
END