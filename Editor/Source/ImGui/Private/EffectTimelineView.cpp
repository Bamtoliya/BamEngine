#include "Editor_Includes.h"
#include "EffectTimelineView.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"
#include "InputManager.h"
#include "ShortcutManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace EditorUI
{
    float TimelineScale::ToX(float time) const { return Left + (time - Start) / Span * Width; }
    float TimelineScale::ToTime(float x) const { return Start + (x - Left) / Width * Span; }

    TimelineScale MakeTimelineScale(float left, float width, float duration, float zoom, float start)
    {
        const float span = std::max(duration, MinimumTimelineDuration) / std::clamp(zoom, 1.0f, 40.0f);
        return { left, std::max(1.0f, width), std::clamp(start, 0.0f, std::max(0.0f, duration - span)), span };
    }

    void NavigateTimeline(float mouseX, float wheel, float panPixels, const TimelineScale& scale,
        float duration, float& zoom, float& start)
    {
        if (wheel != 0.0f)
        {
            const float anchor = std::clamp((mouseX - scale.Left) / scale.Width, 0.0f, 1.0f);
            zoom = std::clamp(zoom * std::pow(1.25f, wheel), 1.0f, 40.0f);
            start = scale.Start + anchor * (scale.Span - duration / zoom);
        }
        start -= panPixels / scale.Width * (duration / zoom);
        start = std::clamp(start, 0.0f, std::max(0.0f, duration - duration / zoom));
    }

    void DrawTimelineRuler(ImDrawList& draw, const TimelineScale& scale, float top, float bottom, float fps)
    {
        const float right = scale.Left + scale.Width;
        const float font = ImGui::GetFontSize();
        draw.AddRectFilled(ImVec2(scale.Left, top), ImVec2(right, bottom), TimelineRulerBackground);
        draw.AddLine(ImVec2(scale.Left, bottom), ImVec2(right, bottom), IM_COL32(55, 60, 70, 255));
        draw.PushClipRect(ImVec2(scale.Left, top), ImVec2(right, bottom), true);
        const float minTick = scale.Span * font / scale.Width;
        float tick = 1.0f / std::max(1.0f, fps);
        while (tick < minTick) tick *= 2.0f;
        float labelStep = tick * 4.0f;
        const float labelWidth = ImGui::CalcTextSize("0000  00.00s").x;
        while (labelStep / scale.Span * scale.Width < labelWidth) labelStep *= 2.0f;
        float time = std::ceil(scale.Start / tick) * tick;
        for (int i = 0; i < 512 && time <= scale.Start + scale.Span; ++i, time += tick)
        {
            const float x = scale.ToX(time);
            const float snapped = std::round(time / labelStep) * labelStep;
            const bool major = std::abs(time - snapped) <= tick * 0.25f;
            draw.AddLine(ImVec2(x, bottom - font * (major ? 0.6f : 0.3f)), ImVec2(x, bottom),
                major ? IM_COL32(180, 190, 205, 255) : IM_COL32(90, 95, 105, 200));
            if (major)
            {
                char label[40];
                std::snprintf(label, sizeof(label), "%.0f  %.2fs", std::round(time * fps), time);
                draw.AddText(ImVec2(x + 2.0f, top + font * 0.15f), ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
            }
        }
        draw.PopClipRect();
    }

    void DrawTimelinePlayhead(ImDrawList& draw, const ImVec2& min, const ImVec2& max, float x)
    {
        if (x < min.x || x > max.x) return;
        const float width = ImGui::GetFontSize() * 0.35f;
        draw.PushClipRect(min, max, true);
        draw.AddLine(ImVec2(x, min.y), ImVec2(x, max.y), TimelinePlayheadColor, 2.0f);
        draw.AddTriangleFilled(ImVec2(x - width, min.y), ImVec2(x + width, min.y),
            ImVec2(x, min.y + width * 1.5f), TimelinePlayheadColor);
        draw.PopClipRect();
    }

    TimelineInteraction DrawEffectTimeline(EffectTimelineState& state, std::span<const TimelineRow> rows,
        float duration, float playhead, float minimumDuration)
    {
        TimelineInteraction result;
        duration = std::max(duration, MinimumTimelineDuration);
        minimumDuration = std::max(minimumDuration, MinimumTimelineDuration);
        const float viewDuration = std::max(duration, minimumDuration);

        const auto origin = ImGui::GetCursorScreenPos();
        const auto avail = ImGui::GetContentRegionAvail();
        if (avail.x < 2.0f) return result;
        const float rowHeight = ImGui::GetFrameHeightWithSpacing();
        const float rulerHeight = rowHeight * 1.25f;
        const float headerMax = avail.x * 0.55f;
        const float headerMin = std::min(ImGui::GetFontSize() * 5.0f, headerMax);
        const float header = std::clamp(state.HeaderWidth, headerMin, headerMax);
        const float height = std::max(avail.y, rulerHeight + rowHeight * rows.size());
        const float right = origin.x + avail.x;
        const float endY = origin.y + height;
        const float rulerY = origin.y + rulerHeight;
        const float edge = ImGui::GetFontSize() * 0.4f;
        auto scale = MakeTimelineScale(origin.x + header, avail.x - header, viewDuration, state.Zoom, state.Start);
        auto& input = Engine::InputManager::Get();
        const auto mouse = input.GetScreenMousePosition();
        const bool held = input.IsMouseButtonPressed(Engine::EMouseButton::Left);

        ImGui::InvisibleButton("##EffectTimelineCanvas", ImVec2(avail.x, height));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered && !state.Drag)
        {
            const float wheel = input.GetMouseScrollDeltaY();
            if (mouse.x >= scale.Left)
            {
                const bool panning = input.IsMouseButtonDragging(Engine::EMouseButton::Middle) ||
                    (input.IsAltPressed() && input.IsMouseButtonDragging(Engine::EMouseButton::Right));
                NavigateTimeline(mouse.x, wheel, panning ? input.GetMouseDelta().x : 0.0f,
                    scale, viewDuration, state.Zoom, state.Start);
                scale = MakeTimelineScale(scale.Left, scale.Width, viewDuration, state.Zoom, state.Start);
            }
            else if (wheel != 0.0f)
                ImGui::SetScrollY(ImGui::GetScrollY() - wheel * rowHeight * 3.0f);
        }

        const float durationX = scale.ToX(duration);
        const float lastHandleX = std::max(scale.Left, right - 1.0f);
        const float durationHandleX = std::clamp(durationX, scale.Left, lastHandleX);
        const bool overDuration = mouse.y < rulerY && mouse.x >= scale.Left &&
            durationX >= scale.Left && durationX <= right + 1.0f && std::abs(mouse.x - durationHandleX) <= edge;
        const bool overHeader = std::abs(mouse.x - scale.Left) <= edge * 0.5f;
        if (hovered && (overDuration || overHeader)) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActivated())
        {
            state.Drag.reset();
            if (overDuration)
                state.Drag = TimelineEdit{ TimelineDragMode::Duration, 0, 0.0f, duration };
            else if (overHeader)
                state.Drag = TimelineEdit{ TimelineDragMode::Header, 0, header, 0.0f };
            else if (mouse.y < rulerY && mouse.x >= scale.Left)
                state.Drag = TimelineEdit{ TimelineDragMode::Seek, 0, 0.0f, 0.0f };
            else if (mouse.y >= rulerY)
            {
                const auto index = static_cast<std::size_t>((mouse.y - rulerY) / rowHeight);
                if (index < rows.size())
                {
                    const auto& row = rows[index];
                    const float x0 = scale.ToX(row.Start);
                    const float x1 = scale.ToX(row.End);
                    const bool overBar = mouse.x >= scale.Left && mouse.x >= x0 - edge &&
                        mouse.x <= std::max(x0 + edge, x1) + edge;
                    if (mouse.x < scale.Left || overBar) result.Selection = row.Id;
                    if (overBar)
                    {
                        auto mode = TimelineDragMode::Move;
                        if (row.Kind != TimelineRowKind::Marker)
                        {
                            const float startDistance = std::abs(mouse.x - x0);
                            const float endDistance = std::abs(mouse.x - x1);
                            if (startDistance <= edge && startDistance <= endDistance) mode = TimelineDragMode::Start;
                            else if (endDistance <= edge) mode = TimelineDragMode::End;
                        }
                        state.Drag = TimelineEdit{ mode, row.Id, row.Start, row.End };
                    }
                }
            }
            if (state.Drag)
            {
                state.DragScale = scale;
                state.GrabX = mouse.x;
                state.OriginalStart = state.Drag->Start;
                state.OriginalEnd = state.Drag->End;
            }
        }

        if (state.Drag && (input.IsKeyButtonDown(Engine::EKeyCode::Escape) ||
            (!ImGui::IsItemActive() && !ImGui::IsItemDeactivated())))
            state.Drag.reset();
        if (state.Drag)
        {
            auto& edit = *state.Drag;
            const float delta = (mouse.x - state.GrabX) * state.DragScale.Span / state.DragScale.Width;
            const float limit = std::max(600.0f, viewDuration);
            const float length = std::max(0.0f, state.OriginalEnd - state.OriginalStart);
            switch (edit.Mode)
            {
            case TimelineDragMode::Header:
                if (held) state.HeaderWidth = std::clamp(state.OriginalStart + mouse.x - state.GrabX, headerMin, headerMax);
                break;
            case TimelineDragMode::Seek:
                if (held || ImGui::IsItemActivated()) result.Seek = std::clamp(scale.ToTime(mouse.x), 0.0f, duration);
                break;
            case TimelineDragMode::Duration:
                edit.End = std::clamp(state.OriginalEnd + delta, minimumDuration, std::max(minimumDuration, limit));
                break;
            case TimelineDragMode::Start:
                edit.Start = std::clamp(state.OriginalStart + delta, 0.0f,
                    std::max(0.0f, state.OriginalEnd - MinimumTimelineDuration));
                break;
            case TimelineDragMode::End:
                edit.End = std::clamp(state.OriginalEnd + delta, edit.Start + MinimumTimelineDuration,
                    std::max(edit.Start + MinimumTimelineDuration, limit));
                break;
            case TimelineDragMode::Move:
                edit.Start = std::clamp(state.OriginalStart + delta, 0.0f, std::max(0.0f, limit - length));
                edit.End = edit.Start + length;
                break;
            }
            if (!held)
            {
                if (edit.Mode != TimelineDragMode::Header && edit.Mode != TimelineDragMode::Seek &&
                    std::abs(mouse.x - state.GrabX) >= 1.0f &&
                    (edit.Start != state.OriginalStart || edit.End != state.OriginalEnd))
                    result.Commit = edit;
                state.Drag.reset();
            }
        }

        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(right, rulerY), TimelineRulerBackground);
        const std::string title = std::string(ICON_FA_LAYER_GROUP " ") + LOCAL("Timeline.Tracks");
        draw->PushClipRect(origin, ImVec2(scale.Left, rulerY), true);
        draw->AddText(ImVec2(origin.x + edge, origin.y + edge), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
        draw->PopClipRect();

        DrawTimelineRuler(*draw, scale, origin.y, rulerY, EffectTimelineFPS);
        draw->PushClipRect(ImVec2(scale.Left, rulerY), ImVec2(right, endY), true);
        const float gridStep = std::max(1.0f / EffectTimelineFPS, scale.Span * ImGui::GetFontSize() * 4.0f / scale.Width);
        for (int i = 0; i < 256; ++i)
        {
            const float x = scale.ToX((std::ceil(scale.Start / gridStep) + i) * gridStep);
            if (x > right) break;
            draw->AddLine(ImVec2(x, rulerY), ImVec2(x, endY), IM_COL32(70, 78, 92, 160));
        }

        draw->PopClipRect();

        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const auto& row = rows[i];
            const float y = rulerY + i * rowHeight;
            const auto textColor = ImGui::GetColorU32(row.Selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
            if (row.Selected) draw->AddRectFilled(ImVec2(origin.x, y), ImVec2(right, y + rowHeight),
                ImGui::GetColorU32(ImGuiCol_Header));
            draw->PushClipRect(ImVec2(origin.x, y), ImVec2(scale.Left, y + rowHeight), true);
            draw->AddText(ImVec2(origin.x + edge, y + edge * 0.5f), row.Color, row.Icon);
            draw->AddText(ImVec2(origin.x + edge * 2.0f + ImGui::CalcTextSize(row.Icon).x, y + edge * 0.5f),
                textColor, row.Label.data(), row.Label.data() + row.Label.size());
            draw->PopClipRect();

            const auto* edit = state.Drag && state.Drag->Row == row.Id &&
                state.Drag->Mode != TimelineDragMode::Duration && state.Drag->Mode != TimelineDragMode::Seek &&
                state.Drag->Mode != TimelineDragMode::Header ? &*state.Drag : nullptr;
            const float x0 = scale.ToX(edit ? edit->Start : row.Start);
            const float x1 = scale.ToX(edit ? edit->End : row.End);
            const float mid = y + rowHeight * 0.5f;
            draw->PushClipRect(ImVec2(scale.Left, y), ImVec2(right, y + rowHeight), true);
            if (row.Kind == TimelineRowKind::Marker)
                draw->AddQuadFilled(ImVec2(x0 - edge, mid), ImVec2(x0, mid - edge), ImVec2(x0 + edge, mid),
                    ImVec2(x0, mid + edge), row.Color);
            else
            {
                const ImVec2 min(x0, y + edge * 0.5f);
                const ImVec2 max(std::max(x0 + 1.0f, x1), y + rowHeight - edge * 0.5f);
                draw->AddRectFilled(min, max, row.Color, 2.0f);
                if (row.Selected) draw->AddRect(min, max, IM_COL32(255, 255, 255, 230), 2.0f);
                if (row.Selected || edit)
                {
                    draw->AddLine(ImVec2(x0 + 2.0f, min.y), ImVec2(x0 + 2.0f, max.y), IM_COL32_WHITE, 2.0f);
                    draw->AddLine(ImVec2(x1 - 2.0f, min.y), ImVec2(x1 - 2.0f, max.y), IM_COL32_WHITE, 2.0f);
                }
            }
            draw->PopClipRect();
            if (hovered && mouse.y >= y && mouse.y < y + rowHeight)
            {
                if (mouse.x < scale.Left)
                {
                    if (row.Kind == TimelineRowKind::OpenEnd)
                        ImGui::SetTooltip("%.*s\n%s", static_cast<int>(row.Label.size()), row.Label.data(),
                            LOCAL_CSTR("EffectTimeline.OpenEmission"));
                    else ImGui::SetTooltip("%.*s", static_cast<int>(row.Label.size()), row.Label.data());
                }
                else if (row.Kind != TimelineRowKind::Marker &&
                    (std::abs(mouse.x - x0) <= edge || std::abs(mouse.x - x1) <= edge))
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
        }

        draw->AddLine(ImVec2(scale.Left, origin.y), ImVec2(scale.Left, endY), IM_COL32(70, 78, 92, 255));
        DrawTimelinePlayhead(*draw, ImVec2(scale.Left, origin.y), ImVec2(right, endY),
            scale.ToX(result.Seek.value_or(playhead)));
        const bool resizingDuration = state.Drag && state.Drag->Mode == TimelineDragMode::Duration;
        if (durationX >= scale.Left && durationX <= right + 1.0f)
        {
            const float x = resizingDuration ? std::clamp(scale.ToX(state.Drag->End), scale.Left, lastHandleX) : durationHandleX;
            draw->AddRectFilled(ImVec2(std::max(scale.Left, x - edge), rulerY - edge * 1.5f),
                ImVec2(std::min(right, x + edge), rulerY), IM_COL32(235, 197, 96, 255), 2.0f);
            if ((hovered && overDuration) || resizingDuration)
                ImGui::SetTooltip("%s  %.2f", LOCAL_CSTR("Timeline.Duration"), resizingDuration ? state.Drag->End : duration);
        }
        return result;
    }

    void DrawEffectEditorLayout(float& listWidth, float& inspectorWidth, float& timelineHeight,
        const std::function<void()>& list, const std::function<void()>& preview,
        const std::function<void()>& inspector, const std::function<void()>& timeline)
    {
        const auto avail = ImGui::GetContentRegionAvail();
        if (avail.x < 2.0f || avail.y < 4.0f) return;
        const auto spacing = ImGui::GetStyle().ItemSpacing;
        const float font = ImGui::GetFontSize();
        const float splitter = std::max(2.0f, font * 0.25f);
        const float height = std::max(2.0f, avail.y - splitter - spacing.y * 2.0f);
        const float minTimeline = std::min(font * 4.0f, height * 0.5f);
        const float maxTimeline = std::max(minTimeline, height - font * 5.0f);
        const float timelineSize = std::clamp(timelineHeight, minTimeline, maxTimeline);
        const float topSize = height - timelineSize;
        auto& input = Engine::InputManager::Get();

        const auto split = [](const char* id, const ImVec2& size, ImGuiMouseCursor cursor)
        {
            ImGui::InvisibleButton(id, size);
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(cursor);
            ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_SeparatorActive : ImGuiCol_Separator));
            return ImGui::IsItemActive();
        };

        const float horizontalReserve = 2.0f * splitter + 4.0f * spacing.x;
        if (avail.x >= font * 32.0f + horizontalReserve)
        {
            const float maxSides = avail.x - horizontalReserve - font * 12.0f;
            const float minSide = std::min(font * 7.0f, maxSides * 0.5f);
            const float left = std::clamp(listWidth, minSide, maxSides - minSide);
            const float right = std::clamp(inspectorWidth, minSide, maxSides - left);
            const float center = avail.x - horizontalReserve - left - right;
            if (ImGui::BeginChild("##EffectList", ImVec2(left, topSize), ImGuiChildFlags_Borders)) list();
            ImGui::EndChild();
            ImGui::SameLine();
            if (split("##EffectLeftSplitter", ImVec2(splitter, topSize), ImGuiMouseCursor_ResizeEW))
                listWidth = std::clamp(left + input.GetMouseDelta().x, minSide, maxSides - right);
            ImGui::SameLine();
            if (ImGui::BeginChild("##EffectPreview", ImVec2(center, topSize), ImGuiChildFlags_Borders)) preview();
            ImGui::EndChild();
            ImGui::SameLine();
            if (split("##EffectRightSplitter", ImVec2(splitter, topSize), ImGuiMouseCursor_ResizeEW))
                inspectorWidth = std::clamp(right - input.GetMouseDelta().x, minSide, maxSides - left);
            ImGui::SameLine();
            if (ImGui::BeginChild("##EffectInspector", ImVec2(0.0f, topSize), ImGuiChildFlags_Borders)) inspector();
            ImGui::EndChild();
        }
        else
        {
            if (ImGui::BeginChild("##EffectCompact", ImVec2(0.0f, topSize), ImGuiChildFlags_Borders) &&
                ImGui::BeginTabBar("##EffectPaneTabs"))
            {
                if (ImGui::BeginTabItem(LOCAL_LABEL("Timeline.Tracks").c_str())) { list(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem(LOCAL_LABEL("EffectTimeline.Preview").c_str())) { preview(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem(LOCAL_LABEL("EffectTimeline.Properties").c_str())) { inspector(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
            ImGui::EndChild();
        }

        if (split("##EffectTimelineSplitter", ImVec2(avail.x, splitter), ImGuiMouseCursor_ResizeNS))
            timelineHeight = std::clamp(timelineSize - input.GetMouseDelta().y, minTimeline, maxTimeline);
        if (ImGui::BeginChild("##EffectTimeline", ImVec2(0.0f, timelineSize), ImGuiChildFlags_Borders,
            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            timeline();
        ImGui::EndChild();
    }

    void DrawTimelineAction(ToolbarLayout& toolbar, std::string_view prefix, const char* action,
        const char* icon, const char* label, bool active)
    {
        auto& shortcuts = Editor::ShortcutManager::Get();
        const auto id = std::string(prefix) + "." + action;
        IconButtonOptions options;
        options.Active = active;
        options.Enabled = shortcuts.CanExecuteAction(id);
        options.ShortcutHint = shortcuts.GetShortcutHint(id);
        if (toolbar.Icon(action, icon, label, options)) shortcuts.ExecuteAction(id);
    }

    void DrawTimelineTransport(ToolbarLayout& toolbar, std::string_view prefix, bool playing, bool looping)
    {
        DrawTimelineAction(toolbar, prefix, "FirstFrame", ICON_FA_BACKWARD_FAST, "Action.Timeline.FirstFrame");
        DrawTimelineAction(toolbar, prefix, "PrevFrame", ICON_FA_BACKWARD_STEP, "Action.Timeline.PrevFrame");
        DrawTimelineAction(toolbar, prefix, "PlayPause", playing ? ICON_FA_PAUSE : ICON_FA_PLAY, "Action.Timeline.PlayPause");
        DrawTimelineAction(toolbar, prefix, "NextFrame", ICON_FA_FORWARD_STEP, "Action.Timeline.NextFrame");
        DrawTimelineAction(toolbar, prefix, "LastFrame", ICON_FA_FORWARD_FAST, "Action.Timeline.LastFrame");
        DrawTimelineAction(toolbar, prefix, "ToggleLoop", ICON_FA_REPEAT, "Action.Timeline.ToggleLoop", looping);
        DrawTimelineAction(toolbar, prefix, "ZoomReset", ICON_FA_MAXIMIZE, "Action.Timeline.ZoomReset");
    }
}
