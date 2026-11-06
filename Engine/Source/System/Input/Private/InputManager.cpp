#pragma once

#include "InputManager.h"
#include "SDL3/SDL.h"
#include "InputActionSettings.h"
#include "Logger.h"

IMPLEMENT_SINGLETON(InputManager)

#pragma region Helper
namespace
{
	uint8 MakeModifierMask(bool ctrl, bool shift, bool alt)
	{
		return static_cast<uint8>(
			(ctrl ? Engine::EModifierKey::Ctrl : Engine::EModifierKey::None) |
			(shift ? Engine::EModifierKey::Shift : Engine::EModifierKey::None) |
			(alt ? Engine::EModifierKey::Alt : Engine::EModifierKey::None));
	}

	std::optional<vec2> ToInputScreenPosition(SDL_WindowID windowId, f32 x, f32 y)
	{
		auto* window = SDL_GetWindowFromID(windowId);
		int windowX = 0;
		int windowY = 0;
		if (!window || !SDL_GetWindowPosition(window, &windowX, &windowY))
			return std::nullopt;

		return vec2(static_cast<f32>(windowX) + x, static_cast<f32>(windowY) + y);
	}
}
#pragma endregion

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
	// Preserve taps that begin and end between two frame snapshots.
	m_KeyDownEvents = m_PendingKeyDown;
	m_KeyUpEvents = m_PendingKeyUp;
	m_KeyRepeatEvents = m_PendingKeyRepeat;
	m_MouseDownEvents = m_PendingMouseDown;
	m_MouseUpEvents = m_PendingMouseUp;
	m_KeyDownModifiers = m_PendingKeyDownModifiers;
	m_PendingKeyDown.fill(false);
	m_PendingKeyUp.fill(false);
	m_PendingKeyRepeat.fill(false);
	m_PendingMouseDown.fill(false);
	m_PendingMouseUp.fill(false);

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
	vec2 screenMousePosition;
	SDL_GetGlobalMouseState(&screenMousePosition.x, &screenMousePosition.y);

	for (uint32 i = 1; i <= static_cast<uint32>(EMouseButton::Count); ++i)
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

		bool isDownNow = IsMouseButtonDown(static_cast<EMouseButton>(i));
		bool isUpNow = IsMouseButtonUp(static_cast<EMouseButton>(i));
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
			m_MouseButtonStatesData[i].DownPosition = screenMousePosition;
			m_MouseButtonStatesData[i].MaxDragDistance = 0.0f;
			m_MouseButtonStatesData[i].IsDragging = false;
		}

		// [드래그 체크]
		if (isPressed)
		{
			m_MouseButtonStatesData[i].MaxDragDistance = std::max(m_MouseButtonStatesData[i].MaxDragDistance,
				glm::distance(screenMousePosition, m_MouseButtonStatesData[i].DownPosition));
			if (!m_MouseButtonStatesData[i].IsDragging)
			{
				// 누른 위치로부터 임계값 이상 멀어졌다면 드래그 시작
				if (m_MouseButtonStatesData[i].MaxDragDistance >= m_DragThreshold)
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

	m_MouseDelta = screenMousePosition - m_ScreenMousePosition;
	if (SDL_Window* window = SDL_GetMouseFocus(); window && SDL_GetWindowRelativeMouseMode(window))
		SDL_GetRelativeMouseState(&m_MouseDelta.x, &m_MouseDelta.y);
	m_ScreenMousePosition = screenMousePosition;
	m_MousePosition = newMousePosition;
	m_MouseScrollDelta = m_AccmulatedMouseScrollDelta;
	m_AccmulatedMouseScrollDelta = vec2(0.f);
	m_MouseWheelSamples.swap(m_PendingMouseWheelSamples);
	m_PendingMouseWheelSamples.clear();
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
	return m_KeyDownEvents[keyIndex] || (m_CurrentKeyStates[keyIndex] && !m_PreviousKeyStates[keyIndex]);
}
bool InputManager::IsKeyButtonUp(EKeyCode key)
{
	uint32 keyIndex = static_cast<uint32>(key);
	if (keyIndex >= SDL_SCANCODE_COUNT)
	{
		return false;
	}
	return m_KeyUpEvents[keyIndex] || (!m_CurrentKeyStates[keyIndex] && m_PreviousKeyStates[keyIndex]);
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
bool InputManager::IsKeyButtonRepeated(EKeyCode key) const
{
	const auto index = static_cast<uint32>(key);
	return index < m_KeyRepeatEvents.size() &&
		m_CurrentKeyStates[index] != 0 && m_KeyRepeatEvents[index];
}

bool InputManager::IsKeyButtonDown(const string& key)
{
	return IsKeyButtonDown(StringToEngineKeyCode(key));
}

bool InputManager::IsKeyButtonUp(const string& key)
{
	return IsKeyButtonUp(StringToEngineKeyCode(key));
}

bool InputManager::IsKeyButtonPressed(const string& key)
{
	return IsKeyButtonPressed(StringToEngineKeyCode(key));
}

bool InputManager::IsKeyButtonRepeated(const string& key) const
{
	return IsKeyButtonRepeated(StringToEngineKeyCode(key));
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
	return m_MouseDownEvents[buttonIndex] || (m_CurrentMouseButtonStates[buttonIndex] && !m_PreviousMouseButtonStates[buttonIndex]);
}
std::optional<vec2> InputManager::GetMouseButtonDownScreenPosition(EMouseButton button) const
{
	const auto index = static_cast<uint32>(button);
	if (index >= m_MouseDownScreenPositions.size() || !m_MouseDownEvents[index])
		return std::nullopt;

	return m_MouseDownScreenPositions[index];
}
bool InputManager::IsMouseButtonUp(EMouseButton button)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count))
	{
		return false;
	}
	return m_MouseUpEvents[buttonIndex] || (!m_CurrentMouseButtonStates[buttonIndex] && m_PreviousMouseButtonStates[buttonIndex]);
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
bool InputManager::IsMouseButtonDragging(EMouseButton button, f32 threshold)
{
	uint32 buttonIndex = static_cast<uint32>(button);
	if (buttonIndex >= static_cast<uint32>(EMouseButton::Count)) return false;
	if (!m_CurrentMouseButtonStates[buttonIndex]) return false;
	if (threshold == m_DragThreshold) return m_MouseButtonStatesData[buttonIndex].IsDragging;
	return m_MouseButtonStatesData[buttonIndex].MaxDragDistance >= threshold;
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
	if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
	{
		const auto key = SDLKeyToEngineKeyCode(event.key.scancode);
		const auto index = static_cast<uint32>(key);

		if (key != EKeyCode::Unknown && index < m_PendingKeyDown.size())
		{
			if (event.type == SDL_EVENT_KEY_UP)
			{
				m_PendingKeyUp[index] = true;
				m_PendingKeyRepeat[index] = false;
			}
			else if (event.key.repeat)
			{
				m_PendingKeyRepeat[index] = true;
			}
			else
			{
				m_PendingKeyDown[index] = true;
				const auto modifiers = event.key.mod;
				m_PendingKeyDownModifiers[index] = MakeModifierMask(
					(modifiers & SDL_KMOD_CTRL) != 0, (modifiers & SDL_KMOD_SHIFT) != 0, (modifiers & SDL_KMOD_ALT) != 0);
			}
		}
	}
	else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
	{
		const auto button = SDLButtonToEngineMouseButton(event.button.button);
		if (button != EMouseButton::Count)
		{
			const auto index = static_cast<uint32>(button);
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
			{
				m_PendingMouseDown[index] = true;
				m_MouseDownScreenPositions[index] = ToInputScreenPosition(event.button.windowID, event.button.x, event.button.y);
			}
			else
			{
				m_PendingMouseUp[index] = true;
			}
		}
	}
	if (event.type == SDL_EVENT_MOUSE_WHEEL)
	{
		m_AccmulatedMouseScrollDelta.x += event.wheel.x;
		m_AccmulatedMouseScrollDelta.y += event.wheel.y;

		const auto position = ToInputScreenPosition(event.wheel.windowID, event.wheel.mouse_x, event.wheel.mouse_y);
		if (position)
			m_PendingMouseWheelSamples.push_back({ *position, event.wheel.y });
	}
}
#pragma endregion

#pragma region Input Actions
bool InputManager::ValidateInputActionSettings(const InputActionSettings& settings)
{
	return BuildActionMaps(settings).has_value();
}

bool InputManager::SetInputActionSettings(const InputActionSettings& settings)
{
	auto maps = BuildActionMaps(settings);
	if (!maps) return false;

	m_ActionMaps = std::move(*maps);
	return true;
}

std::optional<InputManager::ActionMaps> InputManager::BuildActionMaps(const InputActionSettings& settings)
{
	ActionMaps maps;
	const auto reject = [](std::string_view map, std::string_view action, std::string_view reason)
		{
			ENGINE_LOG_ERROR("Invalid input mapping in '{}', action '{}': {}", map, action, reason);
			return std::nullopt;
		};

	for (const auto& source : settings.Maps)
	{
		const auto [map, inserted] = maps.try_emplace(source.Id);
		if (source.Id.empty() || !inserted)
			return reject(source.Id, "", "empty or duplicate map ID");

		auto& actions = map->second.Actions;
		for (const auto& action : source.Actions)
		{
			if (action.Type != "Button")
				return reject(source.Id, action.Id, "unsupported action type");
			if (action.Id.empty() || !actions.try_emplace(action.Id).second)
				return reject(source.Id, action.Id, "empty or duplicate action ID");
		}

		for (const auto& binding : source.Bindings)
		{
			const auto action = actions.find(binding.ActionId);
			if (action == actions.end())
				return reject(source.Id, binding.ActionId, "binding references an unknown action");
			constexpr std::string_view mousePrefix = "Mouse:";
			if (binding.Key.starts_with(mousePrefix))
			{
				const auto button = StringToEngineMouseButton(binding.Key.substr(mousePrefix.size()));
				if (button == EMouseButton::Count)
					return reject(source.Id, binding.ActionId, "invalid mouse button: " + binding.Key);

				auto& buttons = action->second.MouseButtons;
				if (std::ranges::find(buttons, button) != buttons.end())
					return reject(source.Id, binding.ActionId, "duplicate mouse button: " + binding.Key);

				buttons.push_back(button);
				continue;
			}
			const KeyChord chord = KeyChord::FromString(binding.Key);
			if (!chord.IsValid())
				return reject(source.Id, binding.ActionId, "invalid chord: " + binding.Key);

			auto& chords = action->second.Chords;
			if (std::find(chords.begin(), chords.end(), chord) != chords.end())
				return reject(source.Id, binding.ActionId, "duplicate chord: " + binding.Key);

			chords.push_back(chord);
			if (chord.Modifiers != 0)
				map->second.ModifiedChords.emplace(chord.Key, chord.Modifiers);
		}
	}

	return maps;
}

void InputManager::EvaluateActionMap(std::string_view mapId, bool enabled, const InputActionMapMouseInput& mouse)
{
	const auto map = m_ActionMaps.find(mapId);
	if (map == m_ActionMaps.end()) return;

	auto& state = map->second;
	const bool wasEnabled = state.Enabled;
	state.Enabled = enabled;
	state.MouseInput = enabled ? mouse : InputActionMapMouseInput{};

	const auto matches = [&state](const KeyChord& chord, uint8 modifiers)
		{
			if (chord.Modifiers != 0)
				return chord.Modifiers == modifiers;

			return !state.ModifiedChords.contains({ chord.Key, modifiers });
		};

	const auto isMouseAllowed = [allowedMouseButtons = mouse.AllowedButtons](EMouseButton button)
		{
			const auto bit = uint32{ 1 } << static_cast<uint32>(button);
			return (allowedMouseButtons & bit) != 0;
		};

	const uint8 heldModifiers = GetKeyboardModifiers();
	for (auto& [id, action] : state.Actions)
	{
		const bool wasHeld = action.Held;
		action.Pressed = enabled && wasEnabled && !action.Held &&
			(std::ranges::any_of(action.Chords, [&](const KeyChord& chord)
				{
					return IsKeyButtonDown(chord.Key) && matches(chord, GetKeyDownModifiers(chord.Key));
				}) || std::ranges::any_of(action.MouseButtons, [&](EMouseButton button)
					{
						return isMouseAllowed(button) && IsMouseButtonDown(button);
					}));

				action.Held = enabled &&
					(std::ranges::any_of(action.Chords, [&](const KeyChord& chord)
						{
							return IsKeyButtonPressed(chord.Key) && matches(chord, heldModifiers);
						}) || std::ranges::any_of(action.MouseButtons, [&](EMouseButton button)
							{
								return isMouseAllowed(button) && IsMouseButtonPressed(button);
							}));
						action.Released = enabled && wasEnabled && !action.Held && (wasHeld || action.Pressed);
	}
}

f32 InputManager::GetActionMapMouseScrollDeltaY(std::string_view mapId) const
{
	const auto map = m_ActionMaps.find(mapId);
	return map != m_ActionMaps.end() && map->second.Enabled ? map->second.MouseInput.ScrollDeltaY : 0.0f;
}

vec2 InputManager::GetActionMapMouseDelta(std::string_view mapId) const
{
	const auto map = m_ActionMaps.find(mapId);
	return map != m_ActionMaps.end() && map->second.Enabled ? map->second.MouseInput.MotionDelta : vec2(0.0f);
}

bool InputManager::WasActionPressed(std::string_view mapId, std::string_view actionId) const
{
	const auto map = m_ActionMaps.find(mapId);
	if (map == m_ActionMaps.end() || !map->second.Enabled) return false;

	const auto action = map->second.Actions.find(actionId);
	return action != map->second.Actions.end() && action->second.Pressed;
}

bool InputManager::WasActionReleased(std::string_view mapId, std::string_view actionId) const
{
	const auto map = m_ActionMaps.find(mapId);
	if (map == m_ActionMaps.end() || !map->second.Enabled)
		return false;

	const auto action = map->second.Actions.find(actionId);
	return action != map->second.Actions.end() && action->second.Released;
}

bool InputManager::IsActionHeld(std::string_view mapId, std::string_view actionId) const
{
	const auto map = m_ActionMaps.find(mapId);
	if (map == m_ActionMaps.end() || !map->second.Enabled)
		return false;

	const auto action = map->second.Actions.find(actionId);
	return action != map->second.Actions.end() && action->second.Held;
}

bool InputManager::HasKeyboardFocus() const
{
	return SDL_GetKeyboardFocus() != nullptr;
}
#pragma endregion

#pragma region KeyChord
uint8 InputManager::GetKeyboardModifiers()
{
	return MakeModifierMask(IsCtrlPressed(), IsShiftPressed(), IsAltPressed());
}

uint8 InputManager::GetKeyDownModifiers(EKeyCode key)
{
	const auto index = static_cast<uint32>(key);
	if (index < m_KeyDownEvents.size() && m_KeyDownEvents[index])
		return m_KeyDownModifiers[index];

	return GetKeyboardModifiers();
}
#pragma endregion
