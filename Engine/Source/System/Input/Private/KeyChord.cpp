#include "KeyChord.h"
#include "InputManager.h"

bool KeyChord::Evaluate(Engine::InputManager& input) const
{
    return Evaluate(input, false);
}

bool KeyChord::Evaluate(Engine::InputManager& input, bool allowRepeat) const
{
    if (!IsValid())
        return false;

    if (input.IsCtrlPressed() != HasCtrl()) return false;
    if (input.IsShiftPressed() != HasShift()) return false;
    if (input.IsAltPressed() != HasAlt()) return false;

    return input.IsKeyButtonDown(Key) || (allowRepeat && input.IsKeyButtonRepeated(Key));
}

std::string KeyChord::ToString() const
{
    if (!IsValid())
        return "None";

    std::string result;
    if (HasCtrl())  result += "Ctrl+";
    if (HasShift()) result += "Shift+";
    if (HasAlt())   result += "Alt+";

    result += Engine::EngineKeyCodeToString(Key);
    return result;
}

KeyChord KeyChord::FromString(const std::string& str)
{
    if (str.empty() || str == "None")
        return KeyChord();

    uint8 modifiers = 0;
    std::string keyText = str;

    while (true)
    {
        const auto separator = keyText.find('+');
        if (separator == std::string::npos)
            break;

        std::string token = keyText.substr(0, separator);
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        uint8 modifier = 0;

        if (token == "ctrl")
            modifier = static_cast<uint8>(EModifierKey::Ctrl);
        else if (token == "shift")
            modifier = static_cast<uint8>(EModifierKey::Shift);
        else if (token == "alt")
            modifier = static_cast<uint8>(EModifierKey::Alt);
        else
            break;

        if ((modifiers & modifier) != 0)
            return KeyChord();

        modifiers |= modifier;
        keyText.erase(0, separator + 1);
    }

    const auto key = Engine::StringToEngineKeyCode(keyText);
    if (key == Engine::EKeyCode::Unknown)
        return KeyChord();

    return KeyChord(key, modifiers);
}

namespace
{
    using Engine::EKeyCode;

    constexpr std::array ChordModifierKeys{
        EKeyCode::LCtrl, EKeyCode::RCtrl,
        EKeyCode::LShift, EKeyCode::RShift,
        EKeyCode::LAlt, EKeyCode::RAlt
    };
}

Engine::KeyChord Engine::KeyChord::Capture(InputManager& input)
{
    for (uint32 value = 1; value < static_cast<uint32>(EKeyCode::Count); ++value)
    {
        const auto key = static_cast<EKeyCode>(value);
        if (std::ranges::find(ChordModifierKeys, key) != ChordModifierKeys.end())
            continue;

        if (input.IsKeyButtonDown(key))
            return { key, input.GetKeyDownModifiers(key) };
    }

    return {};
}

Engine::KeyChord Engine::KeyChord::Capture(InputManager& input, EKeyCode& pendingModifier)
{
    if (const auto chord = Capture(input); chord.IsValid())
    {
        pendingModifier = EKeyCode::Unknown;
        return chord;
    }

    EKeyCode pressedModifier = EKeyCode::Unknown;
    int activeModifierCount = 0;
    for (const auto key : ChordModifierKeys)
    {
        const bool wentDown = input.IsKeyButtonDown(key);
        if (wentDown)
            pressedModifier = key;

        if (wentDown || input.IsKeyButtonPressed(key))
            ++activeModifierCount;
    }

    const bool changedCandidate = pendingModifier != EKeyCode::Unknown &&
        pressedModifier != EKeyCode::Unknown && pressedModifier != pendingModifier;
    if (activeModifierCount > 1 || changedCandidate)
    {
        pendingModifier = EKeyCode::Unknown;
        return {};
    }

    if (pressedModifier != EKeyCode::Unknown)
        pendingModifier = pressedModifier;

    if (pendingModifier == EKeyCode::Unknown || !input.IsKeyButtonUp(pendingModifier))
        return {};

    const KeyChord chord{ pendingModifier };
    pendingModifier = EKeyCode::Unknown;
    return chord;
}