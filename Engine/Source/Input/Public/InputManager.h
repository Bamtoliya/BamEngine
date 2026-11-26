#pragma once

#include "Base.h"
#include "InputActionSettings.h"
#include "InputMap.h"
#include "KeyChord.h"

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
	f32 MaxDragDistance = 0.0f;
};

BEGIN(Engine)
struct InputActionSettings;

struct InputActionMapMouseInput
{
	uint32 AllowedButtons = ~uint32{ 0 };
	f32 ScrollDeltaY = 0.0f;
	vec2 MotionDelta = vec2(0.0f);
};

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
	bool IsKeyButtonRepeated(EKeyCode key) const;
public:
	bool IsKeyButtonDown(const string& key);
	bool IsKeyButtonUp(const string& key);
	bool IsKeyButtonPressed(const string& key);
	bool IsKeyButtonRepeated(const string& key) const;
	bool IsCtrlPressed() { return IsKeyButtonPressed(EKeyCode::LCtrl) || IsKeyButtonPressed(EKeyCode::RCtrl); }
	bool IsShiftPressed() { return IsKeyButtonPressed(EKeyCode::LShift) || IsKeyButtonPressed(EKeyCode::RShift); }
	bool IsAltPressed() { return IsKeyButtonPressed(EKeyCode::LAlt) || IsKeyButtonPressed(EKeyCode::RAlt); }
#pragma endregion

#pragma region Mouse
public:
	struct MouseWheelSample
	{
		vec2 ScreenPosition;
		f32 DeltaY;
	};

	const std::vector<MouseWheelSample>& GetMouseWheelSamples() const { return m_MouseWheelSamples; }
public:
	bool IsMouseButtonDown(EMouseButton button);
	bool IsMouseButtonUp(EMouseButton button);	
	bool IsMouseButtonPressed(EMouseButton button);

	bool IsMouseButtonDoubleClicked(EMouseButton button);
	bool IsMouseButtonDragging(EMouseButton button, f32 threshold = 3.0f);
public:
	bool IsMouseButtonDown(const string& key);
	bool IsMouseButtonUp(const string& key);
	bool IsMouseButtonPressed(const string& key);

	bool IsMouseButtonDoubleClicked(const string& key);
	bool IsMouseButtonDragging(const string& key);
public:
	// Window-local coordinates for runtime viewports; desktop coordinates for detached editor panels.
	vec2 GetMousePosition() const { return m_MousePosition; }
	vec2 GetScreenMousePosition() const { return m_ScreenMousePosition; }
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

#pragma region Input Actions
public:
	bool SetInputActionSettings(const InputActionSettings& settings);
	bool RebindAction(std::string_view mapId, std::string_view actionId, std::string_view newBindingKey);
	bool RebindAction(std::string_view mapId, std::string_view actionId, std::size_t bindingIndex, std::string_view newBindingKey);
	bool ResetActionBindingsToDefault(std::string_view mapId, std::string_view actionId);
	void ResetAllBindingsToDefault();
	bool SaveBindingsToFile(const std::filesystem::path& filePath) const;
	bool LoadBindingsFromFile(const std::filesystem::path& filePath);
	std::vector<std::string> GetActionBindings(std::string_view mapId, std::string_view actionId) const;
	const InputActionSettings& GetCurrentInputActionSettings() const { return m_CurrentActionSettings; }
	const InputActionSettings& GetDefaultInputActionSettings() const { return m_DefaultActionSettings; }
	std::optional<vec2> GetMouseButtonDownScreenPosition(EMouseButton button) const;
	std::optional<vec2> GetMouseButtonDownPosition(EMouseButton button) const;
	void EvaluateActionMap(std::string_view mapId, bool enabled, const InputActionMapMouseInput& mouse = {});
	vec2 GetActionMapMouseDelta(std::string_view mapId) const;
	f32 GetActionMapMouseScrollDeltaY(std::string_view mapId) const;
	bool WasActionPressed(std::string_view mapId, std::string_view actionId) const;
	bool WasActionReleased(std::string_view mapId, std::string_view actionId) const;
	bool WasActionPressedInFixedUpdate(std::string_view mapId, std::string_view actionId) const;
	bool WasActionReleasedInFixedUpdate(std::string_view mapId, std::string_view actionId) const;
	void ConsumeFixedStepActions();
	bool IsActionHeld(std::string_view mapId, std::string_view actionId) const;
	bool HasKeyboardFocus() const;
	static bool ValidateInputActionSettings(const InputActionSettings& settings);
#pragma endregion

#pragma region Gamepad
public:
	static constexpr uint32 MaxGamepads = 4;

	bool IsGamepadConnected(uint32 gamepadIndex = 0) const;
	uint32 GetConnectedGamepadCount() const;

	bool IsGamepadButtonDown(EGamepadButton button, uint32 gamepadIndex = 0) const;
	bool IsGamepadButtonUp(EGamepadButton button, uint32 gamepadIndex = 0) const;
	bool IsGamepadButtonPressed(EGamepadButton button, uint32 gamepadIndex = 0) const;

	vec2 GetGamepadLeftStick(uint32 gamepadIndex = 0) const;
	vec2 GetGamepadRightStick(uint32 gamepadIndex = 0) const;
	f32 GetGamepadLeftTrigger(uint32 gamepadIndex = 0) const;
	f32 GetGamepadRightTrigger(uint32 gamepadIndex = 0) const;

	void SetGamepadDeadzone(f32 stickDeadzone, f32 triggerDeadzone = 0.05f);
	f32 GetGamepadStickDeadzone() const { return m_GamepadStickDeadzone; }
	f32 GetGamepadTriggerDeadzone() const { return m_GamepadTriggerDeadzone; }

	bool SetGamepadRumble(f32 lowFreq, f32 highFreq, uint32 durationMs, uint32 gamepadIndex = 0);
#pragma endregion

#pragma region KeyChord
public:
	uint8 GetKeyDownModifiers(EKeyCode key);
private:
	uint8 GetKeyboardModifiers();
#pragma endregion


#pragma region Member Variables
private:
	array<uint8, SDL_SCANCODE_COUNT> m_CurrentKeyStates{}, m_PreviousKeyStates{};
	array<bool, SDL_SCANCODE_COUNT> m_KeyDownEvents{}, m_KeyUpEvents{}, m_KeyRepeatEvents{};
	array<bool, SDL_SCANCODE_COUNT> m_PendingKeyDown{}, m_PendingKeyUp{}, m_PendingKeyRepeat{};
	array<uint8, SDL_SCANCODE_COUNT> m_KeyDownModifiers{}, m_PendingKeyDownModifiers{};

	array<uint8, static_cast<uint32>(EMouseButton::Count)> m_CurrentMouseButtonStates{};
	array<uint8, static_cast<uint32>(EMouseButton::Count)> m_PreviousMouseButtonStates{};
	array<bool, static_cast<uint32>(EMouseButton::Count)> m_MouseDownEvents{}, m_MouseUpEvents{};
	array<bool, static_cast<uint32>(EMouseButton::Count)> m_PendingMouseDown{}, m_PendingMouseUp{};
	std::vector<MouseWheelSample> m_MouseWheelSamples, m_PendingMouseWheelSamples;
	std::array<std::optional<vec2>, static_cast<std::size_t>(EMouseButton::Count)> m_MouseDownScreenPositions{};
	std::array<vec2, static_cast<std::size_t>(EMouseButton::Count)> m_MouseDownPositions{};


	vec2 m_MousePosition = vec2(0.f);
	vec2 m_ScreenMousePosition = vec2(0.f);
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
private:// Input Action Maps
	struct ButtonActionState
	{
		std::vector<KeyChord> Chords;
		std::vector<EMouseButton> MouseButtons;
		std::vector<EGamepadButton> GamepadButtons;
		bool Held = false;
		bool Pressed = false;
		bool Released = false;
		bool FixedPressed = false;
		bool FixedReleased = false;
	};

	struct ActionMapState
	{
		std::map<std::string, ButtonActionState, std::less<>> Actions;
		std::set<std::pair<EKeyCode, uint8>> ModifiedChords;
		bool Enabled = false;
		InputActionMapMouseInput MouseInput;
	};

	using ActionMaps = std::map<std::string, ActionMapState, std::less<>>;
	static std::optional<ActionMaps> BuildActionMaps(const InputActionSettings& settings);
	bool ApplyActionSettingsPreservingRuntimeState(InputActionSettings settings);
	ActionMaps m_ActionMaps;
	InputActionSettings m_DefaultActionSettings;
	InputActionSettings m_CurrentActionSettings;

private:// Gamepad
	struct GamepadState
	{
		SDL_Gamepad* Handle = nullptr;
		SDL_JoystickID InstanceId = 0;
		bool Connected = false;
		std::array<uint8, static_cast<std::size_t>(EGamepadButton::Count)> CurrentButtons{};
		std::array<uint8, static_cast<std::size_t>(EGamepadButton::Count)> PreviousButtons{};
		std::array<bool, static_cast<std::size_t>(EGamepadButton::Count)> DownEvents{};
		std::array<bool, static_cast<std::size_t>(EGamepadButton::Count)> UpEvents{};
		std::array<bool, static_cast<std::size_t>(EGamepadButton::Count)> PendingDown{};
		std::array<bool, static_cast<std::size_t>(EGamepadButton::Count)> PendingUp{};
		vec2 RawLeftStick = vec2(0.0f);
		vec2 RawRightStick = vec2(0.0f);
		vec2 LeftStick = vec2(0.0f);
		vec2 RightStick = vec2(0.0f);
		f32 RawLeftTrigger = 0.0f;
		f32 RawRightTrigger = 0.0f;
		f32 LeftTrigger = 0.0f;
		f32 RightTrigger = 0.0f;
	};

	int32 FindGamepadSlot(SDL_JoystickID instanceId) const;
	void OpenGamepadById(SDL_JoystickID instanceId);
	void CloseGamepadSlot(uint32 slotIndex);
	void UpdateGamepads();

	std::array<GamepadState, MaxGamepads> m_Gamepads{};
	f32 m_GamepadStickDeadzone = 0.15f;
	f32 m_GamepadTriggerDeadzone = 0.05f;
	bool m_GamepadSubsystemInitialized = false;
#pragma endregion
};
END
