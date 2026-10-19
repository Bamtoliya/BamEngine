#include "InputManager.h"
#include <SDL3/SDL.h>
#include <iostream>

int main()
{
    using namespace Engine;
    if (!SDL_Init(SDL_INIT_EVENTS) || !InputManager::Create()) return 1;
    struct Scope { ~Scope() { InputManager::Destroy(); SDL_Quit(); } } scope;
    auto& input = InputManager::Get();
    input.Update(0.016f);
    if (input.IsKeyButtonPressed(EKeyCode::A) || input.IsMouseButtonPressed(EMouseButton::X2)) return 2;

    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_Z;
    input.ProcessEvent(event);
    event.type = SDL_EVENT_KEY_UP;
    input.ProcessEvent(event);
    input.Update(0.016f);
    if (!input.IsKeyButtonDown(EKeyCode::Z) || !input.IsKeyButtonUp("Z")) return 3;
    input.Update(0.016f);
    if (input.IsKeyButtonDown(EKeyCode::Z) || input.IsKeyButtonUp(EKeyCode::Z)) return 4;

    event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_Z;
    event.key.repeat = true;
    input.ProcessEvent(event);
    input.Update(0.016f);
    if (input.IsKeyButtonDown(EKeyCode::Z)) return 5;

    for (uint8 button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; ++button)
    {
        event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = button;
        input.ProcessEvent(event);
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        input.ProcessEvent(event);
        input.Update(0.016f);
        constexpr EMouseButton buttons[]{ EMouseButton::Left, EMouseButton::Middle, EMouseButton::Right,
            EMouseButton::X1, EMouseButton::X2 };
        const auto mapped = buttons[button - SDL_BUTTON_LEFT];
        if (!input.IsMouseButtonDown(mapped) || !input.IsMouseButtonUp(mapped)) return 6;
        input.Update(0.016f);
        if (input.IsMouseButtonDown(mapped) || input.IsMouseButtonUp(mapped)) return 7;
    }

    event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.x = -0.5f;
    event.wheel.y = 1.25f;
    input.ProcessEvent(event);
    input.ProcessEvent(event);
    input.Update(0.016f);
    if (input.GetMouseScrollDelta() != vec2(-1.0f, 2.5f)) return 8;
    input.Update(0.016f);
    if (input.GetMouseScrollDelta() != vec2(0.0f)) return 9;
    if (input.IsMouseButtonDown(EMouseButton::Count) || input.IsMouseButtonDragging(EMouseButton::Count) ||
        input.IsKeyButtonPressed(static_cast<EKeyCode>(SDL_SCANCODE_COUNT))) return 10;
    std::cout << "Input frame edges, short taps, repeat filtering, all mouse buttons and wheel checks passed.\n";
    return 0;
}
