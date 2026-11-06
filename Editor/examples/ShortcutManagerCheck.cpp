#include "ShortcutManager.h"
#include "InputManager.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <string_view>

namespace
{
    void Tap(Engine::InputManager& input, SDL_Scancode key)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.scancode = key;
        input.ProcessEvent(event);
        event.type = SDL_EVENT_KEY_UP;
        input.ProcessEvent(event);
        input.Update(0.016f);
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << message << '\n';
        return condition;
    }
}

int main()
{
    using namespace Editor;
    if (!SDL_Init(SDL_INIT_EVENTS) || !Engine::InputManager::Create()) return 1;
    ShortcutManagerDesc desc;
    if (!ShortcutManager::Create(&desc)) return 1;
    struct Scope
    {
        ~Scope() { ShortcutManager::Destroy(); Engine::InputManager::Destroy(); SDL_Quit(); }
    } scope;

    auto& manager = ShortcutManager::Get();
    auto& input = Engine::InputManager::Get();
    int globalCount = 0, graphCount = 0, timelineCount = 0;
    bool graphEnabled = true;
    const ShortcutChord deleteChord(Engine::EKeyCode::Delete);
    manager.RegisterAction({ .Id = "Test.Global", .Context = EShortcutContext::Global,
        .DefaultChord = deleteChord, .ExecuteHandler = [&]() { ++globalCount; } });
    manager.RegisterAction({ .Id = "Test.Graph", .Context = EShortcutContext::GraphEditor,
        .DefaultChord = deleteChord, .ExecuteHandler = [&]() { ++graphCount; },
        .CanExecute = [&]() { return graphEnabled; } });
    manager.RegisterAction({ .Id = "Test.Timeline", .Context = EShortcutContext::AnimationTimeline,
        .DefaultChord = deleteChord, .ExecuteHandler = [&]() { ++timelineCount; } });
    manager.RegisterAction({ .Id = "Test.OtherGraph", .Context = EShortcutContext::GraphEditor,
        .DefaultChord = ShortcutChord(Engine::EKeyCode::B), .ExecuteHandler = []() {} });

    if (!Check(manager.FindConflict(deleteChord, EShortcutContext::GraphEditor, "Test.Graph").empty(),
        "Independent contexts and Global fallback must not be reported as conflicts.")) return 2;

    Tap(input, SDL_SCANCODE_DELETE);
    manager.ProcessShortcuts(EShortcutContext::GraphEditor);
    if (!Check(graphCount == 1 && globalCount == 0, "Local command must take precedence over Global.")) return 3;
    graphEnabled = false;
    manager.ProcessShortcuts(EShortcutContext::GraphEditor);
    if (!Check(graphCount == 1 && globalCount == 0, "Disabled local command must consume its matching chord.")) return 4;
    manager.ProcessShortcuts(EShortcutContext::AnimationTimeline);
    if (!Check(timelineCount == 1 && globalCount == 0, "Only the active local context may run.")) return 5;
    manager.ProcessShortcuts(EShortcutContext::Global);
    if (!Check(globalCount == 1, "Global command must run without a local context.")) return 6;

    manager.ProcessShortcuts(EShortcutContext::Global, true);
    manager.SetCapturing(true);
    manager.ProcessShortcuts();
    manager.SetCapturing(false);
    manager.SuppressNextProcess();
    manager.ProcessShortcuts();
    if (!Check(globalCount == 1, "UI suppression and key capture must block commands.")) return 7;
    input.Update(0.016f);
    manager.ProcessShortcuts();
    if (!Check(globalCount == 1, "A released key must not execute again in the next frame.")) return 8;

    manager.Rebind("Test.Graph", ShortcutChord(Engine::EKeyCode::B));
    const char* hint = manager.GetShortcutHint("Test.Graph");
    if (!Check(hint && std::string_view(hint) == "B" && !manager.GetShortcutHint("Test.OtherGraph"),
        "Rebinding must update the hint and remove only same-context conflicts.")) return 9;
    graphEnabled = true;
    Tap(input, SDL_SCANCODE_DELETE);
    manager.ProcessShortcuts(EShortcutContext::GraphEditor);
    if (!Check(globalCount == 2 && graphCount == 1, "Old local chord must stop executing after rebind.")) return 10;
    Tap(input, SDL_SCANCODE_B);
    manager.ProcessShortcuts(EShortcutContext::GraphEditor);
    if (!Check(graphCount == 2, "New local chord must execute through InputManager.")) return 11;
    manager.Rebind("Test.Graph", ShortcutChord());
    if (!Check(!manager.GetShortcutHint("Test.Graph"), "Unbound commands must not advertise a shortcut.")) return 12;
    manager.ResetAllBindings();
    if (!Check(manager.GetActiveChord("Test.Graph") == deleteChord, "Reset must restore the default chord.")) return 13;
    std::cout << "Shortcut context priority, disabled-local isolation, suppression, rebinding and hint checks passed.\n";
    return 0;
}
