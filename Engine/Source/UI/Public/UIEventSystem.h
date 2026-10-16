#pragma once

#include "UILayout.h"
#include "Delegate.h"

namespace Engine
{
    class Scene;
    class Entity;

    enum class EUIPointerEventType { Enter, Exit, Down, Up, Click, Cancel };

    struct UIPointerInput
    {
        glm::vec2 position{ 0.0f }; // Canvas pixels, Y down. The caller maps its input surface to these coordinates.
        bool insideViewport = false;
        bool focused = true;
        bool held = false;
        bool down = false;
        bool up = false;
    };

    struct UIPointerEvent
    {
        EUIPointerEventType type = EUIPointerEventType::Cancel;
        entt::entity target = entt::null;
        uint64 targetId = 0;
        entt::entity canvas = entt::null;
        uint64 canvasId = 0;
        glm::vec2 position{ 0.0f };
        glm::vec2 delta{ 0.0f };
    };

    // One owner per canvas/input surface. Keeps its scene alive and contains no Editor or SDL dependency.
    class ENGINE_API UIEventSystem final
    {
    public:
        UIEventSystem(Scene& scene, entt::entity canvas);
        ~UIEventSystem();
        UIEventSystem(const UIEventSystem&) = delete;
        UIEventSystem& operator=(const UIEventSystem&) = delete;

        // Reentrant Update/Cancel calls from a listener are rejected. Do not destroy this object in a listener.
        bool Update(const UIPointerInput& input);
        bool Cancel();
        MulticastDelegate<const UIPointerEvent&>& OnPointerEvent() { return m_OnPointerEvent; }
        Entity* ResolveTarget(const UIPointerEvent& event) const;
        bool IsPointerOverUI() const { return m_PointerOverUI; }
        bool HasPointerCapture() const { return m_Pressed.entity != entt::null; }
        static glm::vec4 GetButtonTint(const entt::registry& world, entt::entity entity);

    private:
        struct Target
        {
            entt::entity entity = entt::null;
            uint64 id = 0;
            bool operator==(const Target&) const = default;
        };
        bool Exists(Target target) const;
        bool CanInteract(Target target, const std::vector<UIElementLayout>& layouts) const;
        Target HitTest(const glm::vec2& point, const std::vector<UIElementLayout>& layouts, bool& blocked) const;
        void SetState(Target target, bool hovered, bool pressed);
        void Queue(std::vector<UIPointerEvent>& events, EUIPointerEventType type, Target target) const;
        void Reset(std::vector<UIPointerEvent>* events);
        void Dispatch(const std::vector<UIPointerEvent>& events);

        Scene* m_Scene;
        Target m_Canvas;
        Target m_Hovered;
        Target m_Pressed;
        glm::vec2 m_Position{ 0.0f };
        glm::vec2 m_Delta{ 0.0f };
        bool m_HasPosition = false;
        bool m_SuppressUntilRelease = true;
        bool m_PointerOverUI = false;
        bool m_Processing = false;
        MulticastDelegate<const UIPointerEvent&> m_OnPointerEvent;
    };
}
