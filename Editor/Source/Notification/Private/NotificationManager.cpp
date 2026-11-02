#include "NotificationManager.h"

IMPLEMENT_SINGLETON(NotificationManager)

#pragma region Constructor&Destructor
EResult NotificationManager::Initialize(void*)
{
    return EResult::Success;
}

void NotificationManager::Free()
{
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_PendingNotifications.clear();
    }

    m_Notifications.clear();
    m_NextId = 1;

    __super::Free();
}
#pragma endregion

void NotificationManager::AddNotification(const std::string& title, const std::string& message, ELogLevel level, f32 duration, std::string_view category, std::source_location location)
{
    // 큐에서 제외되더라도 로그는 남깁니다. 기존 Logger의 최소 레벨 정책을 따릅니다.
    Logger::Log(level, "Editor", category, location, "{}: {}", title, message);

    const f32 lifetime = std::isfinite(duration) && duration > 0.0f ? duration : 5.0f;

    std::lock_guard<std::mutex> lock(m_QueueMutex);

    if (m_PendingNotifications.size() >= MaxPendingNotifications)
        m_PendingNotifications.pop_front();

    m_PendingNotifications.push_back({ 0, title, message, level, lifetime });
}

void NotificationManager::Update(f32 dt)
{
    const f32 elapsed = std::isfinite(dt) && dt > 0.0f ? dt : 0.0f;

    for (auto& notification : m_Notifications)
        notification.RemainingTime -= elapsed;

    std::erase_if(m_Notifications, [](const Notification& notification)
        {
            return notification.RemainingTime <= 0.0f;
        });

    std::deque<Notification> pending;
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        pending.swap(m_PendingNotifications);
    }

    // 현재 시각 문자열 생성 헬퍼
    auto getNowTimeString = []() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &in_time_t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%H:%M:%S");
        return ss.str();
        };

    for (auto& notification : pending)
    {
        notification.Id = m_NextId++;

        // 1. 지나간 알림 히스토리에도 영구 보관 (최대 100개)
        HistoryItem historyItem{};
        historyItem.Id = notification.Id;
        historyItem.Title = notification.Title;
        historyItem.Message = notification.Message;
        historyItem.Level = notification.Level;
        historyItem.TimeString = getNowTimeString();

        if (m_History.size() >= MaxHistoryNotifications)
            m_History.pop_front();
        m_History.push_back(std::move(historyItem));

        // 2. 우측 하단 플로팅 토스트 표시 대기열
        if (m_Notifications.size() >= MaxVisibleNotifications)
            m_Notifications.pop_front();

        m_Notifications.push_back(std::move(notification));
    }
}

void NotificationManager::DrawNotificationUI(f32 bottomOffset)
{
    if (m_Notifications.empty())
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const f32 fontSize = ImGui::GetFontSize();
    const f32 margin = fontSize;
    const f32 gap = fontSize * 0.5f;
    const f32 width = std::max(1.0f, std::min(fontSize * 22.0f, viewport->WorkSize.x - margin * 2.0f));
    f32 bottom = viewport->WorkPos.y + viewport->WorkSize.y - margin - std::max(0.0f, bottomOffset);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    for (auto it = m_Notifications.rbegin(); it != m_Notifications.rend(); ++it)
    {
        auto& notification = *it;
        if (notification.RemainingTime <= 0.0f)
            continue;

        const f32 availableHeight = bottom - viewport->WorkPos.y - margin;
        if (availableHeight <= 0.0f)
            break;

        const bool isError = notification.Level >= ELogLevel::Error;
        const bool isWarning = notification.Level == ELogLevel::Warn;

        ImVec4 color(0.4f, 0.85f, 0.5f, 1.0f);
        if (isError)
            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        else if (isWarning)
            color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);

        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, bottom),
            ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f),
            ImVec2(width, std::min(fontSize * 14.0f, availableHeight)));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        const std::string windowName = "##Notification_" + std::to_string(notification.Id);

        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            if (ImGui::SmallButton("X"))
                notification.RemainingTime = 0.0f;

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", notification.Title.c_str());
            ImGui::PopStyleColor();

            ImGui::Separator();
            ImGui::TextWrapped("%s", notification.Message.c_str());
        }

        bottom -= ImGui::GetWindowHeight() + gap;

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }
}

void NotificationManager::DrawHistoryPopup()
{
    if (!m_ShowHistoryPopup)
        return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float popupWidth = 380.0f;
    const float popupHeight = 320.0f;

    // 우측 하단 푸터 바로 위에 뜨도록 위치 지정
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - popupWidth - 10.0f,
        vp->WorkPos.y + vp->WorkSize.y - popupHeight - 32.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(popupWidth, popupHeight), ImGuiCond_Appearing);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin((ICON_FA_BELL " " + LOCAL("Notification.HistoryTitle")).c_str(), &m_ShowHistoryPopup, flags))
    {
        // 상단: 히스토리 개수 & 모두 지우기 버튼
        ImGui::Text("%s: %d", LOCAL_CSTR("Notification.TotalCount"), static_cast<int>(m_History.size()));
        ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);
        if (ImGui::SmallButton(LOCAL_CSTR("EditorCommon.Clear")))
        {
            ClearHistory();
        }

        ImGui::Separator();

        // 알림 목록 스크롤 영역
        if (ImGui::BeginChild("NotificationHistoryScrollList", ImVec2(0, 0), false))
        {
            if (m_History.empty())
            {
                ImGui::Spacing();
                ImGui::TextDisabled("  %s", LOCAL_CSTR("Notification.NoHistory"));
            }
            else
            {
                // 최신 알림이 위로 오도록 역순 순회
                for (auto it = m_History.rbegin(); it != m_History.rend(); ++it)
                {
                    const auto& item = *it;
                    ImVec4 color(0.4f, 0.85f, 0.5f, 1.0f);
                    const char* icon = ICON_FA_CIRCLE_INFO;

                    if (item.Level >= ELogLevel::Error)
                    {
                        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                        icon = ICON_FA_CIRCLE_XMARK;
                    }
                    else if (item.Level == ELogLevel::Warn)
                    {
                        color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                        icon = ICON_FA_TRIANGLE_EXCLAMATION;
                    }

                    ImGui::PushID(static_cast<int>(item.Id));

                    // 시각 및 레벨 아이콘
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", item.TimeString.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(color, "%s %s", icon, item.Title.c_str());

                    if (!item.Message.empty())
                    {
                        ImGui::Indent(18.0f);
                        ImGui::TextWrapped("%s", item.Message.c_str());
                        ImGui::Unindent(18.0f);
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}