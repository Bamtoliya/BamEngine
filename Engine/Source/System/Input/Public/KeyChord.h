#pragma once

#include "Engine_Includes.h"
#include "InputMap.h"
#include "EnumBit.h"

BEGIN(Engine)
class InputManager;
enum class EModifierKey : uint8
{
    None = 0,
    Ctrl = 1 << 0,
    Shift = 1 << 1,
    Alt = 1 << 2
};

ENABLE_BITMASK_OPERATORS(EModifierKey)

struct ENGINE_API KeyChord
{
    Engine::EKeyCode Key = Engine::EKeyCode::Unknown;
    uint8 Modifiers = static_cast<uint8>(EModifierKey::None);

    KeyChord() = default;
    KeyChord(Engine::EKeyCode key, uint8 modifiers = 0)
        : Key(key), Modifiers(modifiers) {
    }
    KeyChord(Engine::EKeyCode key, EModifierKey modifier)
        : Key(key), Modifiers(static_cast<uint8>(modifier)) {
    }

    bool IsValid() const { return Key != Engine::EKeyCode::Unknown; }

    bool HasCtrl() const { return (Modifiers & static_cast<uint8>(EModifierKey::Ctrl)) != 0; }
    bool HasShift() const { return (Modifiers & static_cast<uint8>(EModifierKey::Shift)) != 0; }
    bool HasAlt() const { return (Modifiers & static_cast<uint8>(EModifierKey::Alt)) != 0; }

    bool operator==(const KeyChord& other) const
    {
        return Key == other.Key && Modifiers == other.Modifiers;
    }

    bool operator!=(const KeyChord& other) const { return !(*this == other); }

    bool Evaluate(Engine::InputManager& input) const;
    bool Evaluate(Engine::InputManager& input, bool allowRepeat) const;
    std::string ToString() const;
    static KeyChord FromString(const std::string& str);
    static KeyChord Capture(InputManager& input);
    static KeyChord Capture(InputManager& input, EKeyCode& pendingModifier);
};

END