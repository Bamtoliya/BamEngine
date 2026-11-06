#include "InputManager.h"
#include <SDL3/SDL.h>
#include <iostream>
#include "InputActionSettings.h"

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

    InputActionSettings settings;
    settings.Maps.push_back({
        "Gameplay",
        { { "Jump", "Button" }, { "ModifiedJump", "Button" } },
        { { "Jump", "J" }, { "Jump", "K" }, { "Jump", "Mouse:Right" }, { "ModifiedJump", "Ctrl+J" } }
        });

    if (!input.SetInputActionSettings(settings)) return 11;
    input.EvaluateActionMap("Gameplay", true);

    auto invalidSettings = settings;
    invalidSettings.Maps.front().Bindings.push_back({ "MissingAction", "L" });
    if (input.SetInputActionSettings(invalidSettings)) return 12;

    const auto tapKey = [&input](SDL_Scancode scancode, SDL_Keymod modifiers = SDL_KMOD_NONE)
        {
            SDL_Event keyEvent{};
            keyEvent.type = SDL_EVENT_KEY_DOWN;
            keyEvent.key.scancode = scancode;
            keyEvent.key.mod = modifiers;
            input.ProcessEvent(keyEvent);

            keyEvent.type = SDL_EVENT_KEY_UP;
            input.ProcessEvent(keyEvent);
            input.Update(0.016f);
        };

    tapKey(SDL_SCANCODE_J);
    input.EvaluateActionMap("Gameplay", true);
    if (!input.WasActionPressed("Gameplay", "Jump") ||
        input.WasActionPressed("Gameplay", "ModifiedJump")) return 13;

    tapKey(SDL_SCANCODE_K);
    input.EvaluateActionMap("Gameplay", true);
    if (!input.WasActionPressed("Gameplay", "Jump")) return 14;

    tapKey(SDL_SCANCODE_J, SDL_KMOD_LCTRL);
    input.EvaluateActionMap("Gameplay", true);
    if (input.WasActionPressed("Gameplay", "Jump") ||
        !input.WasActionPressed("Gameplay", "ModifiedJump")) return 15;

    tapKey(SDL_SCANCODE_J);
    input.EvaluateActionMap("Gameplay", false);
    if (input.WasActionPressed("Gameplay", "Jump") ||
        input.IsActionHeld("Gameplay", "Jump")) return 16;

    input.Update(0.016f);
    input.EvaluateActionMap("Gameplay", true);

    SDL_Event mouseTap{};
    mouseTap.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    mouseTap.button.button = SDL_BUTTON_RIGHT;
    input.ProcessEvent(mouseTap);

    mouseTap.type = SDL_EVENT_MOUSE_BUTTON_UP;
    input.ProcessEvent(mouseTap);
    input.Update(0.016f);

    input.EvaluateActionMap("Gameplay", true);
    if (!input.WasActionPressed("Gameplay", "Jump") ||
        input.IsActionHeld("Gameplay", "Jump")) return 17;

    constexpr uint32 leftMouseOnly = uint32{ 1 } << static_cast<uint32>(EMouseButton::Left);

    mouseTap.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    input.ProcessEvent(mouseTap);
    mouseTap.type = SDL_EVENT_MOUSE_BUTTON_UP;
    input.ProcessEvent(mouseTap);
    input.Update(0.016f);

    input.EvaluateActionMap("Gameplay", true, { .AllowedButtons = leftMouseOnly });
    if (input.WasActionPressed("Gameplay", "Jump") ||
        input.IsActionHeld("Gameplay", "Jump")) return 18;

    tapKey(SDL_SCANCODE_J);
    input.EvaluateActionMap("Gameplay", true, { .AllowedButtons = leftMouseOnly });
    if (!input.WasActionPressed("Gameplay", "Jump")) return 19;

    if (!input.WasActionReleased("Gameplay", "Jump")) return 20;

    input.Update(0.016f);
    input.EvaluateActionMap("Gameplay", true);
    if (input.WasActionReleased("Gameplay", "Jump")) return 21;

    tapKey(SDL_SCANCODE_J);
    input.EvaluateActionMap("Gameplay", false);
    if (input.WasActionReleased("Gameplay", "Jump")) return 22;

    std::cout << "Input action validation, multiple bindings, chord priority and disabled map checks passed.\n";

    std::cout << "Input frame edges, short taps, repeat filtering, all mouse buttons and wheel checks passed.\n";
    return 0;
}
