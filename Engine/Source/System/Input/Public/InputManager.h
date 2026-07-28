#pragma once

#include "Base.h"
#include "InputMap.h"

using namespace std;

#define KEY_DOWN(key) InputManager::Get().IsKeyButtonDown(key)
#define KEY_UP(key) InputManager::Get().IsKeyButtonUp(key)
#define KEY_PRESSED(key) InputManager::Get().IsKeyButtonPressed(key)

#define MOUSE_BUTTON_DOWN(button) InputManager::Get().IsMouseButtonDown(button)
#define MOUSE_BUTTON_UP(button) InputManager::Get().IsMouseButtonUp(button)
#define MOUSE_BUTTON_PRESSED(button) InputManager::Get().IsMouseButtonPressed(button)

#define MOUSE_BUTTON_DOUBLE_CLICK(button) InputManager::Get().IsMouseButtonDoubleClicked(button)
#define MOUSE_BUTTON_DRAGGING(button) InputManager::Get().IsMouseButtonDragging(button)

#define MOUSE_POSITION InputManager::Get().GetMousePosition()

#define MOUSE_SCROLL InputManager::Get().GetMouseScrollDelta()

struct MouseStateData
{
	f32 LastDownTime = -1.0f;
	vec2 DownPosition = vec2(0.f);
	bool IsDragging = false;
};

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

	bool IsMouseButtonDoubleClicked(EMouseButton button);
	bool IsMouseButtonDragging(EMouseButton button);
public:
	bool IsMouseButtonDown(const string& key);
	bool IsMouseButtonUp(const string& key);
	bool IsMouseButtonPressed(const string& key);

	bool IsMouseButtonDoubleClicked(const string& key);
	bool IsMouseButtonDragging(const string& key);
public:
	vec2 GetMousePosition() const { return m_MousePosition; }
	vec2 GetMouseDelta() const { return m_MouseDelta; }
	vec2 GetMouseScrollDelta() const { return m_MouseScrollDelta; }
	f32 GetMouseScrollDeltaX() const { return m_MouseScrollDelta.x; }
	f32 GetMouseScrollDeltaY() const { return m_MouseScrollDelta.y; }
public:
	vec2 GetLogicalMousePosition() const
	{
		if (m_ViewportRect.z > 0.0f && m_ViewportRect.w > 0.0f &&
			m_TargetResolution.x > 0.0f && m_TargetResolution.y > 0.0f)
		{
			f32 u = (m_MousePosition.x - m_ViewportRect.x) / m_ViewportRect.z;
			f32 v = (m_MousePosition.y - m_ViewportRect.y) / m_ViewportRect.w;
			return vec2(u * m_TargetResolution.x, v * m_TargetResolution.y);
		}
		return m_MousePosition;
	}
	void SetViewportRect(const vec4& viewportRect) { m_ViewportRect = viewportRect; }
	vec4 GetViewportRect() const { return m_ViewportRect; }
	void SetTargetResolution(const vec2& targetResolution) { m_TargetResolution = targetResolution; }
	vec2 GetTargetResolution() const { return m_TargetResolution; }
public:
	void ProcessEvent(const SDL_Event& event);
#pragma endregion

#pragma region Member Variables
private:
	array<uint8, SDL_SCANCODE_COUNT> m_CurrentKeyStates;
	array<uint8, SDL_SCANCODE_COUNT> m_PreviousKeyStates;

	array<uint8, static_cast<uint32>(EMouseButton::Count)> m_CurrentMouseButtonStates;
	array<uint8, static_cast<uint32>(EMouseButton::Count)> m_PreviousMouseButtonStates;

	vec2 m_MousePosition = vec2(0.f);
	vec2 m_MouseDelta = vec2(0.f);

	vec2 m_AccmulatedMouseScrollDelta = vec2(0.f);
	vec2 m_MouseScrollDelta = vec2(0.f);

	f32 m_CurrentTime = 0.0f;
	f32 m_DoubleClickThreshold = 0.3f;
	f32 m_DragThreshold = 3.0f;

	vec4 m_ViewportRect = vec4(0.f); // 뷰포트 영역 (x, y, width, height)
	vec2 m_TargetResolution = vec2(1920.f, 1080.f); // 타겟 해상도 (논리적 좌표계 기준)

	array<MouseStateData, static_cast<uint32>(EMouseButton::Count)> m_MouseButtonStatesData = {};
	array<bool, static_cast<uint32>(EMouseButton::Count)> m_CurrentDoubleClicks = {false,};
#pragma endregion
};
END