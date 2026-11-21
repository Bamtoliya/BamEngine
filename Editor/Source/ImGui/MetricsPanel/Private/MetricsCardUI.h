#pragma once

#include "Editor_Includes.h"
#include <algorithm>
#include <string>

namespace Editor::MetricsCardUI
{
    inline float ButtonWidth(const char* text)
    {
        return ImGui::CalcTextSize(text).x + 2.0f * ImGui::GetStyle().FramePadding.x;
    }

    template <typename... Args>
    void Bullet(const char* format, Args... args)
    {
        ImGui::Bullet();
        ImGui::Text(format, args...);
    }

    inline void ShowFullTextOnHover(const char* text)
    {
        if (!ImGui::IsItemHovered() || !ImGui::BeginTooltip())
            return;
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    inline bool Button(const char* id, const char* text)
    {
        const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        if (ButtonWidth(text) <= available)
            return ImGui::Button((std::string(text) + "###" + id).c_str());

        const auto padding = ImGui::GetStyle().FramePadding;
        const float inset = std::min(padding.x, available * 0.2f);
        const float wrapWidth = std::max(1.0f, available - 2.0f * inset);
        const auto textSize = ImGui::CalcTextSize(text, nullptr, false, wrapWidth);
        const float height = textSize.y + 2.0f * padding.y;
        const bool clicked = ImGui::Button((std::string("###") + id).c_str(), ImVec2(available, height));
        if (ImGui::IsItemVisible())
        {
            const auto min = ImGui::GetItemRectMin();
            const auto max = ImGui::GetItemRectMax();
            const ImVec4 clip(min.x, min.y, max.x, max.y);
            const ImVec2 pos(min.x + std::max(inset, (available - textSize.x) * 0.5f), min.y + padding.y);
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos,
                ImGui::GetColorU32(ImGuiCol_Text), text, nullptr, wrapWidth, &clip);
        }
        ShowFullTextOnHover(text);
        return clicked;
    }

    inline void Header(const char* title, ImVec4 titleColor, const char* status, ImVec4 statusColor)
    {
        const float available = ImGui::GetContentRegionAvail().x;
        const float start = ImGui::GetCursorPosX();
        const float titleWidth = ImGui::CalcTextSize(title).x;
        const float statusWidth = ImGui::CalcTextSize(status).x;
        ImGui::TextColored(titleColor, "%s", title);
        if (titleWidth > available)
            ShowFullTextOnHover(title);
        if (titleWidth + ImGui::GetStyle().ItemSpacing.x + statusWidth <= available)
            ImGui::SameLine(start + available - statusWidth);
        ImGui::TextColored(statusColor, "%s", status);
        if (statusWidth > available)
            ShowFullTextOnHover(status);
    }

    template <typename Content>
    bool Draw(const char* id, float width, bool selected, const char* title, ImVec4 titleColor,
        const char* status, ImVec4 statusColor, Content content)
    {
        const ImVec4 background = selected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
        const bool visible = ImGui::BeginChild(id, ImVec2(width, 0.0f),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize);
        bool clicked = false;
        if (visible)
        {
            ImGui::PushTextWrapPos(0.0f);
            Header(title, titleColor, status, statusColor);
            content();
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            clicked = Button("Details", LOCAL_CSTR(selected ? "Metrics.CloseDetail" : "Metrics.ViewDetails"));
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return clicked;
    }
}
