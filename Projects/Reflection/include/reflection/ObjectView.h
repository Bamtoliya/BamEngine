#pragma once

#include <memory>
#include <type_traits>
#include <typeindex>

namespace reflection
{
    struct PropertyInfo;
    struct TypeInfo;

    class ObjectView
    {
    public:
        ObjectView() = default;

        template<typename T>
            requires ((std::is_class_v<T> || std::is_union_v<T>) &&
        !std::is_volatile_v<T>)
            [[nodiscard]]
        static ObjectView From(T& object)
        {
            using Object = std::remove_const_t<T>;

            ObjectView view;
            view.m_type = std::type_index(typeid(Object));
            view.m_readAddress = std::addressof(object);

            if constexpr (!std::is_const_v<T>)
            {
                view.m_writeAddress = std::addressof(object);
            }

            return view;
        }

        // 임시 객체를 가리키는 뷰를 만들지 않습니다.
        template<typename T>
        static ObjectView From(const T&&) = delete;

        [[nodiscard]]
        bool IsValid() const noexcept
        {
            return m_readAddress != nullptr;
        }

        [[nodiscard]]
        bool IsReadOnly() const noexcept
        {
            return IsValid() && m_writeAddress == nullptr;
        }

        [[nodiscard]]
        std::type_index GetCppType() const noexcept
        {
            return m_type;
        }

        template<typename T>
            requires (std::is_class_v<T> || std::is_union_v<T>)
        [[nodiscard]]
        bool Is() const noexcept
        {
            using Object = std::remove_cv_t<T>;
            return IsValid() && m_type == std::type_index(typeid(Object));
        }

    private:
        std::type_index m_type{ typeid(void) };
        const void* m_readAddress = nullptr;
        void* m_writeAddress = nullptr;

        friend struct PropertyInfo;
        friend struct TypeInfo;
    };
}