#pragma once

#include "InputManager.h"
#include "SDL3/SDL.h"
#include "InputActionSettings.h"
#include "Logger.h"

IMPLEMENT_SINGLETON(InputManager)

#pragma region Helper
namespace InputBindingDetail
{
	struct UserBindingEntryDto
	{
		std::string ActionId;
		std::vector<std::string> Keys;
	};

	struct UserActionMapBindingsDto
	{
		std::string MapId;
		std::vector<UserBindingEntryDto> Actions;
	};

	struct UserInputBindingsFileDto
	{
		uint32 Version = 1;
		std::vector<UserActionMapBindingsDto> Maps;
	};
}

namespace
{
	using InputBindingDetail::UserBindingEntryDto;
	using InputBindingDetail::UserActionMapBindingsDto;
	using InputBindingDetail::UserInputBindingsFileDto;

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

	SDL_GamepadButton EngineGamepadButtonToSDL(Engine::EGamepadButton button)
	{
		switch (button)
		{
		case Engine::EGamepadButton::South: return SDL_GAMEPAD_BUTTON_SOUTH;
		case Engine::EGamepadButton::East: return SDL_GAMEPAD_BUTTON_EAST;
		case Engine::EGamepadButton::West: return SDL_GAMEPAD_BUTTON_WEST;
		case Engine::EGamepadButton::North: return SDL_GAMEPAD_BUTTON_NORTH;
		case Engine::EGamepadButton::Back: return SDL_GAMEPAD_BUTTON_BACK;
		case Engine::EGamepadButton::Guide: return SDL_GAMEPAD_BUTTON_GUIDE;
		case Engine::EGamepadButton::Start: return SDL_GAMEPAD_BUTTON_START;
		case Engine::EGamepadButton::LeftStick: return SDL_GAMEPAD_BUTTON_LEFT_STICK;
		case Engine::EGamepadButton::RightStick: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
		case Engine::EGamepadButton::LeftShoulder: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
		case Engine::EGamepadButton::RightShoulder: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
		case Engine::EGamepadButton::DPadUp: return SDL_GAMEPAD_BUTTON_DPAD_UP;
		case Engine::EGamepadButton::DPadDown: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
		case Engine::EGamepadButton::DPadLeft: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
		case Engine::EGamepadButton::DPadRight: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
		default: return SDL_GAMEPAD_BUTTON_INVALID;
		}
	}

	f32 NormalizeGamepadStickAxis(Sint16 value)
	{
		return value < 0 ? static_cast<f32>(value) / 32768.0f : static_cast<f32>(value) / 32767.0f;
	}

	f32 NormalizeGamepadTriggerAxis(Sint16 value)
	{
		return std::clamp(static_cast<f32>(value) / 32767.0f, 0.0f, 1.0f);
	}

	vec2 ApplyRadialDeadzone(const vec2& raw, f32 deadzone)
	{
		const f32 magnitude = glm::length(raw);
		if (magnitude <= deadzone || magnitude <= 0.00001f)
			return vec2(0.0f);

		const f32 clampedMag = std::min(magnitude, 1.0f);
		const f32 rescaled = (clampedMag - deadzone) / (1.0f - deadzone);
		return (raw / magnitude) * std::clamp(rescaled, 0.0f, 1.0f);
	}

	f32 ApplyTriggerDeadzone(f32 raw, f32 deadzone)
	{
		if (raw <= deadzone)
			return 0.0f;
		return std::clamp((raw - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
	}
}
#pragma endregion

#pragma region Constructor&Destructor
EResult InputManager::Initialize(void* arg)
{
	if (SDL_InitSubSystem(SDL_INIT_GAMEPAD))
	{
		m_GamepadSubsystemInitialized = true;
		int gamepadCount = 0;
		if (SDL_JoystickID* ids = SDL_GetGamepads(&gamepadCount))
		{
			for (int i = 0; i < gamepadCount; ++i)
				OpenGamepadById(ids[i]);
			SDL_free(ids);
		}
	}
	else
	{
		ENGINE_LOG_WARN("SDL_InitSubSystem(SDL_INIT_GAMEPAD) failed: {}", SDL_GetError());
	}
	return EResult::Success;
}
void InputManager::Free()
{
	for (uint32 i = 0; i < MaxGamepads; ++i)
		CloseGamepadSlot(i);

	if (m_GamepadSubsystemInitialized)
	{
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		m_GamepadSubsystemInitialized = false;
	}
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

	UpdateGamepads();
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
std::optional<vec2> InputManager::GetMouseButtonDownPosition(EMouseButton button) const
{
	const auto index = static_cast<uint32>(button);
	if (index >= m_MouseDownPositions.size() || !m_MouseDownEvents[index])
		return std::nullopt;

	return m_MouseDownPositions[index];
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
				m_MouseDownPositions[index] = vec2(event.button.x, event.button.y);
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
	else if (event.type == SDL_EVENT_GAMEPAD_ADDED)
	{
		OpenGamepadById(event.gdevice.which);
	}
	else if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
	{
		const int32 slot = FindGamepadSlot(event.gdevice.which);
		if (slot >= 0)
			CloseGamepadSlot(static_cast<uint32>(slot));
	}
	else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
	{
		const int32 slotIndex = FindGamepadSlot(event.gbutton.which);
		if (slotIndex >= 0)
		{
			const auto button = SDLButtonToEngineGamepadButton(event.gbutton.button);
			if (button != EGamepadButton::Count)
			{
				auto& slot = m_Gamepads[static_cast<std::size_t>(slotIndex)];
				const auto idx = static_cast<std::size_t>(button);
				if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
				{
					slot.PendingDown[idx] = true;
					if (!slot.Handle)
						slot.CurrentButtons[idx] = 1;
				}
				else
				{
					slot.PendingUp[idx] = true;
					if (!slot.Handle)
						slot.CurrentButtons[idx] = 0;
				}
			}
		}
	}
	else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
	{
		const int32 slotIndex = FindGamepadSlot(event.gaxis.which);
		if (slotIndex >= 0)
		{
			auto& slot = m_Gamepads[static_cast<std::size_t>(slotIndex)];
			switch (event.gaxis.axis)
			{
			case SDL_GAMEPAD_AXIS_LEFTX: slot.RawLeftStick.x = NormalizeGamepadStickAxis(event.gaxis.value); break;
			case SDL_GAMEPAD_AXIS_LEFTY: slot.RawLeftStick.y = NormalizeGamepadStickAxis(event.gaxis.value); break;
			case SDL_GAMEPAD_AXIS_RIGHTX: slot.RawRightStick.x = NormalizeGamepadStickAxis(event.gaxis.value); break;
			case SDL_GAMEPAD_AXIS_RIGHTY: slot.RawRightStick.y = NormalizeGamepadStickAxis(event.gaxis.value); break;
			case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: slot.RawLeftTrigger = NormalizeGamepadTriggerAxis(event.gaxis.value); break;
			case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: slot.RawRightTrigger = NormalizeGamepadTriggerAxis(event.gaxis.value); break;
			default: break;
			}
		}
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
	m_DefaultActionSettings = settings;
	m_CurrentActionSettings = settings;
	return true;
}

bool InputManager::ApplyActionSettingsPreservingRuntimeState(InputActionSettings settings)
{
	auto maps = BuildActionMaps(settings);
	if (!maps) return false;

	for (auto& [mapId, newMap] : *maps)
	{
		const auto oldMapIt = m_ActionMaps.find(mapId);
		if (oldMapIt == m_ActionMaps.end()) continue;

		newMap.Enabled = oldMapIt->second.Enabled;
		newMap.MouseInput = oldMapIt->second.MouseInput;

		for (auto& [actionId, newAction] : newMap.Actions)
		{
			const auto oldActionIt = oldMapIt->second.Actions.find(actionId);
			if (oldActionIt == oldMapIt->second.Actions.end()) continue;

			newAction.Held = oldActionIt->second.Held;
			newAction.Pressed = oldActionIt->second.Pressed;
			newAction.Released = oldActionIt->second.Released;
			newAction.FixedPressed = oldActionIt->second.FixedPressed;
			newAction.FixedReleased = oldActionIt->second.FixedReleased;
		}
	}

	m_ActionMaps = std::move(*maps);
	m_CurrentActionSettings = std::move(settings);
	return true;
}

bool InputManager::RebindAction(std::string_view mapId, std::string_view actionId, std::string_view newBindingKey)
{
	if (newBindingKey.empty())
	{
		ENGINE_LOG_ERROR("RebindAction failed for '{}.{}': empty binding key", mapId, actionId);
		return false;
	}

	InputActionSettings candidate = m_CurrentActionSettings;
	auto mapIt = std::ranges::find_if(candidate.Maps, [mapId](const InputActionMap& m) { return m.Id == mapId; });
	if (mapIt == candidate.Maps.end())
	{
		ENGINE_LOG_ERROR("RebindAction failed: unknown map '{}'", mapId);
		return false;
	}

	const bool actionExists = std::ranges::any_of(mapIt->Actions, [actionId](const InputAction& a) { return a.Id == actionId; });
	if (!actionExists)
	{
		ENGINE_LOG_ERROR("RebindAction failed: unknown action '{}.{}'", mapId, actionId);
		return false;
	}

	std::erase_if(mapIt->Bindings, [actionId](const InputBinding& b) { return b.ActionId == actionId; });
	mapIt->Bindings.push_back(InputBinding{ std::string(actionId), std::string(newBindingKey) });

	return ApplyActionSettingsPreservingRuntimeState(std::move(candidate));
}

bool InputManager::RebindAction(std::string_view mapId, std::string_view actionId, std::size_t bindingIndex, std::string_view newBindingKey)
{
	if (newBindingKey.empty())
	{
		ENGINE_LOG_ERROR("RebindAction failed for '{}.{}': empty binding key", mapId, actionId);
		return false;
	}

	InputActionSettings candidate = m_CurrentActionSettings;
	auto mapIt = std::ranges::find_if(candidate.Maps, [mapId](const InputActionMap& m) { return m.Id == mapId; });
	if (mapIt == candidate.Maps.end())
	{
		ENGINE_LOG_ERROR("RebindAction failed: unknown map '{}'", mapId);
		return false;
	}

	const bool actionExists = std::ranges::any_of(mapIt->Actions, [actionId](const InputAction& a) { return a.Id == actionId; });
	if (!actionExists)
	{
		ENGINE_LOG_ERROR("RebindAction failed: unknown action '{}.{}'", mapId, actionId);
		return false;
	}

	std::size_t currentIndex = 0;
	bool replaced = false;
	for (auto& binding : mapIt->Bindings)
	{
		if (binding.ActionId != actionId) continue;
		if (currentIndex == bindingIndex)
		{
			binding.Key = std::string(newBindingKey);
			replaced = true;
			break;
		}
		++currentIndex;
	}

	if (!replaced)
	{
		if (bindingIndex != currentIndex)
		{
			ENGINE_LOG_ERROR("RebindAction failed for '{}.{}': binding index {} out of range", mapId, actionId, bindingIndex);
			return false;
		}
		mapIt->Bindings.push_back(InputBinding{ std::string(actionId), std::string(newBindingKey) });
	}

	return ApplyActionSettingsPreservingRuntimeState(std::move(candidate));
}

bool InputManager::ResetActionBindingsToDefault(std::string_view mapId, std::string_view actionId)
{
	const auto defaultMapIt = std::ranges::find_if(m_DefaultActionSettings.Maps, [mapId](const InputActionMap& m) { return m.Id == mapId; });
	if (defaultMapIt == m_DefaultActionSettings.Maps.end()) return false;

	const bool defaultActionExists = std::ranges::any_of(defaultMapIt->Actions, [actionId](const InputAction& a) { return a.Id == actionId; });
	if (!defaultActionExists) return false;

	InputActionSettings candidate = m_CurrentActionSettings;
	auto mapIt = std::ranges::find_if(candidate.Maps, [mapId](const InputActionMap& m) { return m.Id == mapId; });
	if (mapIt == candidate.Maps.end()) return false;

	std::erase_if(mapIt->Bindings, [actionId](const InputBinding& b) { return b.ActionId == actionId; });
	for (const auto& defaultBinding : defaultMapIt->Bindings)
	{
		if (defaultBinding.ActionId == actionId)
			mapIt->Bindings.push_back(defaultBinding);
	}

	return ApplyActionSettingsPreservingRuntimeState(std::move(candidate));
}

void InputManager::ResetAllBindingsToDefault()
{
	ApplyActionSettingsPreservingRuntimeState(m_DefaultActionSettings);
}

bool InputManager::SaveBindingsToFile(const std::filesystem::path& filePath) const
{
	if (filePath.empty())
	{
		ENGINE_LOG_ERROR("SaveBindingsToFile failed: empty file path");
		return false;
	}

	UserInputBindingsFileDto dto;
	for (const auto& map : m_CurrentActionSettings.Maps)
	{
		UserActionMapBindingsDto mapDto;
		mapDto.MapId = map.Id;

		for (const auto& action : map.Actions)
		{
			UserBindingEntryDto entryDto;
			entryDto.ActionId = action.Id;
			for (const auto& binding : map.Bindings)
			{
				if (binding.ActionId == action.Id)
					entryDto.Keys.push_back(binding.Key);
			}
			mapDto.Actions.push_back(std::move(entryDto));
		}

		dto.Maps.push_back(std::move(mapDto));
	}

	std::string outJson;
	const auto writeEc = glz::write<glz::opts{ .prettify = true, .indentation_width = 4 }>(dto, outJson);
	if (writeEc)
	{
		ENGINE_LOG_ERROR("SaveBindingsToFile failed: JSON serialization error for '{}'", filePath.string());
		return false;
	}

	std::error_code ec;
	const auto parent = filePath.parent_path();
	if (!parent.empty())
	{
		std::filesystem::create_directories(parent, ec);
		if (ec)
		{
			ENGINE_LOG_ERROR("SaveBindingsToFile failed to create directory '{}': {}", parent.string(), ec.message());
			return false;
		}
	}

	std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		ENGINE_LOG_ERROR("SaveBindingsToFile failed to open '{}' for writing", filePath.string());
		return false;
	}

	file.write(outJson.data(), static_cast<std::streamsize>(outJson.size()));
	if (!file.good())
	{
		ENGINE_LOG_ERROR("SaveBindingsToFile failed while writing '{}'", filePath.string());
		return false;
	}

	return true;
}

bool InputManager::LoadBindingsFromFile(const std::filesystem::path& filePath)
{
	if (filePath.empty())
	{
		ENGINE_LOG_ERROR("LoadBindingsFromFile failed: empty file path");
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::exists(filePath, ec) || ec)
	{
		ENGINE_LOG_WARN("LoadBindingsFromFile: file not found '{}'", filePath.string());
		return false;
	}

	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open())
	{
		ENGINE_LOG_ERROR("LoadBindingsFromFile failed to open '{}'", filePath.string());
		return false;
	}

	std::ostringstream ss;
	ss << file.rdbuf();
	const std::string json = ss.str();
	if (json.empty())
	{
		ENGINE_LOG_ERROR("LoadBindingsFromFile: empty JSON file '{}'", filePath.string());
		return false;
	}

	UserInputBindingsFileDto dto;
	const auto parseEc = glz::read<glz::opts{ .error_on_unknown_keys = false }>(dto, json);
	if (parseEc)
	{
		ENGINE_LOG_ERROR("LoadBindingsFromFile parse error in '{}': {}", filePath.string(), glz::format_error(parseEc, json));
		return false;
	}

	InputActionSettings candidate = m_DefaultActionSettings;
	for (const auto& savedMap : dto.Maps)
	{
		auto mapIt = std::ranges::find_if(candidate.Maps, [&](const InputActionMap& m) { return m.Id == savedMap.MapId; });
		if (mapIt == candidate.Maps.end())
		{
			ENGINE_LOG_WARN("LoadBindingsFromFile: skipping unknown map '{}'", savedMap.MapId);
			continue;
		}

		for (const auto& savedAction : savedMap.Actions)
		{
			const bool actionExists = std::ranges::any_of(mapIt->Actions, [&](const InputAction& a) { return a.Id == savedAction.ActionId; });
			if (!actionExists)
			{
				ENGINE_LOG_WARN("LoadBindingsFromFile: skipping unknown action '{}.{}'", savedMap.MapId, savedAction.ActionId);
				continue;
			}

			std::erase_if(mapIt->Bindings, [&](const InputBinding& b) { return b.ActionId == savedAction.ActionId; });
			for (const auto& key : savedAction.Keys)
				mapIt->Bindings.push_back(InputBinding{ savedAction.ActionId, key });
		}
	}

	return ApplyActionSettingsPreservingRuntimeState(std::move(candidate));
}

std::vector<std::string> InputManager::GetActionBindings(std::string_view mapId, std::string_view actionId) const
{
	std::vector<std::string> result;
	const auto mapIt = std::ranges::find_if(m_CurrentActionSettings.Maps, [mapId](const InputActionMap& m) { return m.Id == mapId; });
	if (mapIt == m_CurrentActionSettings.Maps.end()) return result;

	for (const auto& binding : mapIt->Bindings)
	{
		if (binding.ActionId == actionId)
			result.push_back(binding.Key);
	}
	return result;
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
			constexpr std::string_view gamepadPrefix = "Gamepad:";
			if (binding.Key.starts_with(gamepadPrefix))
			{
				const auto button = StringToEngineGamepadButton(binding.Key.substr(gamepadPrefix.size()));
				if (button == EGamepadButton::Count)
					return reject(source.Id, binding.ActionId, "invalid gamepad button: " + binding.Key);

				auto& buttons = action->second.GamepadButtons;
				if (std::ranges::find(buttons, button) != buttons.end())
					return reject(source.Id, binding.ActionId, "duplicate gamepad button: " + binding.Key);

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

	const auto isAnyGamepadButtonDown = [this](EGamepadButton button)
		{
			for (uint32 i = 0; i < MaxGamepads; ++i)
			{
				if (IsGamepadButtonDown(button, i)) return true;
			}
			return false;
		};

	const auto isAnyGamepadButtonPressed = [this](EGamepadButton button)
		{
			for (uint32 i = 0; i < MaxGamepads; ++i)
			{
				if (IsGamepadButtonPressed(button, i)) return true;
			}
			return false;
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
					}) || std::ranges::any_of(action.GamepadButtons, isAnyGamepadButtonDown));

				action.Held = enabled &&
					(std::ranges::any_of(action.Chords, [&](const KeyChord& chord)
						{
							return IsKeyButtonPressed(chord.Key) && matches(chord, heldModifiers);
						}) || std::ranges::any_of(action.MouseButtons, [&](EMouseButton button)
							{
								return isMouseAllowed(button) && IsMouseButtonPressed(button);
							}) || std::ranges::any_of(action.GamepadButtons, isAnyGamepadButtonPressed));
						action.Released = enabled && wasEnabled && !action.Held && (wasHeld || action.Pressed);
		if (enabled)
		{
			if (action.Pressed) action.FixedPressed = true;
			if (action.Released) action.FixedReleased = true;
		}
		else
		{
			action.FixedPressed = false;
			action.FixedReleased = false;
		}
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

bool InputManager::WasActionPressedInFixedUpdate(std::string_view mapId, std::string_view actionId) const
{
	const auto map = m_ActionMaps.find(mapId);
	if (map == m_ActionMaps.end() || !map->second.Enabled)
		return false;

	const auto action = map->second.Actions.find(actionId);
	return action != map->second.Actions.end() && action->second.FixedPressed;
}

bool InputManager::WasActionReleasedInFixedUpdate(std::string_view mapId, std::string_view actionId) const
{
	const auto map = m_ActionMaps.find(mapId);
	if (map == m_ActionMaps.end() || !map->second.Enabled)
		return false;

	const auto action = map->second.Actions.find(actionId);
	return action != map->second.Actions.end() && action->second.FixedReleased;
}

void InputManager::ConsumeFixedStepActions()
{
	for (auto& [mapId, mapState] : m_ActionMaps)
	{
		for (auto& [actionId, action] : mapState.Actions)
		{
			action.FixedPressed = false;
			action.FixedReleased = false;
		}
	}
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

#pragma region Gamepad
int32 InputManager::FindGamepadSlot(SDL_JoystickID instanceId) const
{
	for (uint32 i = 0; i < MaxGamepads; ++i)
	{
		if (m_Gamepads[i].Connected && m_Gamepads[i].InstanceId == instanceId)
			return static_cast<int32>(i);
	}
	return -1;
}

void InputManager::OpenGamepadById(SDL_JoystickID instanceId)
{
	if (instanceId == 0 || FindGamepadSlot(instanceId) >= 0)
		return;

	SDL_Gamepad* handle = SDL_OpenGamepad(instanceId);
	if (!handle)
	{
		ENGINE_LOG_WARN("SDL_OpenGamepad({}) failed: {}", static_cast<uint32>(instanceId), SDL_GetError());
		return;
	}

	for (uint32 i = 0; i < MaxGamepads; ++i)
	{
		if (!m_Gamepads[i].Connected)
		{
			m_Gamepads[i] = GamepadState{};
			m_Gamepads[i].Handle = handle;
			m_Gamepads[i].InstanceId = instanceId;
			m_Gamepads[i].Connected = true;
			return;
		}
	}

	ENGINE_LOG_WARN("MaxGamepads ({}) reached; ignoring gamepad {}", MaxGamepads, static_cast<uint32>(instanceId));
	SDL_CloseGamepad(handle);
}

void InputManager::CloseGamepadSlot(uint32 slotIndex)
{
	if (slotIndex >= MaxGamepads) return;
	auto& slot = m_Gamepads[slotIndex];
	if (slot.Handle)
	{
		SDL_CloseGamepad(slot.Handle);
		slot.Handle = nullptr;
	}
	slot = GamepadState{};
}

void InputManager::UpdateGamepads()
{
	for (auto& slot : m_Gamepads)
	{
		slot.PreviousButtons = slot.CurrentButtons;
		slot.DownEvents = slot.PendingDown;
		slot.UpEvents = slot.PendingUp;
		slot.PendingDown.fill(false);
		slot.PendingUp.fill(false);

		if (!slot.Connected) continue;

		if (slot.Handle)
		{
			for (uint32 b = 0; b < static_cast<uint32>(EGamepadButton::Count); ++b)
			{
				const SDL_GamepadButton sdlBtn = EngineGamepadButtonToSDL(static_cast<EGamepadButton>(b));
				slot.CurrentButtons[b] = (sdlBtn != SDL_GAMEPAD_BUTTON_INVALID && SDL_GetGamepadButton(slot.Handle, sdlBtn)) ? 1 : 0;
			}

			slot.RawLeftStick.x = NormalizeGamepadStickAxis(SDL_GetGamepadAxis(slot.Handle, SDL_GAMEPAD_AXIS_LEFTX));
			slot.RawLeftStick.y = NormalizeGamepadStickAxis(SDL_GetGamepadAxis(slot.Handle, SDL_GAMEPAD_AXIS_LEFTY));
			slot.RawRightStick.x = NormalizeGamepadStickAxis(SDL_GetGamepadAxis(slot.Handle, SDL_GAMEPAD_AXIS_RIGHTX));
			slot.RawRightStick.y = NormalizeGamepadStickAxis(SDL_GetGamepadAxis(slot.Handle, SDL_GAMEPAD_AXIS_RIGHTY));
			slot.RawLeftTrigger = NormalizeGamepadTriggerAxis(SDL_GetGamepadAxis(slot.Handle, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
			slot.RawRightTrigger = NormalizeGamepadTriggerAxis(SDL_GetGamepadAxis(slot.Handle, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
		}

		slot.LeftStick = ApplyRadialDeadzone(slot.RawLeftStick, m_GamepadStickDeadzone);
		slot.RightStick = ApplyRadialDeadzone(slot.RawRightStick, m_GamepadStickDeadzone);
		slot.LeftTrigger = ApplyTriggerDeadzone(slot.RawLeftTrigger, m_GamepadTriggerDeadzone);
		slot.RightTrigger = ApplyTriggerDeadzone(slot.RawRightTrigger, m_GamepadTriggerDeadzone);
	}
}

bool InputManager::IsGamepadConnected(uint32 gamepadIndex) const
{
	return gamepadIndex < MaxGamepads && m_Gamepads[gamepadIndex].Connected;
}

uint32 InputManager::GetConnectedGamepadCount() const
{
	uint32 count = 0;
	for (const auto& slot : m_Gamepads)
	{
		if (slot.Connected) ++count;
	}
	return count;
}

bool InputManager::IsGamepadButtonDown(EGamepadButton button, uint32 gamepadIndex) const
{
	const auto idx = static_cast<std::size_t>(button);
	if (gamepadIndex >= MaxGamepads || idx >= static_cast<std::size_t>(EGamepadButton::Count))
		return false;

	const auto& slot = m_Gamepads[gamepadIndex];
	if (!slot.Connected) return false;
	return slot.DownEvents[idx] || (slot.CurrentButtons[idx] && !slot.PreviousButtons[idx]);
}

bool InputManager::IsGamepadButtonUp(EGamepadButton button, uint32 gamepadIndex) const
{
	const auto idx = static_cast<std::size_t>(button);
	if (gamepadIndex >= MaxGamepads || idx >= static_cast<std::size_t>(EGamepadButton::Count))
		return false;

	const auto& slot = m_Gamepads[gamepadIndex];
	if (!slot.Connected) return false;
	return slot.UpEvents[idx] || (!slot.CurrentButtons[idx] && slot.PreviousButtons[idx]);
}

bool InputManager::IsGamepadButtonPressed(EGamepadButton button, uint32 gamepadIndex) const
{
	const auto idx = static_cast<std::size_t>(button);
	if (gamepadIndex >= MaxGamepads || idx >= static_cast<std::size_t>(EGamepadButton::Count))
		return false;

	const auto& slot = m_Gamepads[gamepadIndex];
	return slot.Connected && slot.CurrentButtons[idx] != 0;
}

vec2 InputManager::GetGamepadLeftStick(uint32 gamepadIndex) const
{
	if (gamepadIndex >= MaxGamepads || !m_Gamepads[gamepadIndex].Connected)
		return vec2(0.0f);
	return m_Gamepads[gamepadIndex].LeftStick;
}

vec2 InputManager::GetGamepadRightStick(uint32 gamepadIndex) const
{
	if (gamepadIndex >= MaxGamepads || !m_Gamepads[gamepadIndex].Connected)
		return vec2(0.0f);
	return m_Gamepads[gamepadIndex].RightStick;
}

f32 InputManager::GetGamepadLeftTrigger(uint32 gamepadIndex) const
{
	if (gamepadIndex >= MaxGamepads || !m_Gamepads[gamepadIndex].Connected)
		return 0.0f;
	return m_Gamepads[gamepadIndex].LeftTrigger;
}

f32 InputManager::GetGamepadRightTrigger(uint32 gamepadIndex) const
{
	if (gamepadIndex >= MaxGamepads || !m_Gamepads[gamepadIndex].Connected)
		return 0.0f;
	return m_Gamepads[gamepadIndex].RightTrigger;
}

void InputManager::SetGamepadDeadzone(f32 stickDeadzone, f32 triggerDeadzone)
{
	m_GamepadStickDeadzone = std::clamp(stickDeadzone, 0.0f, 0.99f);
	m_GamepadTriggerDeadzone = std::clamp(triggerDeadzone, 0.0f, 0.99f);

	for (auto& slot : m_Gamepads)
	{
		if (!slot.Connected) continue;
		slot.LeftStick = ApplyRadialDeadzone(slot.RawLeftStick, m_GamepadStickDeadzone);
		slot.RightStick = ApplyRadialDeadzone(slot.RawRightStick, m_GamepadStickDeadzone);
		slot.LeftTrigger = ApplyTriggerDeadzone(slot.RawLeftTrigger, m_GamepadTriggerDeadzone);
		slot.RightTrigger = ApplyTriggerDeadzone(slot.RawRightTrigger, m_GamepadTriggerDeadzone);
	}
}

bool InputManager::SetGamepadRumble(f32 lowFreq, f32 highFreq, uint32 durationMs, uint32 gamepadIndex)
{
	if (gamepadIndex >= MaxGamepads) return false;
	const auto& slot = m_Gamepads[gamepadIndex];
	if (!slot.Connected || !slot.Handle) return false;

	const Uint16 low = static_cast<Uint16>(std::lround(std::clamp(lowFreq, 0.0f, 1.0f) * 65535.0f));
	const Uint16 high = static_cast<Uint16>(std::lround(std::clamp(highFreq, 0.0f, 1.0f) * 65535.0f));
	return SDL_RumbleGamepad(slot.Handle, low, high, durationMs);
}
#pragma endregion
