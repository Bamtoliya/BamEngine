#pragma once

#include "Base.h"

BEGIN(Engine)
enum class EKeyCode : uint32
{
    Unknown = 0,

    // 알파벳 (A-Z)
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 숫자 (상단 숫자키 0-9)
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // 넘패드 (Numpad)
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide, NumpadEnter,

    // 기능키 (F1-F12)
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // 특수키
    Escape,
    Enter,
    Space,
    Backspace,
    Tab,
    CapsLock,

    // 방향키
    Up,
    Down,
    Left,
    Right,

    // 수식키 (Modifiers)
    LShift,
    RShift,
    LCtrl,
    RCtrl,
    LAlt,
    RAlt,
    
    // 네비게이션 및 기타
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    PrintScreen,

    // 기호 키
    Equals,       // =
    Minus,        // -
    Semicolon,    // ;
    Apostrophe,   // '
    Comma,        // ,
    Period,       // .
    Slash,        // /
    Backslash,    // "\" 
    GraveAccent,  // ` (틸드 ~)
    LeftBracket,  // [
    RightBracket, // ]

    Count // 전체 키보드 키 개수 (배열 크기 할당 시 유용함)
};
enum class EMouseButton : uint32
{
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,       // 마우스 4번 버튼 (보통 뒤로 가기)
    X2 = 4,       // 마우스 5번 버튼 (보통 앞으로 가기)

    Count
};
EKeyCode SDLKeyToEngineKeyCode(SDL_Scancode sdlScancode);
EKeyCode StringToEngineKeyCode(const string& keyString);
EMouseButton SDLButtonToEngineMouseButton(uint8 sdlButton);
EMouseButton StringToEngineMouseButton(const string& buttonString);
END