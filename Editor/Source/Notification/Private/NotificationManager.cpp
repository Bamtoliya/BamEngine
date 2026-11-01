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

    for (auto& notification : pending)
    {
        notification.Id = m_NextId++;

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