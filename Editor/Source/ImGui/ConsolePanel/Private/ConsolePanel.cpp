#include "ConsolePanel.h"
#include "InputManager.h"

#pragma region Loop
void ConsolePanel::Draw()
{
    if (!m_Open)
        return;

    const string windowTitle = LOCAL("UI_CONSOLE") + "###ConsolePanel";
    if (ImGui::Begin(windowTitle.c_str(), &m_Open))
    {

        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        DrawOptions();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
        DrawSourceFilter();
        ImGui::SameLine();
        DrawSearch();

        RebuildVisibleRows();
        DrawStatus();

        ImGui::Separator();
        DrawLog();
    }

    ImGui::End();
}

void ConsolePanel::Update(Engine::f32 /*dt*/)
{
    auto batch = Engine::Logger::DrainPendingEntries();
    m_DroppedCount += batch.DroppedCount;
    if (!batch.Entries.empty())
        m_HasNewEntries = true;
    for (auto& entry : batch.Entries)
    {
        ConsoleItem item;
        item.DisplayText = BuildDisplayText(entry);
        item.TimeString = FormatTime(entry.Timestamp);
        // 테이블 목록 행 높이 고정을 위해 줄바꿈/탭을 공백으로 치환 (클리퍼 튕김 원천 차단)
        item.SingleLineMessage = entry.Message;
        for (char& ch : item.SingleLineMessage)
        {
            if (ch == '\n' || ch == '\r' || ch == '\t')
                ch = ' ';
        }
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
    m_VisibleRows.clear();
    m_SelectedRowIndex = -1;

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
        ImGui::SetTooltip("%s", LOCAL_CSTR("Console.ClearHistory"));

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
            ? LOCAL_CSTR("Console.AutoScrollOn")
            : LOCAL_CSTR("Console.AutoScrollOff"));
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.5f);
    DrawLevelFilter();
    ImGui::SameLine();
    ImGui::Checkbox(LOCAL_LABEL("Console.CollapseDuplicates").c_str(), &m_CollapseDuplicates);
}

void ConsolePanel::DrawLevelFilter()
{
    int selectedCount = 0;

    for (bool enabled : m_ShowLevels)
    {
        if (enabled)
            ++selectedCount;
    }

    const auto preview = LOCAL_FMT("Console.Levels", selectedCount, m_ShowLevels.size());

    if (ImGui::BeginCombo("##LogLevels", preview.c_str()))
    {
        if (ImGui::Button(LOCAL_LABEL("EditorCommon.All").c_str()))
            m_ShowLevels.fill(true);

        ImGui::SameLine();

        if (ImGui::Button(LOCAL_LABEL("EditorCommon.None").c_str()))
            m_ShowLevels.fill(false);

        ImGui::Separator();

        for (std::size_t i = 0; i < m_ShowLevels.size(); ++i)
        {
            const auto level = static_cast<Engine::ELogLevel>(i);

            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(level));

            const std::string label = fmt::format("{} {}###Level_{}", LevelIcon(level), LOCAL("Console.Level." + std::string(LevelName(level))), static_cast<int>(level));
            ImGui::Checkbox(label.c_str(), &m_ShowLevels[i]);

            ImGui::PopStyleColor();
        }

        ImGui::EndCombo();
    }
}

void ConsolePanel::DrawSourceFilter()
{
    std::string preview = LOCAL("Console.AllSources");
    if (m_SourceFilter.has_value())
        preview = m_SourceFilter->empty() ? LOCAL("Console.EmptySource") : *m_SourceFilter;

    if (!ImGui::BeginCombo("##LogSources", preview.c_str()))
        return;

    if (ImGui::Selectable(LOCAL_LABEL("Console.AllSources").c_str(), !m_SourceFilter.has_value()))
        m_SourceFilter.reset();

    ImGui::Separator();

    std::set<std::string> sources;
    for (const auto& item : m_Items)
        sources.insert(item.Entry.Source);

    // 해당 주체의 로그가 기록에서 사라져도 현재 선택은 유지합니다.
    if (m_SourceFilter.has_value())
        sources.insert(*m_SourceFilter);

    for (const auto& source : sources)
    {
        const bool selected = m_SourceFilter.has_value() && *m_SourceFilter == source;
        const std::string displayName = source.empty() ? LOCAL("Console.EmptySource") : source;
        const std::string label = displayName + "###SourceOption";

        ImGui::PushID(source.c_str());
        if (ImGui::Selectable(label.c_str(), selected))
            m_SourceFilter = source;
        ImGui::PopID();
    }

    ImGui::EndCombo();
}

void ConsolePanel::DrawSearch()
{
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::InputTextWithHint("##ConsoleSearch", LOCAL_CSTR("EditorCommon.Search"), m_Filter.InputBuf,
        IM_ARRAYSIZE(m_Filter.InputBuf)))
    {
        m_Filter.Build();
    }
}

void ConsolePanel::DrawStatus()
{
    const auto status = LOCAL_FMT("Console.Status", m_VisibleRows.size(), m_Items.size(), m_DroppedCount, m_EvictedCount);

    // 패널이 좁아지면 상태 텍스트는 줄바꿈합니다.
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.c_str());
    ImGui::PopTextWrapPos();
}

std::string ConsolePanel::FormatTime(const std::chrono::system_clock::time_point& timestamp)
{
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(timestamp);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seconds).count();
    const auto time = std::chrono::system_clock::to_time_t(seconds);
    std::tm localTime{};
    char timeText[16] = "??:??:??";
    if (::localtime_s(&localTime, &time) == 0)
        std::strftime(timeText, sizeof(timeText), "%H:%M:%S", &localTime);
    return fmt::format("{}.{:03}", timeText, milliseconds);
}
void ConsolePanel::DrawLevelBadge(Engine::ELogLevel level)
{
    const char* name = LevelName(level);
    const char* icon = LevelIcon(level);
    const ImVec4 color = LevelColor(level);
    const float iconWidth = ImGui::CalcTextSize(icon).x;
    const float textWidth = ImGui::CalcTextSize(name).x;
    const float padX = 6.0f;
    const float padY = 2.0f;
    const float spacing = 4.0f;
    const float totalWidth = padX * 2.0f + iconWidth + spacing + textWidth;
    const float badgeHeight = ImGui::GetTextLineHeight() + padY * 2.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 badgeMin = pos;
    const ImVec2 badgeMax = ImVec2(pos.x + totalWidth, pos.y + badgeHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(badgeMin, badgeMax,
        ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.16f)), 4.0f);
    drawList->AddRect(badgeMin, badgeMax,
        ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.55f)), 4.0f, 0, 1.0f);
    drawList->AddText(ImVec2(badgeMin.x + padX, badgeMin.y + padY), ImGui::GetColorU32(color), icon);
    drawList->AddText(ImVec2(badgeMin.x + padX + iconWidth + spacing, badgeMin.y + padY),
        ImGui::GetColorU32(color), name);
    ImGui::Dummy(ImVec2(totalWidth, badgeHeight));
}
void ConsolePanel::DrawCategoryBadge(std::string_view category)
{
    if (category.empty()) return;
    const char* icon = CategoryIcon(category);
    const float iconWidth = ImGui::CalcTextSize(icon).x;
    const float textWidth = ImGui::CalcTextSize(category.data(), category.data() + category.size()).x;
    const float padX = 6.0f;
    const float padY = 2.0f;
    const float spacing = 4.0f;
    const float totalWidth = padX * 2.0f + iconWidth + spacing + textWidth;
    const float badgeHeight = ImGui::GetTextLineHeight() + padY * 2.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 badgeMin = pos;
    const ImVec2 badgeMax = ImVec2(pos.x + totalWidth, pos.y + badgeHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(badgeMin, badgeMax,
        ImGui::GetColorU32(ImVec4(0.20f, 0.30f, 0.45f, 0.25f)), 4.0f);
    drawList->AddRect(badgeMin, badgeMax,
        ImGui::GetColorU32(ImVec4(0.40f, 0.60f, 0.85f, 0.45f)), 4.0f, 0, 1.0f);
    drawList->AddText(ImVec2(badgeMin.x + padX, badgeMin.y + padY),
        ImGui::GetColorU32(ImVec4(0.65f, 0.80f, 1.0f, 1.0f)), icon);
    drawList->AddText(ImVec2(badgeMin.x + padX + iconWidth + spacing, badgeMin.y + padY),
        ImGui::GetColorU32(ImVec4(0.82f, 0.90f, 1.0f, 1.0f)), category.data(), category.data() + category.size());
    ImGui::Dummy(ImVec2(totalWidth, badgeHeight));
}
void ConsolePanel::DrawRepeatBadge(std::uint32_t count, bool capped)
{
    const std::string text = fmt::format("×{}{}", count, capped ? "+" : "");
    const float textWidth = ImGui::CalcTextSize(text.c_str()).x;
    const float padX = 6.0f;
    const float padY = 2.0f;
    const float totalWidth = padX * 2.0f + textWidth;
    const float badgeHeight = ImGui::GetTextLineHeight() + padY * 2.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 badgeMin = pos;
    const ImVec2 badgeMax = ImVec2(pos.x + totalWidth, pos.y + badgeHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(badgeMin, badgeMax,
        ImGui::GetColorU32(ImVec4(0.95f, 0.50f, 0.10f, 0.28f)), 8.0f);
    drawList->AddRect(badgeMin, badgeMax,
        ImGui::GetColorU32(ImVec4(1.00f, 0.65f, 0.20f, 0.65f)), 8.0f, 0, 1.0f);
    drawList->AddText(ImVec2(badgeMin.x + padX, badgeMin.y + padY),
        ImGui::GetColorU32(ImVec4(1.0f, 0.85f, 0.35f, 1.0f)), text.c_str());
    ImGui::Dummy(ImVec2(totalWidth, badgeHeight));
}

void ConsolePanel::DrawLog()
{
    const float rowHeight = ImGui::GetTextLineHeight() + 8.0f;
    const bool hasSelection = (m_SelectedRowIndex >= 0 && m_SelectedRowIndex < static_cast<int>(m_Items.size()));
    const float splitterHeight = 6.0f;
    const float spacingY = ImGui::GetStyle().ItemSpacing.y;
    const float totalAvailY = ImGui::GetContentRegionAvail().y;
    if (totalAvailY < 1.0f) return;

    const float minTableHeight = (std::max)(60.0f, rowHeight * 2.0f);
    const float minDetailHeight = 60.0f;
    const float detailOverhead = splitterHeight + spacingY * 2.0f;
    const float maxDetailHeight = totalAvailY - minTableHeight - detailOverhead;
    const bool showDetail = hasSelection && maxDetailHeight >= minDetailHeight;

    const float activeDetailHeight = showDetail
        ? std::clamp(m_DetailViewHeight, minDetailHeight, maxDetailHeight) : 0.0f;
    if (showDetail) m_DetailViewHeight = activeDetailHeight;

    const float tableHeight = totalAvailY - activeDetailHeight - (showDetail ? detailOverhead : 0.0f);

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit;
    const ImVec2 tableSize(0.0f, tableHeight);

    if (ImGui::BeginTable("ConsoleLogTable", 4, tableFlags, tableSize))
    {
        const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(LOCAL_CSTR("Console.ColTimeLevel"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn(LOCAL_CSTR("Console.ColCategory"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn(LOCAL_CSTR("Console.ColMessage"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(LOCAL_CSTR("Console.ColSource"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_VisibleRows.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                DrawLogItem(m_VisibleRows[row], rowHeight);
        }

        const bool followNewEntries = m_AutoScroll && m_HasNewEntries && wasAtBottom;
        if (m_ScrollToBottom || followNewEntries)
            ImGui::SetScrollHereY(1.0f);

        m_ScrollToBottom = false;
        m_HasNewEntries = false;
        ImGui::EndTable();
    }

    // ── 하단 로그 상세 보기 창 및 상하 인터랙티브 스플리터 ──
    if (showDetail)
    {
        // 1. 수직 스플리터 바 (드래그로 높이 조절)
        const ImVec2 splitterPos = ImGui::GetCursorScreenPos();
        const float splitterWidth = ImGui::GetContentRegionAvail().x;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.60f, 0.95f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.60f, 0.95f, 0.65f));

        ImGui::Button("##ConsoleDetailSplitter", ImVec2(-1.0f, splitterHeight));

        const bool isSplitterHovered = ImGui::IsItemHovered();
        const bool isSplitterActive = ImGui::IsItemActive();

        if (isSplitterHovered || isSplitterActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        if (isSplitterActive)
        {
            const float deltaY = InputManager::Get().GetMouseDelta().y;
            m_DetailViewHeight = std::clamp(activeDetailHeight - deltaY, minDetailHeight, maxDetailHeight);
        }
        ImGui::PopStyleColor(3);

        // 스플리터 시각 구분선 및 중앙 핸들 렌더링
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 lineColor = ImGui::GetColorU32(isSplitterActive
            ? ImVec4(0.40f, 0.75f, 1.0f, 0.95f)
            : (isSplitterHovered ? ImVec4(0.35f, 0.65f, 0.95f, 0.70f) : ImGui::GetStyleColorVec4(ImGuiCol_Separator)));
        const float midY = splitterPos.y + splitterHeight * 0.5f;
        drawList->AddLine(ImVec2(splitterPos.x, midY), ImVec2(splitterPos.x + splitterWidth, midY), lineColor, 1.2f);

        // 중앙 알약 그립 핸들
        const float handleW = 40.0f;
        const float handleH = 3.0f;
        const ImVec2 handleMin(splitterPos.x + (splitterWidth - handleW) * 0.5f, midY - handleH * 0.5f);
        const ImVec2 handleMax(handleMin.x + handleW, handleMin.y + handleH);
        drawList->AddRectFilled(handleMin, handleMax, lineColor, 2.0f);

        // 2. 상세 뷰 본문
        const auto& selected = m_Items[m_SelectedRowIndex];
        if (ImGui::BeginChild("LogDetailView", ImVec2(0.0f, activeDetailHeight), true, ImGuiWindowFlags_None))
        {
            // 상단 메타 정보 헤더
            DrawLevelBadge(selected.Entry.Level);
            ImGui::SameLine(0.0f, 6.0f);
            if (!selected.Entry.Category.empty())
            {
                DrawCategoryBadge(selected.Entry.Category);
                ImGui::SameLine(0.0f, 6.0f);
            }
            if (!selected.Entry.FileName.empty())
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s | %s:%u", selected.TimeString.c_str(), selected.Entry.FileName.c_str(), selected.Entry.Line);
            }

            // 우측 상단 액션 버튼: [복사], [✕ 닫기]
            const float closeBtnW = 24.0f;
            const float copyBtnW = 56.0f;
            const float rightPad = closeBtnW + copyBtnW + ImGui::GetStyle().ItemSpacing.x * 2.0f;
            ImGui::SameLine(ImGui::GetWindowWidth() - rightPad);

            if (ImGui::SmallButton(ICON_FA_COPY " 복사"))
                ImGui::SetClipboardText(selected.Entry.Message.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", LOCAL_CSTR("Console.CopyHint"));

            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_XMARK "##CloseDetail"))
                m_SelectedRowIndex = -1;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("닫기");

            ImGui::Separator();

            // 메시지 전문 출력 (줄바꿈이 온전히 보존되며 가로/세로 스크롤 가능)
            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(selected.Entry.Level));
            ImGui::TextWrapped("%s", selected.Entry.Message.c_str());
            ImGui::PopStyleColor();

            if (ImGui::IsWindowHovered() && InputManager::Get().IsMouseButtonDown(EMouseButton::Right))
                ImGui::SetClipboardText(selected.Entry.Message.c_str());
        }
        ImGui::EndChild();
    }
}
void ConsolePanel::DrawLogItem(const ConsoleRow& row, float rowHeight)
{
    const auto& item = m_Items[row.ItemIndex];
    const auto& entry = item.Entry;
    const bool isSelected = (m_SelectedRowIndex == static_cast<int>(row.ItemIndex));
    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
    // 컬럼 0: 레벨 캡슐 배지 + 시간
    ImGui::TableNextColumn();
    const ImVec2 cellPos = ImGui::GetCursorScreenPos();
    ImGui::PushID(static_cast<int>(row.ItemIndex));
    if (ImGui::Selectable("##RowSelect", isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, rowHeight)))
    {
        m_SelectedRowIndex = static_cast<int>(row.ItemIndex);
    }
    const bool isHovered = ImGui::IsItemHovered();
    ImGui::PopID();
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(cellPos.x + 4.0f, cellPos.y + 4.0f));
    DrawLevelBadge(entry.Level);
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", item.TimeString.c_str());
    // 컬럼 1: 카테고리 태그 배지
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    DrawCategoryBadge(entry.Category);
    // 컬럼 2: 본문 메시지 (줄바꿈이 공백으로 치환된 1줄 요약본을 출력하여 행 높이 완벽 보장)
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if (m_CollapseDuplicates && (row.RepeatCount > 1 || row.CountCapped))
    {
        DrawRepeatBadge(row.RepeatCount, row.CountCapped);
        ImGui::SameLine(0.0f, 6.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.Level));
    ImGui::TextUnformatted(item.SingleLineMessage.c_str());
    ImGui::PopStyleColor();
    // 컬럼 3: 소스 코드 파일:라인
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if (!entry.FileName.empty())
    {
        const auto source = fmt::format("{}:{}", entry.FileName, entry.Line);
        ImGui::TextDisabled("%s", source.c_str());
    }
    // 마우스 호버 시 툴팁 및 우클릭 복사
    if (isHovered)
    {
        if (InputManager::Get().IsMouseButtonDown(EMouseButton::Right))
            ImGui::SetClipboardText(entry.Message.c_str());
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45.0f);
        ImGui::TextUnformatted(entry.Message.c_str());
        ImGui::Separator();
        const auto source = fmt::format("{}:{}", entry.FileName, entry.Line);
        ImGui::TextUnformatted(source.c_str());
        ImGui::TextUnformatted(LOCAL_CSTR("Console.CopyHint"));
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ConsolePanel::RebuildVisibleRows()
{
    m_VisibleRows.clear();
    m_VisibleRows.reserve(m_Items.size());

    using GroupKey = std::tuple<Engine::ELogLevel, std::string_view, std::string_view,
        std::string_view, std::string_view, std::uint_least32_t>;

    std::map<GroupKey, std::size_t> groups;

    // 최신 항목부터 순회하여 그룹의 대표 행이 마지막 발생 항목이 되도록 합니다.
    for (std::size_t i = m_Items.size(); i-- > 0;)
    {
        const auto& item = m_Items[i];
        const auto& entry = item.Entry;
        const auto levelIndex = static_cast<std::size_t>(entry.Level);

        if (levelIndex >= m_ShowLevels.size() || !m_ShowLevels[levelIndex])
            continue;

        if (m_SourceFilter.has_value() && entry.Source != *m_SourceFilter)
            continue;

        if (!m_Filter.PassFilter(item.DisplayText.c_str()))
            continue;

        if (!m_CollapseDuplicates)
        {
            m_VisibleRows.push_back({ i });
            continue;
        }

        GroupKey key{ entry.Level, entry.Source, entry.Category, entry.Message, entry.FileName, entry.Line };
        const auto [it, inserted] = groups.try_emplace(key, m_VisibleRows.size());

        if (inserted)
        {
            m_VisibleRows.push_back({ i });
            continue;
        }

        auto& row = m_VisibleRows[it->second];
        if (row.RepeatCount < MaxRepeatCount)
            ++row.RepeatCount;
        else
            row.CountCapped = true;
    }

    // 역순으로 쌓았으므로 시간 순서(오래된 것 → 최신)로 되돌립니다.
    std::reverse(m_VisibleRows.begin(), m_VisibleRows.end());
}

std::string ConsolePanel::BuildDisplayText(const Engine::LogEntry& entry)
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

    auto text = fmt::format("{} [{}.{:03}] [{}] [{}] [{} {}] {}",
        LevelIcon(entry.Level), timeText, milliseconds, LevelName(entry.Level), entry.Source,
        CategoryIcon(entry.Category), entry.Category, entry.Message);

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

const char* ConsolePanel::LevelIcon(Engine::ELogLevel level)
{
    switch (level)
    {
    case Engine::ELogLevel::Trace: return ICON_FA_CODE;
    case Engine::ELogLevel::Info:  return ICON_FA_CIRCLE_INFO;
    case Engine::ELogLevel::Warn:  return ICON_FA_TRIANGLE_EXCLAMATION;
    case Engine::ELogLevel::Error: return ICON_FA_CIRCLE_XMARK;
    case Engine::ELogLevel::Fatal: return ICON_FA_SKULL;
    }
    return ICON_FA_CIRCLE_INFO;
}

const char* ConsolePanel::CategoryIcon(std::string_view cat)
{
    if (cat == "Asset") return ICON_FA_FOLDER;
    if (cat == "Render" || cat == "Renderer" || cat == "RHI") return ICON_FA_IMAGE;
    if (cat == "Shader" || cat == "ShaderCompiler") return ICON_FA_FIRE;
    if (cat == "Audio" || cat == "AudioManager" || cat == "AudioSystem" || cat == "AudioClip") return ICON_FA_VOLUME_HIGH;
    if (cat == "Application" || cat == "Window" || cat == "Workspace" || cat == "Document") return ICON_FA_DESKTOP;
    if (cat == "HotReload") return ICON_FA_ARROWS_ROTATE;
    if (cat == "Graph" || cat == "GraphSystem" || cat == "BlueprintLog") return ICON_FA_DIAGRAM_PROJECT;
    if (cat == "Localization") return ICON_FA_LANGUAGE;
    if (cat == "Scene" || cat == "SceneManager") return ICON_FA_CUBES;
    if (cat == "Module" || cat == "ModuleManager") return ICON_FA_PUZZLE_PIECE;
    return ICON_FA_TAG;
}

ImVec4 ConsolePanel::LevelColor(Engine::ELogLevel level)
{
    switch (level)
    {
    case Engine::ELogLevel::Trace:
        return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);

    case Engine::ELogLevel::Info:
        return ImVec4(0.40f, 0.85f, 0.50f, 1.0f); // Notification과 동일한 산뜻한 소프트 그린

    case Engine::ELogLevel::Warn:
        return ImVec4(1.00f, 0.80f, 0.25f, 1.0f); // 황색 경고

    case Engine::ELogLevel::Error:
        return ImVec4(1.00f, 0.40f, 0.40f, 1.0f); // 적색 오류

    case Engine::ELogLevel::Fatal:
        return ImVec4(1.00f, 0.25f, 0.65f, 1.0f); // 치명타 마젠타
    }

    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}