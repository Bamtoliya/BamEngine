#include "UIEventSystem.h"
#include "Scene.h"
#include "CoreComponents.h"
#include <cmath>

namespace
{
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
    const auto* button = world.try_get<UIButtonComponent>(target.entity);
    const auto* image = world.try_get<UIImageComponent>(target.entity);
    if (!button || !button->interactable || !UILayout::IsValid(*button) || !image || !UILayout::IsValid(*image))
        return false;
    return std::ranges::any_of(layouts, [&](const auto& layout)
    {
        const float determinant = glm::determinant(layout.renderMatrix);
        return layout.entity == target.entity && layout.visible && std::isfinite(determinant) && determinant != 0.0f;
    });
}

Engine::UIEventSystem::Target Engine::UIEventSystem::HitTest(const glm::vec2& point,
    const std::vector<UIElementLayout>& layouts, bool& blocked) const
{
    blocked = false;
    const auto& world = m_Scene->GetRegistry();
    const auto* canvas = world.try_get<CanvasComponent>(m_Canvas.entity);
    if (!canvas || point.x < 0.0f || point.y < 0.0f || point.x >= canvas->referenceResolution.x ||
        point.y >= canvas->referenceResolution.y) return {};
    for (auto it = layouts.rbegin(); it != layouts.rend(); ++it)
    {
        const auto* image = world.try_get<UIImageComponent>(it->entity);
        if (!image || !image->raycastTarget || !UILayout::IsValid(*image) || !UILayout::Contains(*it, point)) continue;
        blocked = true; // Even a disabled button or a plain image blocks elements behind it.
        auto entity = it->entity;
        for (std::size_t depth = 0; depth < 256 && entity != m_Canvas.entity; ++depth)
        {
            if (world.all_of<UIButtonComponent>(entity))
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

void Engine::UIEventSystem::SetState(Target target, bool hovered, bool pressed)
{
    if (!Exists(target)) return;
    auto& world = m_Scene->GetRegistry();
    if (!hovered && !pressed) world.remove<UIInteractionStateComponent>(target.entity);
    else world.emplace_or_replace<UIInteractionStateComponent>(target.entity, hovered, pressed);
}

void Engine::UIEventSystem::Queue(std::vector<UIPointerEvent>& events, EUIPointerEventType type, Target target) const
{
    if (target.entity != entt::null)
        events.push_back({ type, target.entity, target.id, m_Canvas.entity, m_Canvas.id, m_Position, m_Delta });
}

void Engine::UIEventSystem::Reset(std::vector<UIPointerEvent>* events)
{
    if (events)
    {
        Queue(*events, EUIPointerEventType::Cancel, m_Pressed);
        Queue(*events, EUIPointerEventType::Exit, m_Hovered);
    }
    SetState(m_Pressed, false, false);
    SetState(m_Hovered, false, false);
    m_Pressed = m_Hovered = {};
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
    if (m_Pressed.entity != entt::null && !CanInteract(m_Pressed, layouts))
    {
        Queue(events, EUIPointerEventType::Cancel, m_Pressed);
        SetState(m_Pressed, false, false);
        m_Pressed = {};
        m_SuppressUntilRelease = true;
    }
    bool blocked = false;
    const Target target = input.insideViewport ? HitTest(m_Position, layouts, blocked) : Target{};
    m_PointerOverUI = blocked;
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
        m_Pressed = target;
        SetState(m_Pressed, true, true);
        Queue(events, EUIPointerEventType::Down, m_Pressed);
    }
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
    if (m_Pressed.entity != entt::null) SetState(m_Pressed, m_Pressed == m_Hovered, true);
    if (m_Hovered.entity != entt::null) SetState(m_Hovered, true, m_Hovered == m_Pressed);
    Dispatch(events);
    return true;
}

void Engine::UIEventSystem::Dispatch(const std::vector<UIPointerEvent>& events)
{
    for (const auto& event : events)
    {
        if (event.type != EUIPointerEventType::Cancel && event.type != EUIPointerEventType::Exit)
        {
            std::vector<UIElementLayout> layouts;
            if (!m_Scene->IsActive() || !Exists(m_Canvas) ||
                !UILayout::Build(m_Scene->GetRegistry(), m_Canvas.entity, layouts) ||
                !CanInteract({ event.target, event.targetId }, layouts)) continue;
            if (event.type == EUIPointerEventType::Click)
            {
                bool blocked = false;
                if (HitTest(event.position, layouts, blocked) != Target{ event.target, event.targetId }) continue;
            }
        }
        // All state changes finish before callbacks. No registry references survive a callback.
        m_OnPointerEvent.Broadcast(event);
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
    const auto* button = world.try_get<UIButtonComponent>(entity);
    if (!button || !UILayout::IsValid(*button)) return glm::vec4(1.0f);
    if (!button->interactable) return button->disabledColor;
    const auto* state = world.try_get<UIInteractionStateComponent>(entity);
    if (state && state->hovered) return state->pressed ? button->pressedColor : button->hoveredColor;
    return button->normalColor;
}
