#pragma once

#include "InputMap.h"

BEGIN(Engine)

EKeyCode SDLKeyToEngineKeyCode(SDL_Scancode sdlScancode)
{
    switch (sdlScancode)
    {
        // 알파벳
    case SDL_SCANCODE_A: return EKeyCode::A;
    case SDL_SCANCODE_B: return EKeyCode::B;
    case SDL_SCANCODE_C: return EKeyCode::C;
    case SDL_SCANCODE_D: return EKeyCode::D;
    case SDL_SCANCODE_E: return EKeyCode::E;
    case SDL_SCANCODE_F: return EKeyCode::F;
    case SDL_SCANCODE_G: return EKeyCode::G;
    case SDL_SCANCODE_H: return EKeyCode::H;
    case SDL_SCANCODE_I: return EKeyCode::I;
    case SDL_SCANCODE_J: return EKeyCode::J;
    case SDL_SCANCODE_K: return EKeyCode::K;
    case SDL_SCANCODE_L: return EKeyCode::L;
    case SDL_SCANCODE_M: return EKeyCode::M;
    case SDL_SCANCODE_N: return EKeyCode::N;
    case SDL_SCANCODE_O: return EKeyCode::O;
    case SDL_SCANCODE_P: return EKeyCode::P;
    case SDL_SCANCODE_Q: return EKeyCode::Q;
    case SDL_SCANCODE_R: return EKeyCode::R;
    case SDL_SCANCODE_S: return EKeyCode::S;
    case SDL_SCANCODE_T: return EKeyCode::T;
    case SDL_SCANCODE_U: return EKeyCode::U;
    case SDL_SCANCODE_V: return EKeyCode::V;
    case SDL_SCANCODE_W: return EKeyCode::W;
    case SDL_SCANCODE_X: return EKeyCode::X;
    case SDL_SCANCODE_Y: return EKeyCode::Y;
    case SDL_SCANCODE_Z: return EKeyCode::Z;

        // 숫자 (상단 숫자키)
    case SDL_SCANCODE_0: return EKeyCode::Num0;
    case SDL_SCANCODE_1: return EKeyCode::Num1;
    case SDL_SCANCODE_2: return EKeyCode::Num2;
    case SDL_SCANCODE_3: return EKeyCode::Num3;
    case SDL_SCANCODE_4: return EKeyCode::Num4;
    case SDL_SCANCODE_5: return EKeyCode::Num5;
    case SDL_SCANCODE_6: return EKeyCode::Num6;
    case SDL_SCANCODE_7: return EKeyCode::Num7;
    case SDL_SCANCODE_8: return EKeyCode::Num8;
    case SDL_SCANCODE_9: return EKeyCode::Num9;

        // 넘패드
    case SDL_SCANCODE_KP_0: return EKeyCode::Numpad0;
    case SDL_SCANCODE_KP_1: return EKeyCode::Numpad1;
    case SDL_SCANCODE_KP_2: return EKeyCode::Numpad2;
    case SDL_SCANCODE_KP_3: return EKeyCode::Numpad3;
    case SDL_SCANCODE_KP_4: return EKeyCode::Numpad4;
    case SDL_SCANCODE_KP_5: return EKeyCode::Numpad5;
    case SDL_SCANCODE_KP_6: return EKeyCode::Numpad6;
    case SDL_SCANCODE_KP_7: return EKeyCode::Numpad7;
    case SDL_SCANCODE_KP_8: return EKeyCode::Numpad8;
    case SDL_SCANCODE_KP_9: return EKeyCode::Numpad9;
    case SDL_SCANCODE_KP_PLUS: return EKeyCode::NumpadAdd;
    case SDL_SCANCODE_KP_MINUS: return EKeyCode::NumpadSubtract;
    case SDL_SCANCODE_KP_MULTIPLY: return EKeyCode::NumpadMultiply;
    case SDL_SCANCODE_KP_DIVIDE: return EKeyCode::NumpadDivide;
    case SDL_SCANCODE_KP_ENTER: return EKeyCode::NumpadEnter;

        // 기능키
    case SDL_SCANCODE_F1: return EKeyCode::F1;
    case SDL_SCANCODE_F2: return EKeyCode::F2;
    case SDL_SCANCODE_F3: return EKeyCode::F3;
    case SDL_SCANCODE_F4: return EKeyCode::F4;
    case SDL_SCANCODE_F5: return EKeyCode::F5;
    case SDL_SCANCODE_F6: return EKeyCode::F6;
    case SDL_SCANCODE_F7: return EKeyCode::F7;
    case SDL_SCANCODE_F8: return EKeyCode::F8;
    case SDL_SCANCODE_F9: return EKeyCode::F9;
    case SDL_SCANCODE_F10: return EKeyCode::F10;
    case SDL_SCANCODE_F11: return EKeyCode::F11;
    case SDL_SCANCODE_F12: return EKeyCode::F12;

        // 특수키
    case SDL_SCANCODE_ESCAPE: return EKeyCode::Escape;
    case SDL_SCANCODE_RETURN: return EKeyCode::Enter;
    case SDL_SCANCODE_SPACE: return EKeyCode::Space;
    case SDL_SCANCODE_BACKSPACE: return EKeyCode::Backspace;
    case SDL_SCANCODE_TAB: return EKeyCode::Tab;
    case SDL_SCANCODE_CAPSLOCK: return EKeyCode::CapsLock;

        // 방향키
    case SDL_SCANCODE_UP: return EKeyCode::Up;
    case SDL_SCANCODE_DOWN: return EKeyCode::Down;
    case SDL_SCANCODE_LEFT: return EKeyCode::Left;
    case SDL_SCANCODE_RIGHT: return EKeyCode::Right;

        // 수식키
    case SDL_SCANCODE_LSHIFT: return EKeyCode::LShift;
    case SDL_SCANCODE_RSHIFT: return EKeyCode::RShift;
    case SDL_SCANCODE_LCTRL: return EKeyCode::LCtrl;
    case SDL_SCANCODE_RCTRL: return EKeyCode::RCtrl;
    case SDL_SCANCODE_LALT: return EKeyCode::LAlt;
    case SDL_SCANCODE_RALT: return EKeyCode::RAlt;

        // 네비게이션
    case SDL_SCANCODE_INSERT: return EKeyCode::Insert;
    case SDL_SCANCODE_DELETE: return EKeyCode::Delete;
    case SDL_SCANCODE_HOME: return EKeyCode::Home;
    case SDL_SCANCODE_END: return EKeyCode::End;
    case SDL_SCANCODE_PAGEUP: return EKeyCode::PageUp;
    case SDL_SCANCODE_PAGEDOWN: return EKeyCode::PageDown;
    case SDL_SCANCODE_PRINTSCREEN: return EKeyCode::PrintScreen;

        // 기호
	case SDL_SCANCODE_EQUALS: return EKeyCode::Equals;
	case SDL_SCANCODE_MINUS: return EKeyCode::Minus;
    case SDL_SCANCODE_SEMICOLON: return EKeyCode::Semicolon;
    case SDL_SCANCODE_APOSTROPHE: return EKeyCode::Apostrophe;
    case SDL_SCANCODE_COMMA: return EKeyCode::Comma;
    case SDL_SCANCODE_PERIOD: return EKeyCode::Period;
    case SDL_SCANCODE_SLASH: return EKeyCode::Slash;
    case SDL_SCANCODE_BACKSLASH: return EKeyCode::Backslash;
    case SDL_SCANCODE_GRAVE: return EKeyCode::GraveAccent;
    case SDL_SCANCODE_LEFTBRACKET: return EKeyCode::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return EKeyCode::RightBracket;

    default: return EKeyCode::Unknown;
    }
}
EKeyCode StringToEngineKeyCode(const string& keyString)
{
    static const std::unordered_map<std::string, EKeyCode> StringToKeyMap =
    {
        // 알파벳
        {"a", EKeyCode::A}, {"b", EKeyCode::B}, {"c", EKeyCode::C}, {"d", EKeyCode::D},
        {"e", EKeyCode::E}, {"f", EKeyCode::F}, {"g", EKeyCode::G}, {"h", EKeyCode::H},
        {"i", EKeyCode::I}, {"j", EKeyCode::J}, {"k", EKeyCode::K}, {"l", EKeyCode::L},
        {"m", EKeyCode::M}, {"n", EKeyCode::N}, {"o", EKeyCode::O}, {"p", EKeyCode::P},
        {"q", EKeyCode::Q}, {"r", EKeyCode::R}, {"s", EKeyCode::S}, {"t", EKeyCode::T},
        {"u", EKeyCode::U}, {"v", EKeyCode::V}, {"w", EKeyCode::W}, {"x", EKeyCode::X},
        {"y", EKeyCode::Y}, {"z", EKeyCode::Z},

        // 숫자 (이름 및 실제 char 모두 매핑)
        {"num0", EKeyCode::Num0}, {"0", EKeyCode::Num0},
        {"num1", EKeyCode::Num1}, {"1", EKeyCode::Num1},
        {"num2", EKeyCode::Num2}, {"2", EKeyCode::Num2},
        {"num3", EKeyCode::Num3}, {"3", EKeyCode::Num3},
        {"num4", EKeyCode::Num4}, {"4", EKeyCode::Num4},
        {"num5", EKeyCode::Num5}, {"5", EKeyCode::Num5},
        {"num6", EKeyCode::Num6}, {"6", EKeyCode::Num6},
        {"num7", EKeyCode::Num7}, {"7", EKeyCode::Num7},
        {"num8", EKeyCode::Num8}, {"8", EKeyCode::Num8},
        {"num9", EKeyCode::Num9}, {"9", EKeyCode::Num9},

        // 넘패드 (이름 및 편의를 위한 연산자 기호 매핑)
        {"numpad0", EKeyCode::Numpad0}, {"numpad1", EKeyCode::Numpad1}, {"numpad2", EKeyCode::Numpad2},
        {"numpad3", EKeyCode::Numpad3}, {"numpad4", EKeyCode::Numpad4}, {"numpad5", EKeyCode::Numpad5},
        {"numpad6", EKeyCode::Numpad6}, {"numpad7", EKeyCode::Numpad7}, {"numpad8", EKeyCode::Numpad8},
        {"numpad9", EKeyCode::Numpad9}, {"numpadadd", EKeyCode::NumpadAdd},
        {"numpadsubtract", EKeyCode::NumpadSubtract},
        {"numpadmultiply", EKeyCode::NumpadMultiply}, {"*", EKeyCode::NumpadMultiply},
        {"numpaddivide", EKeyCode::NumpadDivide}, {"numpadenter", EKeyCode::NumpadEnter},

        // 기능키
        {"f1", EKeyCode::F1}, {"f2", EKeyCode::F2}, {"f3", EKeyCode::F3}, {"f4", EKeyCode::F4},
        {"f5", EKeyCode::F5}, {"f6", EKeyCode::F6}, {"f7", EKeyCode::F7}, {"f8", EKeyCode::F8},
        {"f9", EKeyCode::F9}, {"f10", EKeyCode::F10}, {"f11", EKeyCode::F11}, {"f12", EKeyCode::F12},

        // 특수키 (이름 및 스페이스바 기호 매핑)
        {"escape", EKeyCode::Escape}, {"enter", EKeyCode::Enter},
        {"space", EKeyCode::Space}, {" ", EKeyCode::Space},
        {"backspace", EKeyCode::Backspace}, {"tab", EKeyCode::Tab}, {"capslock", EKeyCode::CapsLock},

        // 방향키
        {"up", EKeyCode::Up}, {"down", EKeyCode::Down},
        {"left", EKeyCode::Left}, {"right", EKeyCode::Right},

        // 수식키
        {"lshift", EKeyCode::LShift}, {"rshift", EKeyCode::RShift},
        {"lctrl", EKeyCode::LCtrl}, {"rctrl", EKeyCode::RCtrl},
        {"lalt", EKeyCode::LAlt}, {"ralt", EKeyCode::RAlt},

        // 네비게이션
        {"insert", EKeyCode::Insert}, {"delete", EKeyCode::Delete},
        {"home", EKeyCode::Home}, {"end", EKeyCode::End},
        {"pageup", EKeyCode::PageUp}, {"pagedown", EKeyCode::PageDown},
        {"printscreen", EKeyCode::PrintScreen},

        // 기호 (이름 및 실제 char 모두 매핑)
        {"equals", EKeyCode::Equals}, {"=", EKeyCode::Equals},
        {"minus", EKeyCode::Minus}, {"-", EKeyCode::Minus},
        {"semicolon", EKeyCode::Semicolon}, {";", EKeyCode::Semicolon},
        {"apostrophe", EKeyCode::Apostrophe}, {"'", EKeyCode::Apostrophe},
        {"comma", EKeyCode::Comma}, {",", EKeyCode::Comma},
        {"period", EKeyCode::Period}, {".", EKeyCode::Period},
        {"slash", EKeyCode::Slash}, {"/", EKeyCode::Slash},
        {"backslash", EKeyCode::Backslash}, {"\\", EKeyCode::Backslash},
        {"graveaccent", EKeyCode::GraveAccent}, {"`", EKeyCode::GraveAccent},
        {"leftbracket", EKeyCode::LeftBracket}, {"[", EKeyCode::LeftBracket},
        {"rightbracket", EKeyCode::RightBracket}, {"]", EKeyCode::RightBracket}
    };

    // 대소문자 구분을 없애기 위해 입력 문자열을 모두 소문자로 변환
    std::string lowerStr = keyString;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
        [](unsigned char c) { return std::tolower(c); });

    auto it = StringToKeyMap.find(lowerStr);
    if (it != StringToKeyMap.end())
    {
        return it->second;
    }

    return EKeyCode::Unknown;
}
EMouseButton SDLButtonToEngineMouseButton(uint8 sdlButton)
{
    switch (sdlButton)
    {
    case SDL_BUTTON_LEFT: return EMouseButton::Left;
    case SDL_BUTTON_RIGHT: return EMouseButton::Right;
    case SDL_BUTTON_MIDDLE: return EMouseButton::Middle;
    case SDL_BUTTON_X1: return EMouseButton::X1;
    case SDL_BUTTON_X2: return EMouseButton::X2;
    default: return EMouseButton::Count; // 알 수 없는 버튼은 Count (최대 범위 초과 방지용)
    }
}
EMouseButton StringToEngineMouseButton(const string& buttonString)
{
    static const std::unordered_map<std::string, EMouseButton> StringToMouseMap =
    {
        {"Left", EMouseButton::Left},
        {"Right", EMouseButton::Right},
        {"Middle", EMouseButton::Middle},
        {"X1", EMouseButton::X1},
        {"X2", EMouseButton::X2}
    };

    auto it = StringToMouseMap.find(buttonString);
    if (it != StringToMouseMap.end())
    {
        return it->second;
    }

    return EMouseButton::Count;
}
END