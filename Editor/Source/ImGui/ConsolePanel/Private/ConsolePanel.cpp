#include "ConsolePanel.h"

#pragma region Loop
void ConsolePanel::Draw()
{
    if (!m_Open)
        return;

    if (ImGui::Begin("Console", &m_Open))
    {
        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        DrawOptions();
        ImGui::SameLine();
        DrawSearch();

        RebuildVisibleIndices();
        DrawStatus();

        ImGui::Separator();
        DrawLog();
    }

    ImGui::End();
}

void ConsolePanel::Update(Engine::f32 /*dt*/)
{
    // m_Open 여부와 관계없이 수집합니다.
    auto batch = Engine::Logger::DrainPendingEntries();

    m_DroppedCount += batch.DroppedCount;

    if (!batch.Entries.empty())
        m_HasNewEntries = true;

    for (auto& entry : batch.Entries)
    {
        ConsoleItem item;
        item.DisplayText = BuildDisplayText(entry);
        item.Entry = std::move(entry);

        m_Items.push_back(std::move(item));

        if (m_Items.size() > MAX_CONSOLE_ITEMS)
        {
            m_Items.pop_front();
            ++m_EvictedCount;
        }
    }
}
#pragma endregion

void ConsolePanel::ClearLog()
{
    m_Items.clear();
    m_VisibleIndices.clear();

    m_DroppedCount = 0;
    m_EvictedCount = 0;

    m_HasNewEntries = false;
    m_ScrollToBottom = false;
}

void ConsolePanel::DrawOptions()
{
    const float buttonSize = ImGui::GetFrameHeight();
    const ImVec2 size(buttonSize, buttonSize);

    if (ImGui::Button(ICON_FA_TRASH "##ClearLog", size))
        ClearLog();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Clear console history");

    ImGui::SameLine();

    // 활성화된 토글 버튼은 배경색으로 구분합니다.
    const bool highlighted = m_AutoScroll;

    if (highlighted)
    {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    if (ImGui::Button(ICON_FA_ANGLES_DOWN "##AutoScroll", size))
    {
        m_AutoScroll = !m_AutoScroll;

        if (m_AutoScroll)
            m_ScrollToBottom = true;
    }

    if (highlighted)
        ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "%s",
            m_AutoScroll
            ? "Auto-scroll: ON"
            : "Auto-scroll: OFF");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
    DrawLevelFilter();
}

void ConsolePanel::DrawLevelFilter()
{
    int selectedCount = 0;

    for (bool enabled : m_ShowLevels)
    {
        if (enabled)
            ++selectedCount;
    }

    const auto preview = fmt::format(
        "Levels ({}/{})",
        selectedCount,
        m_ShowLevels.size());

    if (ImGui::BeginCombo("##LogLevels", preview.c_str()))
    {
        if (ImGui::Button("All"))
            m_ShowLevels.fill(true);

        ImGui::SameLine();

        if (ImGui::Button("None"))
            m_ShowLevels.fill(false);

        ImGui::Separator();

        for (std::size_t i = 0; i < m_ShowLevels.size(); ++i)
        {
            const auto level = static_cast<Engine::ELogLevel>(i);

            ImGui::PushStyleColor(
                ImGuiCol_Text, LevelColor(level));

            ImGui::Checkbox(
                LevelName(level), &m_ShowLevels[i]);

            ImGui::PopStyleColor();
        }

        ImGui::EndCombo();
    }
}

void ConsolePanel::DrawSearch()
{
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::InputTextWithHint(
        "##ConsoleSearch",
        "Search...",
        m_Filter.InputBuf,
        IM_ARRAYSIZE(m_Filter.InputBuf)))
    {
        m_Filter.Build();
    }
}

void ConsolePanel::DrawStatus()
{
    const auto status = fmt::format(
        "Visible: {} / {}   Dropped: {}   Expired: {}",
        m_VisibleIndices.size(),
        m_Items.size(),
        m_DroppedCount,
        m_EvictedCount);

    // 패널이 좁아지면 상태 텍스트는 줄바꿈합니다.
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.c_str());
    ImGui::PopTextWrapPos();
}

void ConsolePanel::DrawLog()
{
    if (ImGui::BeginChild(
        "ConsoleLogList",
        ImVec2(0, 0),
        false,
        ImGuiWindowFlags_HorizontalScrollbar))
    {
        const bool wasAtBottom =
            ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

        ImGuiListClipper clipper;
        clipper.Begin(
            static_cast<int>(m_VisibleIndices.size()),
            ImGui::GetTextLineHeightWithSpacing());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart;
                row < clipper.DisplayEnd;
                ++row)
            {
                DrawLogItem(m_Items[m_VisibleIndices[row]]);
            }
        }

        const bool followNewEntries =
            m_AutoScroll && m_HasNewEntries && wasAtBottom;

        if (m_ScrollToBottom || followNewEntries)
            ImGui::SetScrollHereY(1.0f);

        m_ScrollToBottom = false;
        m_HasNewEntries = false;
    }

    ImGui::EndChild();
}

void ConsolePanel::DrawLogItem(const ConsoleItem& item)
{
    ImGui::PushStyleColor(
        ImGuiCol_Text, LevelColor(item.Entry.Level));

    ImGui::TextUnformatted(item.DisplayText.c_str());

    ImGui::PopStyleColor();

    if (!ImGui::IsItemHovered())
        return;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::SetClipboardText(item.Entry.Message.c_str());

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45.0f);

    ImGui::TextUnformatted(item.Entry.Message.c_str());
    ImGui::Separator();

    const auto source = fmt::format(
        "{}:{}",
        item.Entry.FileName,
        item.Entry.Line);

    ImGui::TextUnformatted(source.c_str());
    ImGui::TextUnformatted("Right-click to copy message");

    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}


void ConsolePanel::RebuildVisibleIndices()
{
    m_VisibleIndices.clear();
    m_VisibleIndices.reserve(m_Items.size());

    for (std::size_t i = 0; i < m_Items.size(); ++i)
    {
        const auto& item = m_Items[i];
        const auto levelIndex =
            static_cast<std::size_t>(item.Entry.Level);

        if (levelIndex >= m_ShowLevels.size() ||
            !m_ShowLevels[levelIndex])
        {
            continue;
        }

        if (!m_Filter.PassFilter(item.DisplayText.c_str()))
            continue;

        m_VisibleIndices.push_back(static_cast<int>(i));
    }
}

std::string ConsolePanel::BuildDisplayText(
    const Engine::LogEntry& entry)
{
    const auto seconds =
        std::chrono::time_point_cast<std::chrono::seconds>(
            entry.Timestamp);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.Timestamp - seconds).count();

    const auto time =
        std::chrono::system_clock::to_time_t(seconds);

    std::tm localTime{};
    char timeText[16] = "??:??:??";

    if (::localtime_s(&localTime, &time) == 0)
    {
        std::strftime(
            timeText, sizeof(timeText), "%H:%M:%S", &localTime);
    }

    auto text = fmt::format(
        "[{}.{:03}] [{}] [{}] {}",
        timeText,
        milliseconds,
        LevelName(entry.Level),
        entry.Category,
        entry.Message);

    // 리스트는 고정 높이 한 줄로 표시합니다.
    // 원본 메시지의 줄바꿈은 Entry에 그대로 남아 있습니다.
    for (char& ch : text)
    {
        if (ch == '\n' || ch == '\r' || ch == '\t')
            ch = ' ';
    }

    return text;
}

const char* ConsolePanel::LevelName(Engine::ELogLevel level)
{
    switch (level)
    {
    case Engine::ELogLevel::Trace: return "Trace";
    case Engine::ELogLevel::Info:  return "Info";
    case Engine::ELogLevel::Warn:  return "Warn";
    case Engine::ELogLevel::Error: return "Error";
    case Engine::ELogLevel::Fatal: return "Fatal";
    }

    return "Unknown";
}

ImVec4 ConsolePanel::LevelColor(Engine::ELogLevel level)
{
    switch (level)
    {
    case Engine::ELogLevel::Trace:
        return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);

    case Engine::ELogLevel::Info:
        return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

    case Engine::ELogLevel::Warn:
        return ImVec4(1.00f, 0.80f, 0.25f, 1.0f);

    case Engine::ELogLevel::Error:
        return ImVec4(1.00f, 0.40f, 0.40f, 1.0f);

    case Engine::ELogLevel::Fatal:
        return ImVec4(1.00f, 0.25f, 0.65f, 1.0f);
    }

    return ImVec4(1, 1, 1, 1);
}
