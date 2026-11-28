#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <limits>

using namespace std;

namespace Engine
{
    using DelegateHandle = std::uint64_t;

    template<typename RetVal, typename... ParamTypes>
    class Delegate
    {
    public:
        using FunctionType = function<RetVal(ParamTypes...)>;

    public:
        Delegate() = default;

        Delegate(const Delegate& other)
        {
            if (other.m_Function)
                m_Function = std::make_shared<FunctionType>(*other.m_Function);
        }

        Delegate(Delegate&&) noexcept = default;

        Delegate& operator=(Delegate other) noexcept
        {
            m_Function.swap(other.m_Function);
            return *this;
        }

    public:
        void Bind(FunctionType func)
        {
            m_Function = func ? std::make_shared<FunctionType>(std::move(func)) : nullptr;
        }

        void Unbind()
        {
            m_Function.reset();
        }

        RetVal Execute(ParamTypes... args) const
        {
            const auto function = m_Function;

            if (!function || !*function)
                throw runtime_error("No function bound to delegate.");

            return (*function)(args...);
        }

        void ExecuteIfBound(ParamTypes... args) const
        {
            const auto function = m_Function;

            if (function && *function)
                (*function)(args...);
        }

        bool IsBound() const
        {
            return m_Function && static_cast<bool>(*m_Function);
        }

    private:
        std::shared_ptr<FunctionType> m_Function;
    };

    template<typename... ParamTypes>
    class MulticastDelegate
    {
        friend class EventBus;

    public:
        using FunctionType = function<void(ParamTypes...)>;

    private:
        struct Listener
        {
            DelegateHandle Handle = 0;
            std::shared_ptr<FunctionType> Function;
            bool Connected = true;
        };

    public:
        MulticastDelegate() = default;

        ~MulticastDelegate()
        {
            Clear();
        }

        MulticastDelegate(const MulticastDelegate& other)
            : m_CurrentHandle(other.m_CurrentHandle)
        {
            m_Listeners.reserve(other.m_Listeners.size());

            for (const auto& listener : other.m_Listeners)
                m_Listeners.push_back(std::make_shared<Listener>(*listener));
        }

        MulticastDelegate(MulticastDelegate&& other) noexcept
            : m_CurrentHandle(other.m_CurrentHandle)
        {
            m_Listeners.swap(other.m_Listeners);
        }

        MulticastDelegate& operator=(MulticastDelegate other) noexcept
        {
            m_Listeners.swap(other.m_Listeners);
            std::swap(m_CurrentHandle, other.m_CurrentHandle);

            other.Clear();

            return *this;
        }

    public:
        DelegateHandle Add(FunctionType func)
        {
            if (!func)
                return 0;

            if (m_CurrentHandle == (std::numeric_limits<DelegateHandle>::max)())
                return 0;

            return AddWithHandle(m_CurrentHandle + 1, std::move(func));
        }

        DelegateHandle AddLambda(FunctionType func)
        {
            return Add(std::move(func));
        }

        void Remove(DelegateHandle handle)
        {
            if (handle == 0)
                return;

            const auto it = std::find_if(m_Listeners.begin(), m_Listeners.end(),
                [handle](const auto& listener)
                {
                    return listener->Handle == handle;
                });

            if (it == m_Listeners.end())
                return;

            // 목록에서 제거하는 동안 callable의 수명을 유지한다.
            const auto retired = *it;

            retired->Connected = false;
            m_Listeners.erase(it);
        }

        void Clear()
        {
            vector<std::shared_ptr<Listener>> retired;
            retired.swap(m_Listeners);

            for (const auto& listener : retired)
                listener->Connected = false;
        }

        void Broadcast(ParamTypes... args) const
        {
            const auto snapshot = m_Listeners;

            for (const auto& listener : snapshot)
            {
                if (listener->Connected && listener->Function && *listener->Function)
                    (*listener->Function)(args...);
            }
        }

        bool IsBound() const
        {
            return !m_Listeners.empty();
        }

        size_t GetListenerCount() const
        {
            return m_Listeners.size();
        }

    private:
        DelegateHandle AddWithHandle(DelegateHandle handle, FunctionType func)
        {
            auto listener = std::make_shared<Listener>();

            listener->Handle = handle;
            listener->Function = std::make_shared<FunctionType>(std::move(func));
            listener->Connected = true;

            m_Listeners.push_back(std::move(listener));
            m_CurrentHandle = handle;

            return handle;
        }

    private:
        vector<std::shared_ptr<Listener>> m_Listeners;
        DelegateHandle m_CurrentHandle = 0;
    };

}