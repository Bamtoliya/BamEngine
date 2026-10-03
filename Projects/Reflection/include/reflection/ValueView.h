#pragma once

#include <memory>
#include <type_traits>
#include <typeindex>

namespace reflection
{
    struct PropertyInfo;
    struct TypeInfo;

    class ValueView
    {
    public:
        ValueView() = default;

        template<typename T>
            requires (std::is_object_v<T> && !std::is_volatile_v<T>)
        [[nodiscard]]
        static ValueView From(T& value)
        {
            using Value = std::remove_const_t<T>;

            ValueView view;
            view.m_type = std::type_index(typeid(Value));
            view.m_address = std::addressof(value);
            return view;
        }

        template<typename T>
        static ValueView From(const T&&) = delete;

        [[nodiscard]]
        bool IsValid() const noexcept
        {
            return m_address != nullptr;
        }

        [[nodiscard]]
        std::type_index GetCppType() const noexcept
        {
            return m_type;
        }

        template<typename T>
            requires (std::is_object_v<T> && !std::is_volatile_v<T>)
        [[nodiscard]]
        bool Is() const noexcept
        {
            using Value = std::remove_const_t<T>;
            return IsValid() && m_type == std::type_index(typeid(Value));
        }

        template<typename T>
            requires (std::is_object_v<T> && !std::is_volatile_v<T>)
        [[nodiscard]]
        const T* Get() const noexcept
        {
            return Is<T>() ? static_cast<const T*>(m_address) : nullptr;
        }

    private:
        std::type_index m_type{ typeid(void) };
        const void* m_address = nullptr;

        friend struct PropertyInfo;
        friend struct TypeInfo;
    };
}