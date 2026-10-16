#include "UIEventSystem.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "Reflection/ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <iostream>
#include <limits>
#include <memory>

namespace
{
    using namespace Engine;
    using Type = EUIPointerEventType;
    struct SceneRelease { void operator()(Scene* scene) const { if (scene) scene->Release(); } };

    struct Fixture
    {
        std::unique_ptr<Scene, SceneRelease> scene{ Scene::Create() };
        entt::entity canvas;
        entt::entity button;
        Fixture()
        {
            auto& world = scene->GetRegistry();
            canvas = scene->CreateEntity().GetEntityHandle();
            world.emplace<CanvasComponent>(canvas).referenceResolution = { 400.0f, 300.0f };
            world.emplace<FlagComponent>(canvas);
            world.emplace<HierarchyComponent>(canvas);
            button = Add(canvas, true);
        }
        entt::entity Add(entt::entity parent, bool isButton)
        {
            auto& world = scene->GetRegistry();
            const auto entity = scene->CreateEntity().GetEntityHandle();
            world.emplace<FlagComponent>(entity);
            world.emplace<HierarchyComponent>(entity).parent = parent;
            auto& rect = world.emplace<RectTransformComponent>(entity);
            rect.anchorMin = rect.anchorMax = rect.pivot = glm::vec2(0.0f);
            rect.anchoredPosition = { 10.0f, 10.0f };
            rect.sizeDelta = { 100.0f, 60.0f };
            world.emplace<UIImageComponent>(entity);
            if (isButton) world.emplace<UIButtonComponent>(entity);
            world.get<HierarchyComponent>(parent).children.push_back(entity);
            return entity;
        }
    };

    bool Feed(UIEventSystem& system, glm::vec2 position, bool down = false, bool held = false, bool up = false,
        bool inside = true, bool focused = true)
    {
        return system.Update({ position, inside, focused, held, down, up });
    }
    bool Types(const std::vector<UIPointerEvent>& events, std::initializer_list<Type> types)
    {
        return events.size() == types.size() && std::equal(events.begin(), events.end(), types.begin(),
            [](const auto& event, Type type) { return event.type == type; });
    }
    int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
}

int main()
{
    using namespace Engine;
    if (!ReflectionRegistry::Create()) return Fail("Reflection initialization failed.");
    struct ReflectionRelease { ~ReflectionRelease() { ReflectionRegistry::Destroy(); } } reflectionRelease;
    Fixture fixture;
    auto& world = fixture.scene->GetRegistry();
    const glm::vec2 inside{ 30.0f, 30.0f }, outside{ 200.0f, 200.0f };
    std::vector<UIPointerEvent> events;
    UIEventSystem system(*fixture.scene, fixture.canvas);
    system.OnPointerEvent().AddLambda([&](const auto& event) { events.push_back(event); });

    Feed(system, inside);
    if (!Types(events, { Type::Enter }) || !system.IsPointerOverUI()) return Fail("Hover entry failed.");
    if (UIEventSystem::GetButtonTint(world, fixture.button) !=
        world.get<UIButtonComponent>(fixture.button).hoveredColor)
        return Fail("Hover tint failed.");
    const auto original = world.get<UIImageComponent>(fixture.button).color;
    events.clear();
    Feed(system, inside, true, true);
    if (!Types(events, { Type::Down }) || !system.HasPointerCapture()) return Fail("Pointer capture failed.");
    if (UIEventSystem::GetButtonTint(world, fixture.button) !=
        world.get<UIButtonComponent>(fixture.button).pressedColor)
        return Fail("Pressed tint failed.");

    archive::JsonArchive archive;
    if (!fixture.scene->WriteArchive(archive)) return Fail("Interaction scene serialization failed.");
    Scene* restoredRaw = nullptr;
    if (!Scene::CreateFromArchive(archive, restoredRaw)) return Fail("Interaction scene restoration failed.");
    std::unique_ptr<Scene, SceneRelease> restored(restoredRaw);
    const auto buttonId = world.get<IDComponent>(fixture.button).id;
    auto* restoredButton = restored->FindEntityById(buttonId);
    if (!restoredButton || !restored->GetRegistry().all_of<UIButtonComponent>(restoredButton->GetEntityHandle()) ||
        !restored->GetRegistry().view<UIInteractionStateComponent>().empty())
        return Fail("Button settings or transient-state exclusion failed.");

    events.clear();
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Up, Type::Click }) || system.HasPointerCapture() ||
        system.ResolveTarget(events.back()) != fixture.scene->FindEntity(fixture.button))
        return Fail("Click delivery or entity resolution failed.");
    if (world.get<UIImageComponent>(fixture.button).color != original)
        return Fail("Interaction changed authored color.");

    Feed(system, inside, true, true);
    events.clear();
    Feed(system, outside, false, true);
    Feed(system, outside, false, false, true);
    if (!Types(events, { Type::Exit, Type::Up }) || system.HasPointerCapture())
        return Fail("Release outside a button incorrectly clicked or lost release capture.");

    Feed(system, inside);
    Feed(system, inside, true, true);
    events.clear();
    Feed(system, inside, false, true, false, true, false);
    if (!Types(events, { Type::Cancel, Type::Exit }) || system.HasPointerCapture())
        return Fail("Focus loss did not cancel.");
    events.clear();
    Feed(system, inside, false, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Enter })) return Fail("Focus regain invented a click.");

    Feed(system, inside, true, true);
    world.get<UIButtonComponent>(fixture.button).interactable = false;
    events.clear();
    Feed(system, inside, false, true);
    if (!Types(events, { Type::Cancel, Type::Exit }) || !system.IsPointerOverUI())
        return Fail("Disabling did not cancel capture or preserve input blocking.");
    if (UIEventSystem::GetButtonTint(world, fixture.button) !=
        world.get<UIButtonComponent>(fixture.button).disabledColor)
        return Fail("Disabled tint failed.");
    world.get<UIButtonComponent>(fixture.button).interactable = true;
    events.clear();
    Feed(system, inside, false, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Enter })) return Fail("Re-enabling mid-press invented a click.");

    const auto overlay = fixture.Add(fixture.canvas, false);
    events.clear();
    Feed(system, inside, true, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Exit }) || !system.IsPointerOverUI()) return Fail("Image failed to block a button.");
    world.get<UIImageComponent>(overlay).raycastTarget = false;
    events.clear();
    Feed(system, inside);
    Feed(system, inside, true, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Enter, Type::Down, Type::Up, Type::Click }))
        return Fail("Non-raycast image failed to pass input through.");
    world.get<UIImageComponent>(fixture.button).raycastTarget = false;
    fixture.Add(fixture.button, false); // A child graphic resolves to its nearest button ancestor.
    events.clear();
    Feed(system, inside, true, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Down, Type::Up, Type::Click }))
        return Fail("Child graphic did not route to its button.");

    bool reentryRejected = false;
    const auto disable = system.OnPointerEvent().AddLambda([&](const auto& event)
    {
        if (event.type == Type::Up)
        {
            world.get<UIButtonComponent>(fixture.button).interactable = false;
            reentryRejected = !system.Update({}) && !system.Cancel();
        }
    });
    events.clear();
    Feed(system, inside, true, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Down, Type::Up }) || !reentryRejected)
        return Fail("Callback invalidation or reentrancy handling failed.");
    system.OnPointerEvent().Remove(disable);
    world.get<UIButtonComponent>(fixture.button).interactable = true;
    entt::entity popup = entt::null;
    const auto cover = system.OnPointerEvent().AddLambda([&](const auto& event)
    {
        if (event.type == Type::Up) popup = fixture.Add(fixture.canvas, false);
    });
    events.clear();
    Feed(system, inside, true, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Down, Type::Up }) || popup == entt::null)
        return Fail("A popup opened during Up failed to suppress the pending Click.");
    system.OnPointerEvent().Remove(cover);
    world.get<UIImageComponent>(popup).raycastTarget = false;
    const auto remove = system.OnPointerEvent().AddLambda([&](const auto& event)
    {
        if (event.type == Type::Down) world.remove<UIButtonComponent>(fixture.button);
    });
    events.clear();
    Feed(system, inside, true, true);
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Down, Type::Cancel, Type::Exit }))
        return Fail("Removing a pressed button left stale capture or delivered a click.");
    system.OnPointerEvent().Remove(remove);
    world.emplace<UIButtonComponent>(fixture.button);
    Feed(system, inside);
    events.clear();
    Feed(system, inside, true, false, true); // A complete press/release can arrive in one input sample.
    if (!Types(events, { Type::Down, Type::Up, Type::Click })) return Fail("Same-frame click edges were lost.");
    Feed(system, inside, true, true);
    events.clear();
    Feed(system, inside); // The release edge was lost, e.g. while another input layer consumed it.
    if (!Types(events, { Type::Cancel }) || system.HasPointerCapture())
        return Fail("Missing release edge left capture or invented a click.");
    Feed(system, inside, true, true);
    events.clear();
    Feed(system, inside, false, false, true, false);
    if (!Types(events, { Type::Exit, Type::Up })) return Fail("Viewport clipping did not prevent a click.");
    Feed(system, inside);
    Feed(system, inside, true, true);
    events.clear();
    world.get<FlagComponent>(fixture.canvas).flags = EEntityFlag::None;
    Feed(system, inside, false, true);
    if (!Types(events, { Type::Cancel, Type::Exit })) return Fail("Hidden canvas retained interaction.");
    world.get<FlagComponent>(fixture.canvas).flags = EEntityFlag::Default;
    Feed(system, inside); // Rearm only after the held button is released.
    Feed(system, inside, true, true);
    events.clear();
    Feed(system, { std::numeric_limits<float>::quiet_NaN(), 0.0f }, false, true);
    if (!Types(events, { Type::Cancel, Type::Exit })) return Fail("Invalid pointer coordinates retained interaction.");
    Feed(system, inside);
    Feed(system, inside, true, true);
    events.clear();
    const auto saved = world.get<IDComponent>(fixture.button).id;
    world.get<IDComponent>(fixture.button).id = saved + 1000;
    Feed(system, inside, false, true);
    if (system.HasPointerCapture()) return Fail("An entity identity change retained the old press.");
    for (const auto& event : events)
        if (event.targetId == saved && system.ResolveTarget(event))
            return Fail("Stale event resolved to a new identity.");
    system.Cancel();
    world.get<IDComponent>(fixture.button).id = saved;
    if (!world.view<UIInteractionStateComponent>().empty()) return Fail("Cancellation left transient state.");
    Feed(system, inside);
    Feed(system, inside, true, true);
    events.clear();
    auto* deleted = fixture.scene->FindEntity(fixture.button);
    if (!deleted || !fixture.scene->DestroyEntity(*deleted)) return Fail("Button entity deletion failed.");
    Feed(system, inside, false, false, true);
    if (!Types(events, { Type::Cancel, Type::Exit }) || system.HasPointerCapture())
        return Fail("A deleted entity retained capture or received a release/click.");
    for (const auto& event : events)
        if (system.ResolveTarget(event)) return Fail("A deleted event target still resolved.");

    {
        Fixture retained;
        Scene* owner = retained.scene.release();
        UIEventSystem retainedSystem(*owner, retained.canvas);
        owner->Release(); // The UI input owner now holds the only scene reference.
        Feed(retainedSystem, inside);
        Feed(retainedSystem, inside, true, true);
        if (!retainedSystem.HasPointerCapture()) return Fail("UI input failed to retain its scene.");
    }

    {
        Fixture ordered;
        auto& registry = ordered.scene->GetRegistry();
        const auto second = ordered.Add(ordered.canvas, true);
        std::vector<UIPointerEvent> hits;
        UIEventSystem orderedSystem(*ordered.scene, ordered.canvas);
        orderedSystem.OnPointerEvent().AddLambda([&](const auto& event)
        {
            if (event.type == Type::Click) hits.push_back(event);
        });
        const auto click = [&]
        {
            hits.clear();
            Feed(orderedSystem, inside);
            Feed(orderedSystem, inside, true, true);
            Feed(orderedSystem, inside, false, false, true);
        };
        click();
        if (hits.size() != 1 || hits[0].target != second) return Fail("Default sibling hit order failed.");
        registry.emplace<UIRenderOrderComponent>(ordered.button).zIndex = 5;
        click();
        if (hits.size() != 1 || hits[0].target != ordered.button)
            return Fail("Hit testing did not follow an increased Z-index.");
        registry.emplace<UIRenderOrderComponent>(second).zIndex = 5;
        click();
        if (hits.size() != 1 || hits[0].target != second) return Fail("Equal Z-index lost stable hit order.");
        registry.get<UIRenderOrderComponent>(second).zIndex = -5;
        click();
        if (hits.size() != 1 || hits[0].target != ordered.button) return Fail("Negative Z-index hit order failed.");
        const auto blocker = ordered.Add(ordered.canvas, false);
        registry.emplace<UIRenderOrderComponent>(blocker).zIndex = 9;
        click();
        if (!hits.empty() || !orderedSystem.IsPointerOverUI()) return Fail("High Z-index image did not block input.");
        registry.get<UIImageComponent>(blocker).raycastTarget = false;
        click();
        if (hits.size() != 1 || hits[0].target != ordered.button)
            return Fail("Non-raycast image with a high Z-index did not pass input through.");
    }

    // The publisher must tolerate subscription changes and retain mutable callback state.
    MulticastDelegate<> signal;
    std::vector<int> calls;
    DelegateHandle first = 0, second = 0;
    first = signal.AddLambda([&]
    {
        calls.push_back(1);
        signal.Remove(first);
        signal.Remove(second);
        signal.AddLambda([&] { calls.push_back(3); });
    });
    second = signal.AddLambda([&] { calls.push_back(2); });
    signal.Broadcast();
    signal.Broadcast();
    if (calls != std::vector<int>{ 1, 3 }) return Fail("Delegate mutation invalidated dispatch.");
    signal.Clear();
    int mutableState = 0;
    signal.AddLambda([count = 0, &mutableState]() mutable { mutableState = ++count; });
    signal.Broadcast();
    signal.Broadcast();
    if (mutableState != 2) return Fail("Dispatch copied away mutable callback state.");
    signal.Clear();
    signal.AddLambda([&] { signal.Clear(); });
    signal.AddLambda([&] { calls.push_back(99); });
    signal.Broadcast();
    if (calls != std::vector<int>{ 1, 3 }) return Fail("Clear during dispatch called a disconnected listener.");

    std::cout << "UI pointer interaction, transient archive and delegate checks passed.\n";
    return 0;
}
