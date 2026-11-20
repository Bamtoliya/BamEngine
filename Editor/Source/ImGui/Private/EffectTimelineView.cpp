#include "Editor_Includes.h"
#include "EffectTimelineView.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"
#include "InputManager.h"
#include "ShortcutManager.h"
#include "FloatCurve.h"
#include "ImGuiSplitter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fmt/format.h>

// 공통 타임라인 기본 기능(시간 축, 눈금자, 그리드, 행 배경, 막대)은 TimelineView.cpp에 있다.
namespace EditorUI
{
    namespace
    {
        struct ParticleTail
        {
            float Start;
            float End;
        };

        std::optional<ParticleTail> GetParticleTail(const TimelineRow& row, const TimelineEdit* edit)
        {
            if (!row.ParticleTiming) return std::nullopt;
            const auto& timing = *row.ParticleTiming;
            if (!timing.Enabled || !timing.Supported) return std::nullopt;
            if (!std::isfinite(timing.LifetimeMin) || !std::isfinite(timing.LifetimeMax)) return std::nullopt;

            const float start = edit ? edit->Start : row.Start;
            const bool bounded = row.Kind != TimelineRowKind::OpenEnd || (edit && edit->Mode == TimelineDragMode::End);
            if (!bounded && timing.EmissionLimit <= 0.0f) return std::nullopt;
            float end = bounded ? (edit ? edit->End : row.End) : timing.EmissionLimit;
            if (timing.EmissionLimit > 0.0f) end = std::min(end, timing.EmissionLimit);
            if (end <= start) return std::nullopt;
            const float tailEnd = end + std::max(0.001f, std::max(timing.LifetimeMin, timing.LifetimeMax));
            if (!std::isfinite(tailEnd)) return std::nullopt;
            return ParticleTail{ end, tailEnd };
        }
    }

    TimelineInteraction DrawEffectTimeline(EffectTimelineState& state, std::span<const TimelineRow> rows,
        float duration, float playhead, float minimumDuration, const char* addTrackTooltipKey)
    {
        TimelineInteraction result;
        duration = std::max(duration, MinimumTimelineDuration);
        minimumDuration = std::max(minimumDuration, MinimumTimelineDuration);
        float viewDuration = std::max(duration, minimumDuration);
        for (const auto& row : rows)
        {
            if (const auto tail = GetParticleTail(row, nullptr))
                viewDuration = std::max(viewDuration, tail->End);
        }

        const auto origin = ImGui::GetCursorScreenPos();
        const auto avail = ImGui::GetContentRegionAvail();
        if (avail.x < 2.0f || avail.y < 2.0f) return result;

        const float scrollbarWidth = ImGui::GetStyle().ScrollbarSize;
        const float contentWidth = std::max(2.0f, avail.x - scrollbarWidth);
        const float rowHeight = GetTimelineRowHeight();
        const float rulerHeight = std::min(avail.y, rowHeight * 1.25f);
        const float headerMax = contentWidth * 0.55f;
        const float headerMin = std::min(ImGui::GetFontSize() * 5.0f, headerMax);
        const float header = std::clamp(state.HeaderWidth, headerMin, headerMax);
        const float right = origin.x + contentWidth;
        const float rulerY = origin.y + rulerHeight;
        const float edge = ImGui::GetFontSize() * 0.4f;
        auto scale = MakeTimelineScale(origin.x + header, contentWidth - header, viewDuration, state.Zoom, state.Start);
        auto& input = Engine::InputManager::Get();
        const auto mouse = input.GetScreenMousePosition();
        const bool held = input.IsMouseButtonPressed(Engine::EMouseButton::Left);

        // 1. 상단 고정(Sticky) 머리: 배경과 "트랙" 칸(+ 버튼)을 먼저 그리고 제출한다.
        //    버튼이 머리 입력 영역보다 먼저 제출되어야 클릭을 받고, 배경보다 나중에 그려져야 가려지지 않는다.
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + avail.x, rulerY), TimelineRulerBackground);
        result.AddTrack = DrawTimelineCorner(*draw, origin, ImVec2(scale.Left, rulerY), addTrackTooltipKey);

        // 이름 칸 폭 스플리터: 눈금자부터 행 영역 끝까지 덮는다 (애니메이션 타임라인과 같은 손잡이).
// 머리 입력 영역과 행 자식 창보다 먼저 제출해야 겹친 자리에서 우선 잡힌다.
        const float splitterHitWidth = Editor::SplitterThickness + Editor::SplitterHoverExtend * 2.0f;
        const ImRect splitterRect(ImVec2(scale.Left - splitterHitWidth * 0.5f, origin.y), ImVec2(scale.Left + splitterHitWidth * 0.5f, origin.y + avail.y));
        const auto headerSplitter = Editor::SplitterHandle("##EffectTimelineHeaderSplitter", splitterRect, ImGuiAxis_X);
        if (headerSplitter.Held) state.HeaderWidth = std::clamp(header + input.GetMouseDelta().x, headerMin, headerMax);

        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("##EffectTimelineHeaderCanvas", ImVec2(contentWidth, rulerHeight));
        const bool headerHovered = ImGui::IsItemHovered();
        const bool headerActivated = ImGui::IsItemActivated();
        const bool headerActive = ImGui::IsItemActive();
        const bool headerDeactivated = ImGui::IsItemDeactivated();

        // 2. 하단 트랙 행(Row) 전용 세로 스크롤 영역
        const float rowsViewHeight = std::max(1.0f, avail.y - rulerHeight);
        const float rowsContentHeight = std::max(rowsViewHeight, rowHeight * static_cast<float>(rows.size()));
        ImGui::SetCursorScreenPos(ImVec2(origin.x, rulerY));
        ImGui::SetNextWindowContentSize(ImVec2(contentWidth, rowsContentHeight));

        bool rowsHovered = false;
        bool rowsActivated = false;
        bool rowsActive = false;
        bool rowsDeactivated = false;
        ImVec2 rowsOrigin(origin.x, rulerY);
        ImDrawList* rowsDraw = nullptr;

        if (ImGui::BeginChild("##EffectTimelineRows", ImVec2(avail.x, rowsViewHeight), ImGuiChildFlags_None,
            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            rowsOrigin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##EffectTimelineRowsCanvas", ImVec2(contentWidth, rowsContentHeight));
            rowsHovered = ImGui::IsItemHovered();
            rowsActivated = ImGui::IsItemActivated();
            rowsActive = ImGui::IsItemActive();
            rowsDeactivated = ImGui::IsItemDeactivated();
            rowsDraw = ImGui::GetWindowDrawList();

            if (rowsHovered && !state.Drag)
            {
                const float wheel = input.GetMouseScrollDeltaY();
                if (wheel != 0.0f && (mouse.x < scale.Left || input.IsAltPressed()))
                    ImGui::SetScrollY(ImGui::GetScrollY() - wheel * rowHeight * 3.0f);
            }
        }
        // 행 자식 창은 3단계에서 행을 다 그린 뒤 닫는다. 창이 열려 있어야 그 창의 잘라내기 영역이 행 그리기에 적용된다.

        const bool hovered = headerHovered || rowsHovered;
        const bool itemActivated = headerActivated || rowsActivated;
        const bool itemActive = headerActive || rowsActive;
        const bool itemDeactivated = headerDeactivated || rowsDeactivated;

        if (hovered && !state.Drag)
        {
            if (mouse.x >= scale.Left && !input.IsAltPressed())
            {
                const auto wheel = SplitTimelineWheel(input.GetMouseScrollDeltaY(), input.IsShiftPressed(), input.IsAltPressed());
                float panPixels = wheel.PanPixels;
                const float zoomWheel = wheel.Zoom;

                const bool panning = input.IsMouseButtonDragging(Engine::EMouseButton::Middle) ||
                    input.IsMouseButtonDragging(Engine::EMouseButton::Right);
                if (panning)
                    panPixels += input.GetMouseDelta().x;

                NavigateTimeline(mouse.x, zoomWheel, panPixels, scale, viewDuration, state.Zoom, state.Start);
                scale = MakeTimelineScale(scale.Left, scale.Width, viewDuration, state.Zoom, state.Start);
            }
        }

        const float durationX = scale.ToX(duration);
        const float lastHandleX = std::max(scale.Left, right - 1.0f);
        const float durationHandleX = std::clamp(durationX, scale.Left, lastHandleX);
        const bool overDuration = headerHovered && mouse.x >= scale.Left &&
            durationX >= scale.Left && durationX <= right + 1.0f && std::abs(mouse.x - durationHandleX) <= edge;
        if (hovered && overDuration) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (itemActivated)
        {
            state.Drag.reset();
            if (overDuration)
                state.Drag = TimelineEdit{ TimelineDragMode::Duration, 0, 0.0f, duration };
            else if (headerActivated && mouse.x >= scale.Left)
                state.Drag = TimelineEdit{ TimelineDragMode::Seek, 0, 0.0f, 0.0f };
            else if (rowsActivated && mouse.y >= rulerY)
            {
                const auto index = static_cast<std::size_t>(std::max(0.0f, mouse.y - rowsOrigin.y) / rowHeight);
                if (index < rows.size())
                {
                    const auto& row = rows[index];
                    const float x0 = scale.ToX(row.Start);
                    const float x1 = scale.ToX(row.End);
                    const bool overBar = mouse.x >= scale.Left && mouse.x >= x0 - edge &&
                        mouse.x <= std::max(x0 + edge, x1) + edge;
                    const auto tail = GetParticleTail(row, nullptr);
                    const float tailTop = rowsOrigin.y + (index + 0.5f) * rowHeight +
                        (rowHeight * 0.5f - GetTimelineBarInset()) * 0.25f;
                    const bool overTail = tail && mouse.x >= scale.Left && mouse.x >= scale.ToX(tail->Start) &&
                        mouse.x <= scale.ToX(tail->End) && mouse.y >= tailTop && mouse.y <= rowsOrigin.y +
                        (index + 1.0f) * rowHeight - GetTimelineBarInset();
                    const bool overHandle = std::abs(mouse.x - x0) <= edge || std::abs(mouse.x - x1) <= edge;
                    if (mouse.x < scale.Left || overBar || overTail) result.Selection = row.Id;
                    if (overBar && (!overTail || overHandle))
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

        if (state.Drag && (input.IsKeyButtonDown(Engine::EKeyCode::Escape) || (!itemActive && !itemDeactivated)))
            state.Drag.reset();
        if (state.Drag)
        {
            auto& edit = *state.Drag;
            const float delta = (mouse.x - state.GrabX) * state.DragScale.Span / state.DragScale.Width;
            const float limit = std::max(600.0f, viewDuration);
            const float length = std::max(0.0f, state.OriginalEnd - state.OriginalStart);
            switch (edit.Mode)
            {
            case TimelineDragMode::Seek:
                if (held || itemActivated) result.Seek = std::clamp(scale.ToTime(mouse.x), 0.0f, duration);
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
                if (edit.Mode != TimelineDragMode::Seek &&
                    std::abs(mouse.x - state.GrabX) >= 1.0f &&
                    (edit.Start != state.OriginalStart || edit.End != state.OriginalEnd))
                    result.Commit = edit;
                state.Drag.reset();
            }
        }

        // 3. 하단 스크롤 영역(Rows) 렌더링: 그리드, 트랙 행, 플레이헤드 수직선
        const float playheadX = scale.ToX(result.Seek.value_or(playhead));
        const float rowsBottomY = rulerY + rowsViewHeight;
        if (rowsDraw)
        {
            // ① 행 배경 (그리드가 가려지지 않도록 먼저 그린다)
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                const float y = rowsOrigin.y + i * rowHeight;
                if (y + rowHeight < rulerY || y > rowsBottomY) continue;
                DrawTimelineRowBackground(*rowsDraw, origin.x, scale, y, y + rowHeight, i, rows[i].Selected);
            }

            // ② 눈금자와 같은 간격의 2단계 그리드
            DrawTimelineGrid(*rowsDraw, scale, rulerY, rowsBottomY, EffectTimelineFPS);

            // ③ 이름과 막대
            for (std::size_t i = 0; i < rows.size(); ++i)
            {
                const auto& row = rows[i];
                const float y = rowsOrigin.y + i * rowHeight;
                if (y + rowHeight < rulerY || y > rowsBottomY) continue;

                const auto labelState = row.Selected ? TimelineLabelState::Selected
                    : (row.Disabled ? TimelineLabelState::Disabled : TimelineLabelState::Normal);
                DrawTimelineRowLabel(*rowsDraw, origin.x, scale.Left, y, y + rowHeight, row.Icon, row.Color, row.Label, labelState);

                const auto* edit = state.Drag && state.Drag->Row == row.Id &&
                    state.Drag->Mode != TimelineDragMode::Duration && state.Drag->Mode != TimelineDragMode::Seek ? &*state.Drag : nullptr;
                const float x0 = scale.ToX(edit ? edit->Start : row.Start);
                const float x1 = scale.ToX(edit ? edit->End : row.End);
                const float mid = y + rowHeight * 0.5f;
                rowsDraw->PushClipRect(ImVec2(scale.Left, std::max(y, rulerY)), ImVec2(right, std::min(y + rowHeight, rowsBottomY)), true);
                if (row.Kind == TimelineRowKind::Marker)
                    DrawTimelineMarker(*rowsDraw, ImVec2(x0, mid), row.Color, row.Selected);
                else
                {
                    const float inset = GetTimelineBarInset();
                    const ImVec2 min(x0, y + inset);
                    const ImVec2 max(std::max(x0 + 1.0f, x1), y + rowHeight - inset);
                    DrawTimelineBar(*rowsDraw, min, max, row.Color, row.Selected, !row.ParticleTiming && (row.Selected || edit));

                    if (row.ParticleTiming)
                    {
                        if (const auto tail = GetParticleTail(row, edit))
                        {
                            const float tailStart = scale.ToX(tail->Start);
                            const float tailEnd = scale.ToX(tail->End);
                            const float cutoff = scale.ToX(duration);
                            const float tailTop = mid + (max.y - mid) * 0.25f;
                            const ImVec4 color = ImGui::ColorConvertU32ToFloat4(row.Color);
                            const ImU32 tailColor = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.4f));
                            rowsDraw->AddRectFilled(ImVec2(tailStart, tailTop), ImVec2(tailEnd, max.y), tailColor);
                            if (tail->End > duration)
                            {
                                const float clippedStart = std::max(tailStart, cutoff);
                                rowsDraw->AddRectFilled(ImVec2(clippedStart, tailTop), ImVec2(tailEnd, max.y), IM_COL32(145, 55, 45, 130));
                                const float visibleStart = std::max(clippedStart, scale.Left);
                                const float visibleEnd = std::min(tailEnd, right);
                                const float spacing = std::max(4.0f, ImGui::GetFontSize() * 0.45f);
                                for (float x = visibleStart; x < visibleEnd; x += spacing)
                                    rowsDraw->AddLine(ImVec2(x, tailTop), ImVec2(std::min(x + spacing * 0.5f, visibleEnd), max.y), IM_COL32(255, 215, 160, 180));
                                if (cutoff >= scale.Left && cutoff <= right)
                                    rowsDraw->AddLine(ImVec2(cutoff, y + inset), ImVec2(cutoff, max.y), IM_COL32(255, 150, 100, 255), 2.0f);
                            }
                        }
                        DrawTimelineMarker(*rowsDraw, ImVec2(x0, mid), row.Color, row.Selected);
                        if (row.Kind != TimelineRowKind::OpenEnd || (edit && edit->Mode == TimelineDragMode::End))
                            DrawTimelineMarker(*rowsDraw, ImVec2(x1, mid), row.Color, row.Selected);
                    }

                    // ── [인라인 커브 미니어처 렌더링] ──
                    if (row.Curve && !row.Curve->IsEmpty() && (max.x - min.x) > 12.0f)
                    {
                        const float w = max.x - min.x;
                        constexpr float padY = 2.0f;
                        const float h = std::max(4.0f, (max.y - min.y) - padY * 2.0f);
                        const float baseY = max.y - padY;
                        float minV = 0.0f, maxV = 1.0f;
                        for (const auto& k : row.Curve->keys) { minV = std::min(minV, k.value); maxV = std::max(maxV, k.value); }
                        const float valRange = std::max(maxV - minV, 0.001f);
                        const int numSteps = std::clamp(static_cast<int>(w / 4.0f), 12, 32);
                        ImVec2 points[34];
                        for (int step = 0; step <= numSteps; ++step)
                        {
                            const float u = static_cast<float>(step) / static_cast<float>(numSteps);
                            const float px = min.x + u * w;
                            const float val = row.Curve->Evaluate(u, 0.0f);
                            const float norm = std::clamp((val - minV) / valRange, 0.0f, 1.0f);
                            points[step] = ImVec2(px, baseY - norm * h);
                        }
                        // 1. 곡선 아래 반투명 실루엣 면적 채우기
                        for (int step = 0; step < numSteps; ++step)
                        {
                            rowsDraw->AddQuadFilled(ImVec2(points[step].x, baseY), points[step],
                                points[step + 1], ImVec2(points[step + 1].x, baseY), IM_COL32(255, 255, 255, 30));
                        }
                        // 2. 곡선 외곽선 라인 (선명한 화이트)
                        rowsDraw->AddPolyline(points, numSteps + 1, IM_COL32(255, 255, 255, 220), 0, 1.5f);
                        // 3. 커브 라벨 뱃지 (바 너비가 충분할 때)
                        if (row.CurveLabel && w > 48.0f)
                            rowsDraw->AddText(ImVec2(min.x + 4.0f, min.y + 1.0f), IM_COL32(255, 255, 255, 180), row.CurveLabel);
                    }
                }
                rowsDraw->PopClipRect();
                if (rowsHovered && mouse.y >= std::max(y, rulerY) && mouse.y < std::min(y + rowHeight, rowsBottomY))
                {
                    const auto tail = GetParticleTail(row, edit);
                    const bool overParticle = row.ParticleTiming && (mouse.x < scale.Left ||
                        (mouse.x >= scale.Left && mouse.x >= x0 - edge &&
                         mouse.x <= std::max(x1 + edge, tail ? scale.ToX(tail->End) : x1)));
                    if (overParticle && !state.Drag)
                    {
                        const auto& timing = *row.ParticleTiming;
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(row.Label.data(), row.Label.data() + row.Label.size());
                        if (timing.EventDriven)
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.EventWindow"));
                        else
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.EmissionWindow"));
                        if (row.Kind == TimelineRowKind::OpenEnd)
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.OpenEmitterWindow"));
                        else
                            ImGui::TextUnformatted(LOCAL_FMT("EffectTimeline.EmissionRange", row.Start, row.End).c_str());
                        if (timing.EmissionLimit > 0.0f)
                            ImGui::TextUnformatted(LOCAL_FMT("EffectTimeline.EmissionLimit", timing.EmissionLimit).c_str());
                        ImGui::TextUnformatted(LOCAL_FMT("EffectTimeline.LifetimeRange", timing.LifetimeMin, timing.LifetimeMax).c_str());
                        if (tail)
                        {
                            ImGui::TextUnformatted(LOCAL_FMT("EffectTimeline.TailEstimate", tail->End).c_str());
                            if (tail->End > duration)
                                ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.PreviewTruncated"));
                        }
                        else if (!timing.Enabled)
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.EmitterDisabled"));
                        else if (!timing.Supported)
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.TriggerUnsupported"));
                        else if (timing.EmissionLimit > 0.0f && row.Start >= timing.EmissionLimit)
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.EmissionOutsideLimit"));
                        else if (!std::isfinite(timing.LifetimeMin) || !std::isfinite(timing.LifetimeMax))
                            ImGui::TextUnformatted(LOCAL_CSTR("EffectTimeline.InvalidLifetime"));
                        ImGui::EndTooltip();
                    }
                    else if (mouse.x < scale.Left && !row.ParticleTiming)
                    {
                        if (row.Kind == TimelineRowKind::OpenEnd)
                            ImGui::SetTooltip("%.*s\n%s", static_cast<int>(row.Label.size()), row.Label.data(),
                                LOCAL_CSTR("EffectTimeline.OpenEmission"));
                        else ImGui::SetTooltip("%.*s", static_cast<int>(row.Label.size()), row.Label.data());
                    }
                    if (mouse.x >= scale.Left && row.Kind != TimelineRowKind::Marker &&
                        (std::abs(mouse.x - x0) <= edge || std::abs(mouse.x - x1) <= edge))
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }

            Editor::DrawVerticalSplitterLine(*rowsDraw, headerSplitter, scale.Left, rulerY, rowsBottomY);
            if (playheadX >= scale.Left && playheadX <= right)
            {
                rowsDraw->PushClipRect(ImVec2(scale.Left, rulerY), ImVec2(right, rowsBottomY), true);
                rowsDraw->AddLine(ImVec2(playheadX, rulerY), ImVec2(playheadX, rowsBottomY), TimelinePlayheadColor, 2.0f);
                rowsDraw->PopClipRect();
            }
        }
        ImGui::EndChild();

        // 4. 상단 고정(Sticky) 헤더 및 시간 눈금자 렌더링
        DrawTimelineRuler(*draw, scale, origin.y, rulerY, EffectTimelineFPS);
        draw->AddLine(ImVec2(origin.x, rulerY), ImVec2(origin.x + avail.x, rulerY), IM_COL32(55, 60, 70, 255));
        Editor::DrawVerticalSplitterLine(*draw, headerSplitter, scale.Left, origin.y, rulerY);
        DrawTimelinePlayhead(*draw, ImVec2(scale.Left, origin.y), ImVec2(right, rulerY), playheadX);

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
        const std::function<void()>& inspector, const std::function<void()>& timeline, const std::function<void()>& transport)
    {
        const auto avail = ImGui::GetContentRegionAvail();
        if (avail.x < 2.0f || avail.y < 4.0f) return;
        const auto spacing = ImGui::GetStyle().ItemSpacing;
        const float font = ImGui::GetFontSize();
        const float splitter = Editor::SplitterThickness;
        const float height = std::max(2.0f, avail.y - splitter - spacing.y * 2.0f);
        // 재생 바(약 1행) + 눈금자(1.25행) + 트랙 2행 이상이 보이도록 한다.
        const float minTimeline = std::min(GetTimelineRowHeight() * 5.0f, height * 0.5f);
        const float maxTimeline = std::max(minTimeline, height - font * 5.0f);
        const float timelineSize = std::clamp(timelineHeight, minTimeline, maxTimeline);
        const float topSize = height - timelineSize;

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
            if (const float drag = Editor::SplitterBar("##EffectLeftSplitter", ImGuiAxis_X, topSize); drag != 0.0f)
                listWidth = std::clamp(left + drag, minSide, maxSides - right);
            ImGui::SameLine();
            if (ImGui::BeginChild("##EffectPreview", ImVec2(center, topSize), ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) preview();
            ImGui::EndChild();
            ImGui::SameLine();
            if (const float drag = Editor::SplitterBar("##EffectRightSplitter", ImGuiAxis_X, topSize); drag != 0.0f)
                inspectorWidth = std::clamp(right - drag, minSide, maxSides - left);
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

        if (const float drag = Editor::SplitterBar("##EffectTimelineSplitter", ImGuiAxis_Y, avail.x); drag != 0.0f)
            timelineHeight = std::clamp(timelineSize - drag, minTimeline, maxTimeline);
        // 재생 바와 트랙이 상자 가장자리에 붙도록 안쪽 여백 없이 연다 (애니메이션 타임라인과 같은 배치)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const bool timelineVisible = ImGui::BeginChild("##EffectTimeline", ImVec2(0.0f, timelineSize), ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        if (timelineVisible)
        {
            if (transport) transport();
            timeline();
        }
        ImGui::EndChild();
    }

    
}
