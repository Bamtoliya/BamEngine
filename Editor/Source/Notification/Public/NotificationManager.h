#pragma once

#include "Editor_Includes.h"
#include "Logger.h"

BEGIN(Editor)

class NotificationManager final : public Base
{
    DECLARE_SINGLETON(NotificationManager)

private:
    NotificationManager() = default;
    virtual ~NotificationManager() = default;
    EResult Initialize(void* arg = nullptr);

public:
    void Free() override;

    void AddNotification(
        const std::string& title, const std::string& message, ELogLevel level = ELogLevel::Info,
        f32 duration = 5.0f, std::string_view category = "Notification",
        std::source_location location = std::source_location::current());

    // Update와 DrawNotificationUI는 메인 스레드에서 호출합니다.
    void Update(f32 dt);
    void DrawNotificationUI(f32 bottomOffset = 0.0f);

    // ── 알림 히스토리 (Notification Center) ──
    struct HistoryItem
    {
        uint64 Id = 0;
        std::string Title;
        std::string Message;
        ELogLevel Level = ELogLevel::Info;
        std::string TimeString; // 발생 시각 (예: "19:42:10")
    };

    void ToggleHistoryPopup() { m_ShowHistoryPopup = !m_ShowHistoryPopup; }
    bool IsHistoryPopupOpen() const { return m_ShowHistoryPopup; }
    void DrawHistoryPopup();
    size_t GetHistoryCount() const { return m_History.size(); }
    void ClearHistory() { m_History.clear(); }

private:
    struct Notification
    {
        uint64 Id = 0;
        std::string Title;
        std::string Message;
        ELogLevel Level = ELogLevel::Info;
        f32 RemainingTime = 5.0f;
    };

    static constexpr size_t MaxPendingNotifications = 256;
    static constexpr size_t MaxVisibleNotifications = 5;
    static constexpr size_t MaxHistoryNotifications = 100;

    std::mutex m_QueueMutex;
    std::deque<Notification> m_PendingNotifications;
    std::deque<Notification> m_Notifications;
    std::deque<HistoryItem> m_History;
    bool m_ShowHistoryPopup = false;
    uint64 m_NextId = 1;
};

END