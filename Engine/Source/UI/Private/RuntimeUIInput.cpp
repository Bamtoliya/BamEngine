#include "RuntimeUIInput.h"
#include "UIClipLayout.h"
#include "ComponentEnable.h"
#include "InputManager.h"
#include "Scene.h"
#include "WorldComponents.h"
#include <algorithm>

namespace
{
    struct RuntimeUIPointerEvents
    {
        std::vector<Engine::UIPointerEvent> events;
    };

    void ClearFrameEvents(Engine::Scene& scene)
    {
        if (auto* events = scene.GetRegistry().ctx().find<RuntimeUIPointerEvents>())
            events->events.clear();
    }
}

std::span<const Engine::UIPointerEvent> Engine::GetRuntimeUIPointerEvents(const Scene& scene)
{
    const auto* events = scene.GetRegistry().ctx().find<RuntimeUIPointerEvents>();
    return events ? std::span<const UIPointerEvent>(events->events) : std::span<const UIPointerEvent>{};
}

void Engine::RuntimeUIInput::Clear()
{
    if (InputManager::IsCreated()) InputManager::Get().EvaluateActionMap("GameUI", false);
    for (const auto& canvas : m_Canvases)
        ClearFrameEvents(*canvas.scene);
    m_Canvases.clear();
    m_BlocksGameplayPress = false;
}

bool Engine::RuntimeUIInput::Update(const std::vector<Scene*>& scenes, const UIPointerInput& input, bool* wheelConsumed)
{
    if (wheelConsumed) *wheelConsumed = false;
    InputManager* actions = InputManager::IsCreated() ? &InputManager::Get() : nullptr;
    if (actions) actions->EvaluateActionMap("GameUI", input.focused);
    for (const auto& canvas : m_Canvases)
        ClearFrameEvents(*canvas.scene);

    std::vector<CanvasInput> canvases;
    for (auto* scene : scenes)
    {
        if (!scene || !scene->IsActive() || scene->IsPaused()) continue;
        auto& world = scene->GetRegistry();
        const auto firstCanvas = canvases.size();
        for (const auto canvas : world.view<CanvasComponent, IDComponent>())
        {
            if (!IsComponentEnabled<CanvasComponent>(world, canvas)) continue;
            const auto id = world.get<IDComponent>(canvas).id;
            const auto old = std::ranges::find_if(m_Canvases, [scene, canvas, id](const auto& entry)
            {
                return entry.scene == scene && entry.canvas == canvas && entry.id == id;
            });
            if (old != m_Canvases.end())
            {
                canvases.push_back({ scene, canvas, id, std::move(old->events) });
                continue;
            }

            auto eventSystem = std::make_unique<UIEventSystem>(*scene, canvas);
            eventSystem->OnPointerEvent().AddLambda([scene](const UIPointerEvent& event)
            {
                auto& context = scene->GetRegistry().ctx();
                auto* events = context.find<RuntimeUIPointerEvents>();
                if (!events) events = &context.emplace<RuntimeUIPointerEvents>();
                events->events.push_back(event);
            });
            canvases.push_back({ scene, canvas, id, std::move(eventSystem) });
        }

        // The UI pass submits each scene's canvases back to front by order, then persistent ID.
        std::sort(canvases.begin() + firstCanvas, canvases.end(), [&](const auto& left, const auto& right)
        {
            const auto leftOrder = UILayout::GetZIndex(world, left.canvas);
            const auto rightOrder = UILayout::GetZIndex(world, right.canvas);
            if (leftOrder != rightOrder) return leftOrder < rightOrder;
            return left.id < right.id;
        });
    }

    m_Canvases = std::move(canvases);
    if (input.focused && input.insideViewport && !input.held && !input.down && !input.up &&
        std::isfinite(input.wheelDelta) && input.wheelDelta != 0.0f)
    {
        for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
        {
            auto& world = it->scene->GetRegistry();
            std::vector<UIElementLayout> layouts;
            if (!UILayout::Build(world, it->canvas, layouts)) continue;
            const auto position = input.position * world.get<CanvasComponent>(it->canvas).referenceResolution;
            if (UIClipLayout::ScrollAt(world, layouts, position, input.wheelDelta))
            {
                if (wheelConsumed) *wheelConsumed = true;
                break;
            }
            bool blocked = false;
            it->events->HitTest(position, layouts, blocked);
            if (blocked) break;
        }
    }

    if (input.down && input.downPosition)
    {
        auto press = input;
        press.position = *input.downPosition;
        press.insideViewport = input.downInsideViewport;
        press.held = true;
        press.up = false;
        m_BlocksGameplayPress = UpdateCanvases(press);
    }

    auto current = input;
    if (input.downPosition) current.down = false;
    const bool blocked = UpdateCanvases(current);
    if (current.down) m_BlocksGameplayPress = blocked;
    const bool suppressGameplay = m_BlocksGameplayPress;
    if (!input.held) m_BlocksGameplayPress = false;
    if (actions && input.focused && !input.held && !input.down && !input.up)
    {
        const bool hasFocusedInput = HasFocusedTextInput();
        if (hasFocusedInput)
        {
            UIKeyModifiers mods;
            mods.shift = actions->IsShiftPressed();
            mods.ctrl = actions->IsCtrlPressed();
            mods.alt = actions->IsAltPressed();

            const auto pollKey = [&](EKeyCode code, EUIKey uiKey)
            {
                if (actions->IsKeyButtonDown(code) || actions->IsKeyButtonRepeated(code))
                    ForwardKeyInput(uiKey, mods);
            };

            pollKey(EKeyCode::Backspace, EUIKey::Backspace);
            pollKey(EKeyCode::Delete, EUIKey::Delete);
            pollKey(EKeyCode::Left, EUIKey::Left);
            pollKey(EKeyCode::Right, EUIKey::Right);
            pollKey(EKeyCode::Home, EUIKey::Home);
            pollKey(EKeyCode::End, EUIKey::End);
            pollKey(EKeyCode::Enter, EUIKey::Enter);
            pollKey(EKeyCode::NumpadEnter, EUIKey::Enter);
            pollKey(EKeyCode::Escape, EUIKey::Escape);

            if (mods.ctrl)
            {
                pollKey(EKeyCode::A, EUIKey::A);
                pollKey(EKeyCode::C, EUIKey::C);
                pollKey(EKeyCode::V, EUIKey::V);
                pollKey(EKeyCode::X, EUIKey::X);
            }
        }
        else if (actions->IsKeyButtonDown(EKeyCode::Escape) && HasOpenDropdown())
        {
            ForwardKeyInput(EUIKey::Escape);
        }
        else
        {
            const int direction = static_cast<int>(actions->WasActionPressed("GameUI", "NextButton")) -
                static_cast<int>(actions->WasActionPressed("GameUI", "PreviousButton"));
            const bool submit = actions->WasActionPressed("GameUI", "ActivateButton");
            const int adjust = static_cast<int>(actions->WasActionPressed("GameUI", "IncreaseValue")) -
                static_cast<int>(actions->WasActionPressed("GameUI", "DecreaseValue"));
            if (direction != 0 || submit || adjust != 0)
            {
                const auto focused = std::ranges::find_if(m_Canvases, [](const auto& canvas)
                {
                    return canvas.events->m_KeyboardFocus.entity != entt::null;
                });
                if (focused != m_Canvases.end())
                {
                    if (direction != 0 || submit) focused->events->Navigate(direction, submit);
                    if (adjust != 0) focused->events->AdjustFocusedSlider(adjust);
                }
                else if (direction != 0)
                {
                    for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
                        if (it->events->Navigate(direction, submit)) break;
                }
            }
        }
    }
    return suppressGameplay;
}

bool Engine::RuntimeUIInput::UpdateCanvases(const UIPointerInput& input)
{
    const CanvasInput* hit = nullptr;
    if (input.focused && input.insideViewport)
    {
        for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
        {
            const auto& world = it->scene->GetRegistry();
            std::vector<UIElementLayout> layouts;
            if (!UILayout::Build(world, it->canvas, layouts)) continue;
            const auto position = input.position * world.get<CanvasComponent>(it->canvas).referenceResolution;
            bool blocked = false;
            it->events->HitTest(position, layouts, blocked);
            if (!blocked) continue;
            hit = &*it;
            break;
        }
    }

    bool blocksGameplay = hit != nullptr;
    for (const auto& canvas : m_Canvases)
    {
        auto canvasInput = input;
        canvasInput.position *= canvas.scene->GetRegistry().get<CanvasComponent>(canvas.canvas).referenceResolution;
        canvasInput.insideViewport = &canvas == hit;
        blocksGameplay |= canvas.events->HasPointerCapture();
        canvas.events->Update(canvasInput);
    }
    return blocksGameplay;
}

bool Engine::RuntimeUIInput::HasFocusedTextInput() const
{
    return std::ranges::any_of(m_Canvases, [](const auto& canvas)
    {
        return canvas.events && canvas.events->HasFocusedTextInput();
    });
}

bool Engine::RuntimeUIInput::HasOpenDropdown() const
{
    return std::ranges::any_of(m_Canvases, [](const auto& canvas)
    {
        return canvas.events && canvas.events->HasOpenDropdown();
    });
}

bool Engine::RuntimeUIInput::ForwardKeyInput(EUIKey key, UIKeyModifiers mods, std::string* clipboardOut, std::string_view clipboardIn)
{
    if (key == EUIKey::Escape)
    {
        for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
        {
            if (it->events && it->events->HasOpenDropdown())
            {
                if (it->events->HandleKeyInput(key, mods, clipboardOut, clipboardIn)) return true;
            }
        }
    }

    for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
    {
        if (it->events && it->events->HasFocusedTextInput())
        {
            if (it->events->HandleKeyInput(key, mods, clipboardOut, clipboardIn)) return true;
        }
    }

    if (key == EUIKey::Escape)
    {
        for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
        {
            if (it->events && it->events->HandleKeyInput(key, mods, clipboardOut, clipboardIn)) return true;
        }
    }

    return false;
}

bool Engine::RuntimeUIInput::ForwardTextInput(std::string_view utf8Text)
{
    if (utf8Text.empty()) return false;
    for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
    {
        if (it->events && it->events->HasFocusedTextInput())
        {
            if (it->events->HandleTextInput(utf8Text)) return true;
        }
    }
    return false;
}

bool Engine::RuntimeUIInput::ForwardImeComposition(std::string_view composition, bool commit)
{
    for (auto it = m_Canvases.rbegin(); it != m_Canvases.rend(); ++it)
    {
        if (it->events && it->events->HasFocusedTextInput())
        {
            if (it->events->HandleImeComposition(composition, commit)) return true;
        }
    }
    return false;
}
