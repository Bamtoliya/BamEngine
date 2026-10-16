#pragma once

#include "BaseViewportPanel.h"
#include "CanvasView2D.h"
#include "UILayout.h"
#include "UIEventSystem.h"
#include <optional>

namespace Editor
{
    class UIViewportPanel : public BaseViewportPanel
    {
    public:
        UIViewportPanel() { m_Name = L"UI View"; }
        void Initialize(void* arg = nullptr) override;
        void Free() override;
        void Update(f32 dt) override;
        void Draw() override;

    protected:
        void DrawCustomOptions() override;
        void DrawCustomViewport() override;

    private:
        void SetTarget(Engine::Scene* scene, entt::entity canvas);
        void ResolveTarget();
        bool HasTarget() const;
        void DrawCanvasMenu();
        void DrawOrderMenu();
        void SetZIndex(entt::entity entity, std::int32_t zIndex);
        void DrawGrid(ImDrawList* drawList);
        void DrawSelection(ImDrawList* drawList);
        void HandleInput(bool hovered);
        void HandleInteraction(bool hovered);
        void ApplyPendingEdit();
        const Engine::UIElementLayout* FindLayout(entt::entity entity) const;

        CanvasView2D m_View;
        Engine::Scene* m_TargetScene = nullptr;
        entt::entity m_TargetCanvas = entt::null;
        uint64 m_TargetCanvasId = 0;
        vec2 m_CanvasSize{ 1920.0f, 1080.0f };
        wstring m_CanvasRTName;
        wstring m_CanvasToDraw;
        RenderPassID m_UIOverlayPassID = INVALID_PASS_ID;
        std::vector<Engine::UIElementLayout> m_Layouts;
        bool m_LayoutValid = false;
        bool m_PreviewReady = false;
        Engine::Entity* m_LastSelection = nullptr;
        bool m_FitPending = true;
        bool m_ShowGrid = true;
        bool m_ShowSafeArea = true;
        bool m_ShowAnchors = true;
        bool m_GridSnap = true;
        bool m_PixelSnap = false;
        float m_GridStep = 10.0f;
        bool m_Panning = false;
        bool m_InteractionPreview = false;
        bool m_InteractionUpdated = false;
        std::unique_ptr<Engine::UIEventSystem> m_Interaction;
        std::optional<Engine::EUIPointerEventType> m_LastUIEvent;
        uint64 m_LastUIEventTarget = 0;
        uint64 m_ClickCount = 0;

        enum class DragMode { None, Move, Resize };
        struct DragState
        {
            DragMode mode = DragMode::None;
            entt::entity entity = entt::null;
            uint64 id = 0;
            vec2 mouseStart{ 0.0f };
            Engine::RectTransformComponent start;
            Engine::UIElementLayout layout;
        };
        struct PendingEdit
        {
            entt::entity entity = entt::null;
            uint64 id = 0;
            Engine::RectTransformComponent before;
            Engine::RectTransformComponent after;
        };
        DragState m_Drag;
        std::optional<PendingEdit> m_PendingEdit;
        uint64 m_HistoryRevision = 0;
    };
}
