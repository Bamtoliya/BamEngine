#include "CanvasView2D.h"
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
    std::cout << "Canvas view conversion, zoom, pan and snap checks passed.\n";
    return 0;
}
