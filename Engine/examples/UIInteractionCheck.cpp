#include "UIEventSystem.h"
#include "UIComponents.h"
#include "UILayout.h"
#include "UIEllipseMaskComponent.h"
#include "UIWidgetComponents.h"
#include "CanvasScalerComponent.h"
#include "UITextInputComponent.h"
#include "UIDropdownComponent.h"
#include "UIMultilineTextLayout.h"
#include "UIAdvancedWidgetRuntime.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "ReflectionRegistry.h"
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

    {
        Fixture nested;
        auto& registry = nested.scene->GetRegistry();
        const auto child = nested.Add(nested.button, true);
        const auto* buttonEntry = ReflectionRegistry::Get().FindComponent(typeid(UIButtonComponent));
        if (!buttonEntry || !buttonEntry->SetEnabled(registry, child, false))
            return Fail("Could not disable the nested button component.");
        std::vector<UIPointerEvent> clicks;
        UIEventSystem nestedSystem(*nested.scene, nested.canvas);
        nestedSystem.OnPointerEvent().AddLambda([&](const auto& event)
        {
            if (event.type == Type::Click) clicks.push_back(event);
        });
        const auto click = [&]
        {
            clicks.clear();
            Feed(nestedSystem, inside);
            Feed(nestedSystem, inside, true, true);
            Feed(nestedSystem, inside, false, false, true);
        };
        click();
        if (clicks.size() != 1 || clicks.front().target != nested.button)
            return Fail("Disabled nested button prevented its enabled ancestor from receiving the click.");
        if (!buttonEntry->SetEnabled(registry, child, true)) return Fail("Could not re-enable the nested button.");
        click();
        if (clicks.size() != 1 || clicks.front().target != child)
            return Fail("Re-enabled nested button did not regain input.");
    }

    {
        Fixture menu;
        const auto secondButton = menu.Add(menu.canvas, true);
        auto& world = menu.scene->GetRegistry();
        UIEventSystem system(*menu.scene, menu.canvas);
        std::vector<UIPointerEvent> clicks;
        system.OnPointerEvent().AddLambda([&](const auto& event)
        {
            if (event.type == Type::Click) clicks.push_back(event);
        });
        const auto focused = [&](entt::entity button)
        {
            const auto* state = world.try_get<UIInteractionStateComponent>(button);
            return state && state->keyboardFocused;
        };
        if (!system.Navigate(1, false) || !focused(menu.button))
            return Fail("Keyboard navigation did not select the first button.");
        if (!system.Navigate(-1, true) || !focused(secondButton) || focused(menu.button))
            return Fail("Backward keyboard navigation did not wrap to the last button.");
        if (clicks.size() != 1 || !clicks.front().keyboard || clicks.front().target != secondButton)
            return Fail("Keyboard submit did not activate the selected button exactly once.");
        world.get<UIButtonComponent>(secondButton).interactable = false;
        Feed(system, inside);
        if (!system.Navigate(1, false) || !focused(menu.button) || focused(secondButton))
            return Fail("Keyboard navigation selected a disabled button.");
        system.Cancel();
        system.Navigate(0, true);
        if (focused(menu.button) || clicks.size() != 1)
            return Fail("Keyboard focus survived cancellation.");
    }

    {
        Fixture controls;
        auto& world = controls.scene->GetRegistry();
        world.remove<UIButtonComponent>(controls.button);
        auto& slider = world.emplace<UISliderComponent>(controls.button);
        slider.step = 0.25f;
        UIEventSystem system(*controls.scene, controls.canvas);
        std::vector<UIPointerEvent> changes;
        system.OnPointerEvent().AddLambda([&](const auto& event)
        {
            if (event.type == Type::ValueChanged) changes.push_back(event);
        });

        const glm::vec2 quarter{ 35.0f, 30.0f };
        Feed(system, quarter);
        Feed(system, quarter, true, true);
        Feed(system, quarter, false, true);
        if (!system.HasPointerCapture() || changes.size() != 1 || changes.back().previousValue != 0.5f ||
            changes.back().value != 0.25f || slider.value != 0.25f)
            return Fail("Slider press did not snap once to the selected interval.");
        Feed(system, outside, false, true, false, false);
        Feed(system, outside, false, false, true, false);
        if (system.HasPointerCapture() || changes.size() != 2 || slider.value != 1.0f)
            return Fail("Captured slider did not reach its endpoint or duplicated its release event.");
        if (!system.AdjustFocusedSlider(-1) || changes.size() != 3 || !changes.back().keyboard || slider.value != 0.75f)
            return Fail("A focused slider did not adjust with the keyboard.");
        slider.interactable = false;
        Feed(system, quarter);
        if (system.AdjustFocusedSlider(1) || changes.size() != 3)
            return Fail("A disabled slider accepted keyboard input.");
    }

    {
        Fixture controls;
        auto& world = controls.scene->GetRegistry();
        world.remove<UIButtonComponent>(controls.button);
        auto& toggle = world.emplace<UIToggleComponent>(controls.button);
        UIEventSystem system(*controls.scene, controls.canvas);
        std::vector<UIPointerEvent> changes;
        system.OnPointerEvent().AddLambda([&](const auto& event)
        {
            if (event.type == Type::ValueChanged) changes.push_back(event);
        });

        Feed(system, inside);
        Feed(system, inside, true, true);
        Feed(system, outside, false, false, true);
        if (toggle.checked || !changes.empty()) return Fail("A cancelled toggle click changed its value.");
        Feed(system, inside, true, true);
        Feed(system, inside, false, false, true);
        if (!toggle.checked || changes.size() != 1 || changes.back().previousValue != 0.0f || changes.back().value != 1.0f)
            return Fail("Toggle click did not deliver one value change.");
        if (!system.Navigate(0, true) || toggle.checked || changes.size() != 2 || !changes.back().keyboard)
            return Fail("Keyboard activation did not toggle the selected control.");
    }

    {
        Fixture masked;
        auto& world = masked.scene->GetRegistry();
        world.get<UIImageComponent>(masked.button).raycastTarget = false;
        const auto mask = masked.Add(masked.canvas, false);
        world.get<UIImageComponent>(mask).raycastTarget = false;
        world.emplace<UIEllipseMaskComponent>(mask);
        const auto child = masked.Add(mask, true);
        world.get<RectTransformComponent>(child).anchoredPosition = { 0.0f, 0.0f };
        UIEventSystem system(*masked.scene, masked.canvas);
        std::vector<UIPointerEvent> hits;
        system.OnPointerEvent().AddLambda([&](const auto& event) { hits.push_back(event); });

        Feed(system, { 11.0f, 11.0f });
        if (system.IsPointerOverUI() || !hits.empty()) return Fail("Ellipse mask corner accepted pointer input.");
        Feed(system, { 60.0f, 40.0f });
        if (!system.IsPointerOverUI() || !Types(hits, { Type::Enter }) || hits.front().target != child)
            return Fail("Ellipse mask center rejected pointer input.");
    }

    {
        Fixture scrolling;
        auto& world = scrolling.scene->GetRegistry();
        const auto viewport = scrolling.Add(scrolling.canvas, false);
        const auto content = scrolling.Add(viewport, true);
        world.get<UIImageComponent>(viewport).raycastTarget = false;
        auto& rect = world.get<RectTransformComponent>(content);
        rect.anchoredPosition = glm::vec2(0.0f);
        rect.sizeDelta.y = 180.0f;
        auto& scroll = world.emplace<UIScrollComponent>(viewport);
        UIEventSystem system(*scrolling.scene, scrolling.canvas);
        std::vector<UIPointerEvent> events;
        system.OnPointerEvent().AddLambda([&](const auto& event) { events.push_back(event); });

        Feed(system, { 105.0f, 20.0f });
        Feed(system, { 105.0f, 20.0f }, true, true);
        if (!system.HasPointerCapture() || scroll.offset != 0.0f || !events.empty())
            return Fail("Scrollbar thumb did not capture without moving or activating content.");
        Feed(system, { 160.0f, 250.0f }, false, true, false, false);
        if (!system.HasPointerCapture() || scroll.offset != 120.0f)
            return Fail("Scrollbar drag outside the viewport did not reach its end.");
        Feed(system, { 160.0f, 250.0f }, false, false, true, false);
        if (system.HasPointerCapture() || !events.empty())
            return Fail("Scrollbar did not release its capture cleanly.");
        Feed(system, { 105.0f, 40.0f }, true, true);
        if (!system.HasPointerCapture() || scroll.offset <= 0.0f || scroll.offset >= 120.0f)
            return Fail("Scrollbar track click did not move to an intermediate position.");
        scroll.showScrollbar = false;
        Feed(system, { 105.0f, 40.0f }, false, true);
        if (system.HasPointerCapture()) return Fail("Hiding the scrollbar did not cancel capture.");
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

    {
        CanvasScalerComponent scaler;
        scaler.scaleMode = ECanvasScaleMode::ConstantPixelSize;
        scaler.scaleFactor = 1.5f;
        if (std::abs(scaler.ComputeScaleFactor({ 1920.0f, 1080.0f }) - 1.5f) > 1e-4f)
            return Fail("CanvasScaler ConstantPixelSize scale factor failed.");
        const glm::vec2 eff1 = scaler.ComputeEffectiveDesignSize({ 1500.0f, 900.0f });
        if (std::abs(eff1.x - 1000.0f) > 1e-3f || std::abs(eff1.y - 600.0f) > 1e-3f)
            return Fail("CanvasScaler ConstantPixelSize effective design size failed.");

        scaler.scaleMode = ECanvasScaleMode::ScaleWithScreenSize;
        scaler.referenceResolution = { 1920.0f, 1080.0f };
        scaler.matchWidthOrHeight = 0.0f; // Width match
        if (std::abs(scaler.ComputeScaleFactor({ 3840.0f, 1080.0f }) - 2.0f) > 1e-4f)
            return Fail("CanvasScaler ScaleWithScreenSize width match failed.");

        scaler.matchWidthOrHeight = 1.0f; // Height match
        if (std::abs(scaler.ComputeScaleFactor({ 1920.0f, 2160.0f }) - 2.0f) > 1e-4f)
            return Fail("CanvasScaler ScaleWithScreenSize height match failed.");

        scaler.matchWidthOrHeight = 0.5f; // 0.5 log-weighted match: sqrt(2 * 0.5) = 1.0
        scaler.referenceResolution = { 1000.0f, 1000.0f };
        if (std::abs(scaler.ComputeScaleFactor({ 2000.0f, 500.0f }) - 1.0f) > 1e-4f)
            return Fail("CanvasScaler ScaleWithScreenSize 0.5 log-weighted match failed.");

        // Zero / negative resolution safety
        scaler.referenceResolution = { 0.0f, 1000.0f };
        if (scaler.ComputeScaleFactor({ 1920.0f, 1080.0f }) != 1.0f)
            return Fail("CanvasScaler invalid reference resolution safety failed.");

        // UpdateCanvasScaler helper
        Fixture canvasFixture;
        auto& reg = canvasFixture.scene->GetRegistry();
        auto& cs = reg.emplace<CanvasScalerComponent>(canvasFixture.canvas);
        cs.scaleMode = ECanvasScaleMode::ConstantPixelSize;
        cs.scaleFactor = 2.0f;
        if (!UpdateCanvasScaler(reg, canvasFixture.canvas, { 800.0f, 600.0f }))
            return Fail("UpdateCanvasScaler failed.");
        const auto& canvasComp = reg.get<CanvasComponent>(canvasFixture.canvas);
        if (std::abs(canvasComp.referenceResolution.x - 400.0f) > 1e-3f ||
            std::abs(canvasComp.referenceResolution.y - 300.0f) > 1e-3f)
            return Fail("UpdateCanvasScaler did not apply effective design resolution.");
    }

    {
        UITextInputComponent input;
        input.placeholder = "Enter text...";
        input.isFocused = true;

        // Multibyte Korean text editing
        input.InsertUtf8("한글입력");
        if (input.text != "한글입력" || input.cursorByteIndex != 12)
            return Fail("UITextInput Korean UTF-8 insertion failed.");

        input.MoveCursorLeft();
        if (input.cursorByteIndex != 9) return Fail("UITextInput Korean cursor move left 1 failed.");
        input.MoveCursorLeft();
        if (input.cursorByteIndex != 6) return Fail("UITextInput Korean cursor move left 2 failed.");

        // Backspace deletes '글' (bytes 3..6)
        if (!input.Backspace() || input.text != "한입력" || input.cursorByteIndex != 3)
            return Fail("UITextInput Korean multibyte backspace failed.");

        // DeleteForward deletes '입' (bytes 3..6)
        if (!input.DeleteForward() || input.text != "한력" || input.cursorByteIndex != 3)
            return Fail("UITextInput Korean multibyte delete forward failed.");

        // Range selection and deletion
        input.text = "Hello World";
        input.cursorByteIndex = 5;
        input.selectionAnchorByteIndex = 0;
        if (!input.HasSelection() || input.GetSelectedText() != "Hello")
            return Fail("UITextInput range selection failed.");
        if (!input.DeleteSelection() || input.text != " World" || input.cursorByteIndex != 0)
            return Fail("UITextInput delete selection failed.");

        input.SelectAll();
        if (input.GetSelectedText() != " World") return Fail("UITextInput SelectAll failed.");
        input.ClearSelection();
        if (input.HasSelection()) return Fail("UITextInput ClearSelection failed.");

        // IME composition preview & commit / cancel
        input.text = "안녕";
        input.cursorByteIndex = input.text.size();
        input.selectionAnchorByteIndex = input.cursorByteIndex;
        input.SetImeComposition("하");
        if (input.text != "안녕" || input.GetDisplayText() != "안녕하")
            return Fail("UITextInput IME composition preview failed.");
        input.CommitImeComposition();
        if (input.text != "안녕하" || !input.imeComposition.empty())
            return Fail("UITextInput IME commit failed.");
        input.SetImeComposition("세");
        input.CancelImeComposition();
        if (input.text != "안녕하" || !input.imeComposition.empty())
            return Fail("UITextInput IME cancel failed.");

        // Password masking
        input.isPassword = true;
        if (input.GetDisplayText() != "***")
            return Fail("UITextInput password masking failed.");
        input.SetImeComposition("요");
        if (input.GetDisplayText() != "****")
            return Fail("UITextInput password masking with IME failed.");
        input.CancelImeComposition();
        input.isPassword = false;

        // MaxLength clamping
        input.text.clear();
        input.cursorByteIndex = 0;
        input.selectionAnchorByteIndex = 0;
        input.maxLength = 5;
        input.InsertUtf8("1234");
        if (!input.InsertUtf8("567") || input.text != "12345")
            return Fail("UITextInput maxLength truncate insert failed.");
        if (input.InsertUtf8("8") || input.text != "12345")
            return Fail("UITextInput maxLength boundary rejection failed.");

        // Keyboard routing via UIAdvancedWidgetRuntime
        input.text = "abc";
        input.cursorByteIndex = 3;
        input.selectionAnchorByteIndex = 3;
        if (!HandleTextInputKey(input, EUIKey::Backspace) || input.text != "ab")
            return Fail("HandleTextInputKey backspace failed.");
        std::string clipboard;
        input.SelectAll();
        HandleTextInputKey(input, EUIKey::A, { .ctrl = true });
        HandleTextInputKey(input, EUIKey::C, { .ctrl = true }, &clipboard);
        if (clipboard != "ab") return Fail("HandleTextInputKey copy failed.");
        input.cursorByteIndex = 2;
        input.selectionAnchorByteIndex = 2;
        HandleTextInputKey(input, EUIKey::V, { .ctrl = true }, nullptr, "cd");
        if (input.text != "abcd") return Fail("HandleTextInputKey paste failed.");
    }

    {
        UIDropdownComponent dropdown;
        dropdown.placeholder = "Choose...";

        // Empty options safety
        dropdown.ToggleOpen();
        if (dropdown.isOpen) return Fail("UIDropdown empty options should not open.");
        if (dropdown.GetSelectedLabel() != "Choose...") return Fail("UIDropdown empty placeholder failed.");
        if (dropdown.GetPopupHeight() != 0.0f) return Fail("UIDropdown empty popup height failed.");
        if (dropdown.HitTestOptionIndex(10.0f) != -1) return Fail("UIDropdown empty hit test failed.");
        if (dropdown.SelectIndex(0)) return Fail("UIDropdown empty SelectIndex should fail.");

        // Populated options
        dropdown.options = { "Option A", "Option B", "Option C", "Option D" };
        dropdown.itemHeight = 25.0f;
        dropdown.maxVisibleItems = 3;

        dropdown.ToggleOpen();
        if (!dropdown.isOpen) return Fail("UIDropdown ToggleOpen failed.");
        if (dropdown.GetPopupHeight() != 75.0f)
            return Fail("UIDropdown popup height calculation failed.");

        if (dropdown.HitTestOptionIndex(10.0f) != 0) return Fail("UIDropdown HitTest index 0 failed.");
        if (dropdown.HitTestOptionIndex(30.0f) != 1) return Fail("UIDropdown HitTest index 1 failed.");
        if (dropdown.HitTestOptionIndex(60.0f) != 2) return Fail("UIDropdown HitTest index 2 failed.");
        if (dropdown.HitTestOptionIndex(80.0f) != -1) return Fail("UIDropdown HitTest outside height failed.");

        if (!dropdown.SelectIndex(1) || dropdown.selectedIndex != 1 || dropdown.isOpen)
            return Fail("UIDropdown SelectIndex failed.");
        if (dropdown.GetSelectedLabel() != "Option B")
            return Fail("UIDropdown GetSelectedLabel failed.");

        // HandleDropdownClick runtime helper
        const glm::vec2 headerPos{ 10.0f, 10.0f };
        const glm::vec2 headerSize{ 120.0f, 30.0f };

        // Click on header opens dropdown
        if (!HandleDropdownClick(dropdown, { 50.0f, 20.0f }, headerPos, headerSize) || !dropdown.isOpen)
            return Fail("HandleDropdownClick header click open failed.");

        // Click on popup selects option (popup starts at y = 40.0f; local y = 55.0f -> item 2)
        if (!HandleDropdownClick(dropdown, { 50.0f, 95.0f }, headerPos, headerSize) ||
            dropdown.selectedIndex != 2 || dropdown.isOpen)
            return Fail("HandleDropdownClick popup option selection failed.");

        // Click outside closes open dropdown
        dropdown.Open();
        if (HandleDropdownClick(dropdown, { 300.0f, 300.0f }, headerPos, headerSize) || dropdown.isOpen)
            return Fail("HandleDropdownClick outside click close failed.");
    }

    {
        const auto measureChar10 = [](std::string_view s)
        {
            return static_cast<float>(s.size()) * 10.0f;
        };

        // Explicit newlines
        const auto nlResult = WrapMultilineText("Alpha\nBeta\nGamma", 200.0f, measureChar10, 20.0f);
        if (nlResult.lines.size() != 3 || nlResult.lines[0].text != "Alpha" ||
            nlResult.lines[1].text != "Beta" || nlResult.lines[2].text != "Gamma")
            return Fail("WrapMultilineText explicit newlines failed.");
        if (nlResult.totalHeight != 60.0f)
            return Fail("WrapMultilineText explicit newlines total height failed.");

        // Word wrap with space separation
        const auto wordResult = WrapMultilineText("One Two Three", 60.0f, measureChar10, 20.0f);
        if (wordResult.lines.size() != 3 || wordResult.lines[0].text != "One" ||
            wordResult.lines[1].text != "Two" || wordResult.lines[2].text != "Three")
            return Fail("WrapMultilineText word wrap failed.");

        // Multibyte CJK wrapping
        const auto measureCjk = [](std::string_view s)
        {
            return static_cast<float>(UIUtf8::CountCodepoints(s)) * 20.0f;
        };
        const auto cjkResult = WrapMultilineText("가나다라마", 60.0f, measureCjk, 25.0f);
        if (cjkResult.lines.size() != 2 || cjkResult.lines[0].text != "가나다" || cjkResult.lines[1].text != "라마")
            return Fail("WrapMultilineText CJK multibyte character wrap failed.");

        // Alignment offsets
        const auto alignResult = WrapMultilineText("가나다\n라마", 100.0f, 25.0f, 100.0f,
            EUITextAlignment::Right, EUIVerticalAlignment::Bottom, measureCjk);
        if (std::abs(alignResult.lines[0].offset.x - 40.0f) > 1e-3f ||
            std::abs(alignResult.lines[0].offset.y - 50.0f) > 1e-3f ||
            std::abs(alignResult.lines[1].offset.x - 60.0f) > 1e-3f ||
            std::abs(alignResult.lines[1].offset.y - 75.0f) > 1e-3f)
            return Fail("WrapMultilineText Right / Bottom alignment offsets failed.");

        // Center / Middle alignment
        const auto centerResult = WrapMultilineText("가나다\n라마", 100.0f, 25.0f, 100.0f,
            EUITextAlignment::Center, EUIVerticalAlignment::Middle, measureCjk);
        if (std::abs(centerResult.lines[0].offset.x - 20.0f) > 1e-3f ||
            std::abs(centerResult.lines[0].offset.y - 25.0f) > 1e-3f ||
            std::abs(centerResult.lines[1].offset.x - 30.0f) > 1e-3f ||
            std::abs(centerResult.lines[1].offset.y - 50.0f) > 1e-3f)
            return Fail("WrapMultilineText Center / Middle alignment offsets failed.");
    }

    {
        // 1. UITextComponent multiline and word wrap validation
        UITextComponent textComp;
        textComp.text = "Line1\nLine2";
        if (UILayout::IsValid(textComp))
            return Fail("Single-line UITextComponent unexpectedly accepted newlines.");

        textComp.multiline = true;
        if (!UILayout::IsValid(textComp))
            return Fail("Multiline UITextComponent rejected newlines.");

        textComp.multiline = false;
        textComp.wordWrap = true;
        if (!UILayout::IsValid(textComp))
            return Fail("Word-wrapped UITextComponent rejected newlines.");

        // 2. CanvasScaler effective size integer rounding and layout validity
        Fixture scalerFixture;
        auto& reg = scalerFixture.scene->GetRegistry();
        auto& scaler = reg.emplace<CanvasScalerComponent>(scalerFixture.canvas);
        scaler.scaleMode = ECanvasScaleMode::ScaleWithScreenSize;
        scaler.referenceResolution = { 1920.0f, 1080.0f };
        scaler.matchWidthOrHeight = 0.5f;

        if (!UpdateCanvasScaler(reg, scalerFixture.canvas, { 1366.0f, 768.0f }))
            return Fail("UpdateCanvasScaler failed for arbitrary screen size.");
        const auto& canvasComp = reg.get<CanvasComponent>(scalerFixture.canvas);
        if (!UILayout::IsValid(canvasComp))
            return Fail("CanvasComponent referenceResolution after scaler was not valid integer resolution.");
        std::vector<UIElementLayout> layouts;
        if (!UILayout::Build(reg, scalerFixture.canvas, layouts))
            return Fail("UILayout::Build failed on Canvas scaled by CanvasScalerComponent.");

        // 3. SetTextInputFocus multi-entity test
        Fixture inputFixture;
        auto& inputReg = inputFixture.scene->GetRegistry();
        const auto entityA = inputFixture.scene->CreateEntity().GetEntityHandle();
        const auto entityB = inputFixture.scene->CreateEntity().GetEntityHandle();
        auto& inputA = inputReg.emplace<UITextInputComponent>(entityA);
        auto& inputB = inputReg.emplace<UITextInputComponent>(entityB);

        SetTextInputFocus(inputReg, entityA);
        if (!inputA.isFocused || inputB.isFocused)
            return Fail("SetTextInputFocus entityA failed.");

        SetTextInputFocus(inputReg, entityB);
        if (inputA.isFocused || !inputB.isFocused)
            return Fail("SetTextInputFocus switch to entityB failed.");

        SetTextInputFocus(inputReg, entt::null);
        if (inputA.isFocused || inputB.isFocused)
            return Fail("SetTextInputFocus clear focus failed.");

        // 4. CloseAllDropdowns multi-entity test
        Fixture dropFixture;
        auto& dropReg = dropFixture.scene->GetRegistry();
        const auto dropEntityA = dropFixture.scene->CreateEntity().GetEntityHandle();
        const auto dropEntityB = dropFixture.scene->CreateEntity().GetEntityHandle();
        auto& dropA = dropReg.emplace<UIDropdownComponent>(dropEntityA);
        auto& dropB = dropReg.emplace<UIDropdownComponent>(dropEntityB);
        dropA.options = { "A1", "A2" };
        dropB.options = { "B1", "B2" };
        dropA.Open();
        dropB.Open();
        if (!dropA.isOpen || !dropB.isOpen)
            return Fail("UIDropdown Open failed.");

        CloseAllDropdowns(dropReg, dropEntityA);
        if (!dropA.isOpen || dropB.isOpen)
            return Fail("CloseAllDropdowns with exceptEntity failed.");

        CloseAllDropdowns(dropReg);
        if (dropA.isOpen || dropB.isOpen)
            return Fail("CloseAllDropdowns all failed.");
    }

    std::cout << "UI pointer interaction, transient archive, advanced widgets and delegate checks passed.\n";
    return 0;
}
