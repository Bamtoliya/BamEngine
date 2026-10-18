#pragma once

#include "Macro.h"
#include "Interface/Delegate.h"
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <utility>

BEGIN(Engine)
class EventBus
{
private:
    struct IEventStorage
    {
        virtual ~IEventStorage() = default;
    };

    template<typename TEvent>
    struct EventStorage final : public IEventStorage
    {
        MulticastDelegate<const TEvent&> DelegateInstance;
    };

public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) noexcept = default;
    EventBus& operator=(EventBus&&) noexcept = default;

    // 특정 이벤트(TEvent)에 리스너 등록
    template<typename TEvent>
    DelegateHandle Subscribe(std::function<void(const TEvent&)> callback)
    {
        return GetOrCreateStorage<TEvent>().DelegateInstance.Add(std::move(callback));
    }

    // 구독 해제
    template<typename TEvent>
    void Unsubscribe(DelegateHandle handle)
    {
        const auto it = m_Storages.find(std::type_index(typeid(TEvent)));
        if (it != m_Storages.end())
        {
            static_cast<EventStorage<TEvent>*>(it->second.get())->DelegateInstance.Remove(handle);
        }
    }

    // 이벤트 즉시 발행
    template<typename TEvent>
    void Publish(const TEvent& eventData) const
    {
        const auto it = m_Storages.find(std::type_index(typeid(TEvent)));
        if (it != m_Storages.end())
        {
            static_cast<const EventStorage<TEvent>*>(it->second.get())->DelegateInstance.Broadcast(eventData);
        }
    }

    // 특정 이벤트의 모든 리스너 제거
    template<typename TEvent>
    void Clear()
    {
        const auto it = m_Storages.find(std::type_index(typeid(TEvent)));
        if (it != m_Storages.end())
        {
            static_cast<EventStorage<TEvent>*>(it->second.get())->DelegateInstance.Clear();
        }
    }

    // 모든 이벤트의 리스너 및 저장소 전체 정리 (씬 전환/PIE 종료 시 활용)
    void Reset()
    {
        m_Storages.clear();
    }

private:
    template<typename TEvent>
    EventStorage<TEvent>& GetOrCreateStorage()
    {
        const std::type_index typeIdx(typeid(TEvent));
        auto it = m_Storages.find(typeIdx);
        if (it == m_Storages.end())
        {
            auto storage = std::make_unique<EventStorage<TEvent>>();
            auto* rawPtr = storage.get();
            m_Storages.emplace(typeIdx, std::move(storage));
            return *rawPtr;
        }
        return *static_cast<EventStorage<TEvent>*>(it->second.get());
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IEventStorage>> m_Storages;
};
END