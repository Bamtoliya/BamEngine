#pragma once

#include "Macro.h"
#include "Delegate.h"

#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <memory>
#include <utility>
#include <limits>

BEGIN(Engine)

class EventBus
{
private:
    struct IEventStorage
    {
        virtual ~IEventStorage() = default;
        virtual void DisconnectAll() = 0;
    };

    template<typename TEvent>
    struct EventStorage final : public IEventStorage
    {
        MulticastDelegate<const TEvent&> DelegateInstance;

        void DisconnectAll() override
        {
            DelegateInstance.Clear();
        }
    };

public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    EventBus(EventBus&&) noexcept = default;
    EventBus& operator=(EventBus&&) noexcept = default;

public:
    template<typename TEvent>
    DelegateHandle Subscribe(std::function<void(const TEvent&)> callback)
    {
        if (!callback)
            return 0;

        if (m_CurrentHandle == (std::numeric_limits<DelegateHandle>::max)())
            return 0;

        const auto storage = GetOrCreateStorage<TEvent>();

        return storage->DelegateInstance.AddWithHandle(++m_CurrentHandle, std::move(callback));
    }

    template<typename TEvent>
    void Unsubscribe(DelegateHandle handle)
    {
        if (const auto storage = FindStorage<TEvent>())
            storage->DelegateInstance.Remove(handle);
    }

    template<typename TEvent>
    void Publish(const TEvent& eventData) const
    {
        if (const auto storage = FindStorage<TEvent>())
            storage->DelegateInstance.Broadcast(eventData);
    }

    template<typename TEvent>
    void Clear()
    {
        if (const auto storage = FindStorage<TEvent>())
            storage->DelegateInstance.Clear();
    }

    void Reset()
    {
        decltype(m_Storages) retired;
        retired.swap(m_Storages);

        for (const auto& [type, storage] : retired)
            storage->DisconnectAll();
    }

private:
    template<typename TEvent>
    std::shared_ptr<EventStorage<TEvent>> FindStorage() const
    {
        static_assert(std::is_same_v<TEvent, std::remove_cvref_t<TEvent>>,
            "Event type must not contain const, volatile, or reference qualifiers.");

        static_assert(std::is_object_v<TEvent>, "Event type must be an object type.");

        const auto it = m_Storages.find(std::type_index(typeid(TEvent)));

        if (it == m_Storages.end())
            return nullptr;

        return std::static_pointer_cast<EventStorage<TEvent>>(it->second);
    }

    template<typename TEvent>
    std::shared_ptr<EventStorage<TEvent>> GetOrCreateStorage()
    {
        if (auto storage = FindStorage<TEvent>())
            return storage;

        auto storage = std::make_shared<EventStorage<TEvent>>();

        m_Storages.emplace(std::type_index(typeid(TEvent)), storage);

        return storage;
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<IEventStorage>> m_Storages;

    DelegateHandle m_CurrentHandle = 0;
};

END