#include "CanvasView2D.h"
#include "UILayout.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

int main()
{
    Editor::CanvasView2D view;
    view.viewportOrigin = { 100.0f, 50.0f };
    view.viewportSize = { 800.0f, 600.0f };
    view.Fit({ 1920.0f, 1080.0f });
    const glm::vec2 point{ 123.0f, 456.0f };
    if (glm::length(view.ToCanvas(view.ToScreen(point)) - point) > 0.001f) return 1;
    const glm::vec2 cursor{ 325.0f, 275.0f };
    const auto anchor = view.ToCanvas(cursor);
    view.ZoomAt(cursor, 2.0f);
    if (glm::length(view.ToCanvas(cursor) - anchor) > 0.001f) return 2;
    const auto screen = view.ToScreen(point);
    const glm::vec2 delta{ 35.0f, -19.0f };
    view.Pan(delta);
    if (glm::length(view.ToScreen(point) - screen - delta) > 0.001f) return 3;
    for (float zoom : { 0.125f, 1.0f, 8.0f })
    {
        view.zoom = zoom;
        const auto canvas = view.ToCanvas(view.ToScreen({ 23.2f, -17.3f }));
        if (glm::length(Editor::CanvasView2D::Snap(canvas, 10.0f) - glm::vec2(20.0f, -20.0f)) > 0.001f)
            return 4;
    }
    const auto before = view.zoom;
    view.ZoomAt(cursor, 0.0f);
    if (view.zoom != before) return 5;

    Engine::RectTransformComponent rect;
    rect.anchorMin = { 0.1f, 0.3f };
    rect.anchorMax = { 0.4f, 0.5f };
    rect.pivot = { 0.23f, 0.61f };
    rect.scale = { 2.0f, 0.75f };
    rect.rotation = 24.0f;
    const glm::vec2 parentSize{ 400.0f, 350.0f };
    const glm::vec2 parentPivot{ 0.3f, 0.8f };
    const auto parentTransform = glm::rotate(glm::mat4(1.0f), glm::radians(-37.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(-1.25f, 1.5f, 1.0f));
    const auto localBasis = glm::rotate(glm::mat4(1.0f), glm::radians(rect.rotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(rect.scale, 1.0f));
    Engine::UIElementLayout start;
    if (!Engine::UILayout::Resolve(rect, parentSize, parentPivot, parentTransform, start)) return 6;
    const glm::vec2 corners[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    for (const auto& corner : corners)
    {
        const auto direction = 2.0f * corner - 1.0f;
        const auto resize = Editor::CanvasView2D::ResizeFromCorner(start.size, rect.pivot, corner,
            direction * glm::vec2(17.0f, 13.0f), 10.0f);
        if (glm::length(resize.sizeChange - glm::vec2(20.0f, 10.0f)) > 0.001f) return 7;

        auto resizedRect = rect;
        resizedRect.sizeDelta += resize.sizeChange;
        resizedRect.anchoredPosition += glm::vec2(localBasis * glm::vec4(resize.localPivotOffset, 0.0f, 0.0f));
        Engine::UIElementLayout resized;
        if (!Engine::UILayout::Resolve(resizedRect, parentSize, parentPivot, parentTransform, resized)) return 8;
        const glm::vec4 opposite(glm::vec2(1.0f) - corner, 0.0f, 1.0f);
        if (glm::length(resized.renderMatrix * opposite - start.renderMatrix * opposite) > 0.001f) return 9;

        const auto clamped = Editor::CanvasView2D::ResizeFromCorner(start.size, rect.pivot, corner,
            -direction * (start.size + glm::vec2(100.0f)), 0.0f);
        if (glm::length(start.size + clamped.sizeChange - glm::vec2(1.0f)) > 0.001f) return 10;
    }
    std::cout << "Canvas view conversion, zoom, pan, snap and corner resize checks passed.\n";
    return 0;
}
