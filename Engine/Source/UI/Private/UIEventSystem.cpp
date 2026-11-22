#include "UIEventSystem.h"
#include "UIClipLayout.h"
#include "UIWidgetComponents.h"
#include "UITextInputComponent.h"
#include "UIDropdownComponent.h"
#include "UIAdvancedWidgetRuntime.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "WorldComponents.h"
#include <algorithm>

namespace
{
    bool IsInteractiveWidget(const entt::registry& world, entt::entity entity)
    {
        return Engine::TryGetEnabledComponent<Engine::UISliderComponent>(world, entity) ||
            Engine::TryGetEnabledComponent<Engine::UIToggleComponent>(world, entity) ||
            Engine::TryGetEnabledComponent<Engine::UITextInputComponent>(world, entity) ||
            Engine::TryGetEnabledComponent<Engine::UIDropdownComponent>(world, entity);
    }

    struct UIProcessingScope
    {
        bool& processing;
        explicit UIProcessingScope(bool& value) : processing(value) { processing = true; }
        ~UIProcessingScope() { processing = false; }
    };
}

Engine::UIEventSystem::UIEventSystem(Scene& scene, entt::entity canvas) : m_Scene(&scene)
{
    m_Scene->AddRef();
    const auto& world = scene.GetRegistry();
    const auto* id = world.valid(canvas) ? world.try_get<IDComponent>(canvas) : nullptr;
    m_Canvas = { canvas, id ? id->id : 0 };
}

Engine::UIEventSystem::~UIEventSystem()
{
    m_OnPointerEvent.Clear();
    Reset(nullptr);
    m_Scene->Release();
}

bool Engine::UIEventSystem::Exists(Target target) const
{
    const auto& world = m_Scene->GetRegistry();
    const auto* id = world.valid(target.entity) ? world.try_get<IDComponent>(target.entity) : nullptr;
    return target.id != 0 && id && id->id == target.id;
}

bool Engine::UIEventSystem::CanInteract(Target target, const std::vector<UIElementLayout>& layouts) const
{
    if (!Exists(target)) return false;
    const auto& world = m_Scene->GetRegistry();
    const auto* button = TryGetEnabledComponent<UIButtonComponent>(world, target.entity);
    const auto* image = TryGetEnabledComponent<UIImageComponent>(world, target.entity);
    const auto* slider = TryGetEnabledComponent<UISliderComponent>(world, target.entity);
    const auto* toggle = TryGetEnabledComponent<UIToggleComponent>(world, target.entity);
    const auto* textInput = TryGetEnabledComponent<UITextInputComponent>(world, target.entity);
    const auto* dropdown = TryGetEnabledComponent<UIDropdownComponent>(world, target.entity);
    const bool interactive = slider ? slider->interactable && UIWidgetRange::IsValid(*slider)
        : toggle ? toggle->interactable
        : textInput ? true
        : dropdown ? dropdown->interactable
        : button && button->interactable && UILayout::IsValid(*button);
    if (!interactive || !image || !UILayout::IsValid(*image)) return false;
    return std::ranges::any_of(layouts, [&](const auto& layout)
    {
        const float determinant = glm::determinant(layout.renderMatrix);
        return layout.entity == target.entity && layout.visible && std::isfinite(determinant) && determinant != 0.0f;
    });
}

Engine::UIEventSystem::Target Engine::UIEventSystem::HitTest(const glm::vec2& point,
    const std::vector<UIElementLayout>& layouts, bool& blocked, Target* scrollbar) const
{
    blocked = false;
    if (scrollbar) *scrollbar = {};
    const auto& world = m_Scene->GetRegistry();
    const auto* canvas = TryGetEnabledComponent<CanvasComponent>(world, m_Canvas.entity);
    if (!canvas || point.x < 0.0f || point.y < 0.0f || point.x >= canvas->referenceResolution.x ||
        point.y >= canvas->referenceResolution.y) return {};
    const auto overlays = UIClipLayout::GetScrollbarOverlays(world, layouts);
    auto overlay = overlays.rbegin();
    for (auto index = layouts.size(); index > 0; --index)
    {
        while (overlay != overlays.rend() && layouts[overlay->index].subtreeEnd == index)
        {
            const auto& viewport = layouts[overlay->index];
            const auto* id = world.try_get<IDComponent>(viewport.entity);
            if (id && id->id != 0 && UIClipLayout::IsScrollbarHit(viewport, overlay->geometry, point))
            {
                blocked = true;
                if (scrollbar) *scrollbar = { viewport.entity, id->id };
                return {};
            }
            ++overlay;
        }
        const auto& layout = layouts[index - 1];
        const auto* image = TryGetEnabledComponent<UIImageComponent>(world, layout.entity);
        if (!image || !image->raycastTarget || !UILayout::IsValid(*image) || !UILayout::Contains(layout, point)) continue;
        blocked = true; // Any raycast-target image blocks elements behind it.
        auto entity = layout.entity;
        for (std::size_t depth = 0; depth < 256 && entity != m_Canvas.entity; ++depth)
        {
            if (TryGetEnabledComponent<UIButtonComponent>(world, entity) || IsInteractiveWidget(world, entity))
            {
                const auto* id = world.try_get<IDComponent>(entity);
                const Target target{ entity, id ? id->id : 0 };
                return CanInteract(target, layouts) ? target : Target{};
            }
            const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
            if (!hierarchy || hierarchy->parent == entt::null) break;
            entity = hierarchy->parent;
        }
        return {};
    }
    return {};
}

bool Engine::UIEventSystem::UpdateScrollbar(const std::vector<UIElementLayout>& layouts, bool pressed)
{
    if (!Exists(m_Scrollbar)) return false;
    const auto viewport = std::ranges::find(layouts, m_Scrollbar.entity, &UIElementLayout::entity);
    if (viewport == layouts.end()) return false;
    auto& world = m_Scene->GetRegistry();
    const auto geometry = UIClipLayout::GetScrollbar(world, *viewport);
    if (!geometry) return false;
    const float pointerY = UIClipLayout::ToScrollbarLocal(*viewport, m_Position).y;
    if (!std::isfinite(pointerY)) return false;

    const float thumbOffset = pointerY - geometry->thumbTop;
    if (pressed)
    {
        const bool onThumb = thumbOffset >= 0.0f && thumbOffset <= geometry->thumbHeight;
        m_ScrollbarGrabFraction = onThumb ? thumbOffset / geometry->thumbHeight : 0.5f;
        if (onThumb) return true;
    }

    const float top = pointerY - m_ScrollbarGrabFraction * geometry->thumbHeight;
    const float fraction = std::clamp(top / (1.0f - geometry->thumbHeight), 0.0f, 1.0f);
    world.get<UIScrollComponent>(m_Scrollbar.entity).offset = fraction * geometry->scrollRange;
    return true;
}

void Engine::UIEventSystem::SetState(Target target, bool hovered, bool pressed)
{
    if (!Exists(target)) return;
    auto& world = m_Scene->GetRegistry();
    const bool focused = target == m_KeyboardFocus;
    if (!hovered && !pressed && !focused) world.remove<UIInteractionStateComponent>(target.entity);
    else world.emplace_or_replace<UIInteractionStateComponent>(target.entity, hovered, pressed, focused);
}

void Engine::UIEventSystem::SetKeyboardFocus(Target target)
{
    if (target == m_KeyboardFocus) return;
    const auto previous = m_KeyboardFocus;
    m_KeyboardFocus = target;
    SetState(previous, previous == m_Hovered, previous == m_Pressed);
    SetState(target, target == m_Hovered, target == m_Pressed);
}

bool Engine::UIEventSystem::Navigate(int direction, bool submit)
{
    if (m_Processing || !m_Scene->IsActive() || m_Scene->IsPaused() || !Exists(m_Canvas)) return false;
    UIProcessingScope scope(m_Processing);
    const auto& world = m_Scene->GetRegistry();
    std::vector<UIElementLayout> layouts;
    if (!UILayout::Build(world, m_Canvas.entity, layouts)) return false;

    std::vector<Target> buttons;
    for (const auto& layout : layouts)
    {
        const auto* id = world.try_get<IDComponent>(layout.entity);
        if (!id) continue;
        const Target candidate{ layout.entity, id->id };
        if (CanInteract(candidate, layouts)) buttons.push_back(candidate);
    }
    if (buttons.empty()) return false;

    const auto focused = std::ranges::find(buttons, m_KeyboardFocus);
    if (direction != 0)
    {
        if (focused == buttons.end()) SetKeyboardFocus(direction > 0 ? buttons.front() : buttons.back());
        else if (direction > 0) SetKeyboardFocus(focused + 1 == buttons.end() ? buttons.front() : *(focused + 1));
        else SetKeyboardFocus(focused == buttons.begin() ? buttons.back() : *(focused - 1));
    }
    if (submit && m_KeyboardFocus.entity != entt::null)
    {
        const auto layout = std::ranges::find(layouts, m_KeyboardFocus.entity, &UIElementLayout::entity);
        if (layout != layouts.end())
        {
            const glm::vec2 center(layout->renderMatrix * glm::vec4(0.5f, 0.5f, 0.0f, 1.0f));
            Dispatch({ { EUIPointerEventType::Click, m_KeyboardFocus.entity, m_KeyboardFocus.id,
                m_Canvas.entity, m_Canvas.id, center, glm::vec2(0.0f), true } });
        }
    }
    return true;
}

bool Engine::UIEventSystem::AdjustFocusedSlider(int direction)
{
    if (direction == 0 || m_Processing || !m_Scene->IsActive() || m_Scene->IsPaused() || !Exists(m_Canvas))
        return false;
    const auto* slider = TryGetEnabledComponent<UISliderComponent>(m_Scene->GetRegistry(), m_KeyboardFocus.entity);
    if (!slider || !slider->interactable || !UIWidgetRange::IsValid(*slider)) return false;

    const double extent = static_cast<double>(slider->maximum) - slider->minimum;
    const double increment = slider->step > 0.0f ? slider->step : extent * 0.05;
    const double current = std::clamp(static_cast<double>(slider->value),
        static_cast<double>(slider->minimum), static_cast<double>(slider->maximum));
    double requested = current + (direction > 0 ? increment : -increment);
    // The upper bound stays reachable even when it does not lie on the step grid.
    if (direction < 0 && slider->step > 0.0f && current == slider->maximum)
    {
        double index = std::ceil(extent / increment) - 1.0;
        requested = slider->minimum + index * increment;
        if (static_cast<float>(requested) >= slider->maximum)
            requested = slider->minimum + (index - 1.0) * increment;
    }

    UIPointerEvent changed{ EUIPointerEventType::ValueChanged, m_KeyboardFocus.entity, m_KeyboardFocus.id,
        m_Canvas.entity, m_Canvas.id, m_Position, glm::vec2(0.0f), true };
    changed.value = UIWidgetRange::ClampSliderValue(*slider, requested);
    UIProcessingScope scope(m_Processing);
    Dispatch({ changed });
    return true;
}

void Engine::UIEventSystem::Queue(std::vector<UIPointerEvent>& events, EUIPointerEventType type, Target target) const
{
    if (target.entity != entt::null)
        events.push_back({ type, target.entity, target.id, m_Canvas.entity, m_Canvas.id, m_Position, m_Delta });
}

void Engine::UIEventSystem::QueueSliderValue(std::vector<UIPointerEvent>& events,
    const std::vector<UIElementLayout>& layouts) const
{
    const auto& world = m_Scene->GetRegistry();
    const auto* slider = TryGetEnabledComponent<UISliderComponent>(world, m_Pressed.entity);
    if (!slider) return;
    const auto layout = std::ranges::find(layouts, m_Pressed.entity, &UIElementLayout::entity);
    if (layout == layouts.end()) return;
    const glm::vec4 position = glm::inverse(layout->renderMatrix) * glm::vec4(m_Position, 0.0f, 1.0f);
    if (!std::isfinite(position.x)) return;
    const double extent = static_cast<double>(slider->maximum) - slider->minimum;
    const double requested = slider->minimum + extent * std::clamp(position.x, 0.0f, 1.0f);
    Queue(events, EUIPointerEventType::ValueChanged, m_Pressed);
    events.back().value = UIWidgetRange::ClampSliderValue(*slider, requested);
}

bool Engine::UIEventSystem::ApplyWidgetValue(UIPointerEvent& event)
{
    auto& world = m_Scene->GetRegistry();
    if (auto* slider = TryGetEnabledComponent<UISliderComponent>(world, event.target))
    {
        if (!slider->interactable || !UIWidgetRange::IsValid(*slider)) return false;
        event.previousValue = slider->value;
        event.value = std::clamp(event.value, slider->minimum, slider->maximum);
        if (event.value == slider->value) return false;
        slider->value = event.value;
        return true;
    }
    if (auto* toggle = TryGetEnabledComponent<UIToggleComponent>(world, event.target))
    {
        if (!toggle->interactable) return false;
        event.previousValue = toggle->checked ? 1.0f : 0.0f;
        if (event.value == event.previousValue) return false;
        toggle->checked = event.value != 0.0f;
        return true;
    }
    return false;
}

void Engine::UIEventSystem::Reset(std::vector<UIPointerEvent>* events)
{
    if (events)
    {
        Queue(*events, EUIPointerEventType::Cancel, m_Pressed);
        Queue(*events, EUIPointerEventType::Exit, m_Hovered);
    }
    SetKeyboardFocus({});
    SetState(m_Pressed, false, false);
    SetState(m_Hovered, false, false);
    m_Pressed = m_Hovered = m_Scrollbar = {};
    m_PointerOverUI = false;
    m_HasPosition = false;
    m_SuppressUntilRelease = true;
}

bool Engine::UIEventSystem::Cancel()
{
    if (m_Processing) return false;
    UIProcessingScope scope(m_Processing);
    std::vector<UIPointerEvent> events;
    Reset(&events);
    Dispatch(events);
    return true;
}

bool Engine::UIEventSystem::Update(const UIPointerInput& input)
{
    if (m_Processing) return false;
    UIProcessingScope scope(m_Processing);
    std::vector<UIPointerEvent> events;
    std::vector<UIElementLayout> layouts;
    const bool finite = std::isfinite(input.position.x) && std::isfinite(input.position.y);
    if (!input.focused || !finite || !m_Scene->IsActive() || !Exists(m_Canvas) ||
        !UILayout::Build(m_Scene->GetRegistry(), m_Canvas.entity, layouts))
    {
        Reset(&events);
        Dispatch(events);
        return false;
    }
    m_Delta = m_HasPosition ? input.position - m_Position : glm::vec2(0.0f);
    if (!std::isfinite(m_Delta.x) || !std::isfinite(m_Delta.y)) m_Delta = glm::vec2(0.0f);
    m_Position = input.position;
    m_HasPosition = true;
    if (input.down || (m_KeyboardFocus.entity != entt::null && !CanInteract(m_KeyboardFocus, layouts)))
        SetKeyboardFocus({});
    if (m_Pressed.entity != entt::null && !CanInteract(m_Pressed, layouts))
    {
        Queue(events, EUIPointerEventType::Cancel, m_Pressed);
        SetState(m_Pressed, false, false);
        m_Pressed = {};
        m_SuppressUntilRelease = true;
    }
    bool blocked = false;
    Target scrollbar;
    const bool draggingScrollbar = m_Scrollbar.entity != entt::null;
    const Target target = input.insideViewport && !draggingScrollbar
        ? HitTest(m_Position, layouts, blocked, &scrollbar) : Target{};
    m_PointerOverUI = blocked || draggingScrollbar;
    if (draggingScrollbar)
    {
        const bool updated = (input.held || input.up) && UpdateScrollbar(layouts, false);
        if (!updated || input.up)
        {
            m_Scrollbar = {};
            m_SuppressUntilRelease = input.held || input.down;
        }
        return true;
    }
    if (target != m_Hovered)
    {
        Queue(events, EUIPointerEventType::Exit, m_Hovered);
        SetState(m_Hovered, false, m_Hovered == m_Pressed && m_Pressed.entity != entt::null);
        m_Hovered = target;
        SetState(m_Hovered, true, m_Hovered == m_Pressed && m_Pressed.entity != entt::null);
        Queue(events, EUIPointerEventType::Enter, m_Hovered);
    }
    if (m_SuppressUntilRelease)
    {
        if (!input.held && !input.down) m_SuppressUntilRelease = false;
    }
    else if (input.down && m_Pressed.entity == entt::null)
    {
        if (scrollbar.entity != entt::null)
        {
            m_Scrollbar = scrollbar;
            if (!UpdateScrollbar(layouts, true) || input.up || !input.held) m_Scrollbar = {};
            Dispatch(events);
            return true;
        }
        m_Pressed = target;
        if (IsInteractiveWidget(m_Scene->GetRegistry(), target.entity)) SetKeyboardFocus(target);
        SetState(m_Pressed, true, true);
        Queue(events, EUIPointerEventType::Down, m_Pressed);
    }
    if (m_Pressed.entity != entt::null && (input.down || input.held || input.up))
        QueueSliderValue(events, layouts);
    if (m_Pressed.entity != entt::null && input.up)
    {
        const auto pressed = m_Pressed;
        m_Pressed = {};
        SetState(pressed, pressed == m_Hovered, false);
        Queue(events, EUIPointerEventType::Up, pressed);
        if (pressed == target) Queue(events, EUIPointerEventType::Click, pressed);
    }
    else if (m_Pressed.entity != entt::null && !input.held)
    {
        // Lost release edge: release capture without inventing a click.
        Queue(events, EUIPointerEventType::Cancel, m_Pressed);
        SetState(m_Pressed, m_Pressed == m_Hovered, false);
        m_Pressed = {};
    }
    // An editor restore may remove transient storage; rebuild it from this input owner's current state.
    if (m_KeyboardFocus.entity != entt::null)
        SetState(m_KeyboardFocus, m_KeyboardFocus == m_Hovered, m_KeyboardFocus == m_Pressed);
    if (m_Pressed.entity != entt::null) SetState(m_Pressed, m_Pressed == m_Hovered, true);
    if (m_Hovered.entity != entt::null) SetState(m_Hovered, true, m_Hovered == m_Pressed);
    Dispatch(events);
    return true;
}

void Engine::UIEventSystem::Dispatch(const std::vector<UIPointerEvent>& events)
{
    for (auto event : events)
    {
        if (event.type != EUIPointerEventType::Cancel && event.type != EUIPointerEventType::Exit)
        {
            std::vector<UIElementLayout> layouts;
            if (!m_Scene->IsActive() || !Exists(m_Canvas) ||
                !UILayout::Build(m_Scene->GetRegistry(), m_Canvas.entity, layouts) ||
                !CanInteract({ event.target, event.targetId }, layouts)) continue;
            if (event.type == EUIPointerEventType::Click && !event.keyboard)
            {
                bool blocked = false;
                if (HitTest(event.position, layouts, blocked) != Target{ event.target, event.targetId }) continue;
            }
        }
        if (event.type == EUIPointerEventType::ValueChanged && !ApplyWidgetValue(event)) continue;
        // No registry references survive a callback.
        m_OnPointerEvent.Broadcast(event);
        if (event.type == EUIPointerEventType::Click && Exists({ event.target, event.targetId }))
        {
            auto& reg = m_Scene->GetRegistry();
            const auto* toggle = TryGetEnabledComponent<UIToggleComponent>(reg, event.target);
            if (toggle)
            {
                event.type = EUIPointerEventType::ValueChanged;
                event.value = toggle->checked ? 0.0f : 1.0f;
                Dispatch({ event });
            }
            if (TryGetEnabledComponent<UITextInputComponent>(reg, event.target))
            {
                SetTextInputFocus(reg, event.target);
            }
            else
            {
                SetTextInputFocus(reg, entt::null);
            }
            if (auto* dropdown = TryGetEnabledComponent<UIDropdownComponent>(reg, event.target))
            {
                dropdown->ToggleOpen();
            }
            else
            {
                CloseAllDropdowns(reg);
            }
        }
    }
}

Engine::Entity* Engine::UIEventSystem::ResolveTarget(const UIPointerEvent& event) const
{
    if (event.canvas != m_Canvas.entity || event.canvasId != m_Canvas.id ||
        !Exists({ event.target, event.targetId })) return nullptr;
    return m_Scene->FindEntity(event.target);
}

glm::vec4 Engine::UIEventSystem::GetButtonTint(const entt::registry& world, entt::entity entity)
{
    if (!world.valid(entity)) return glm::vec4(1.0f);
    const auto* button = TryGetEnabledComponent<UIButtonComponent>(world, entity);
    const auto* slider = TryGetEnabledComponent<UISliderComponent>(world, entity);
    const auto* toggle = TryGetEnabledComponent<UIToggleComponent>(world, entity);
    const auto* textInput = TryGetEnabledComponent<UITextInputComponent>(world, entity);
    const auto* dropdown = TryGetEnabledComponent<UIDropdownComponent>(world, entity);
    const auto* state = world.try_get<UIInteractionStateComponent>(entity);
    if (slider || toggle || textInput || dropdown)
    {
        const bool enabled = slider ? slider->interactable && UIWidgetRange::IsValid(*slider)
            : toggle ? toggle->interactable
            : dropdown ? dropdown->interactable
            : true;
        if (!enabled) return { 0.5f, 0.5f, 0.5f, 0.5f };
        if (state && state->pressed) return { 0.7f, 0.7f, 0.7f, 1.0f };
        if (state && (state->hovered || state->keyboardFocused)) return { 0.9f, 0.9f, 0.9f, 1.0f };
        return glm::vec4(1.0f);
    }
    if (!button || !UILayout::IsValid(*button)) return glm::vec4(1.0f);
    if (!button->interactable) return button->disabledColor;
    if (state && state->hovered) return state->pressed ? button->pressedColor : button->hoveredColor;
    if (state && state->keyboardFocused) return button->hoveredColor;
    return button->normalColor;
}
