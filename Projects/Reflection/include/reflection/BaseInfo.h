#pragma once

#include <reflection/ObjectView.h>

#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace reflection
{
    class BaseInfo
    {
    public:
        std::string TypeName;
        bool IsVirtual = false;

        [[nodiscard]] std::type_index GetDerivedCppType() const noexcept { return m_derivedType; }
        [[nodiscard]] std::type_index GetBaseCppType() const noexcept { return m_baseType; }
        [[nodiscard]] bool CanCast() const noexcept { return m_read != nullptr && m_write != nullptr; }

        [[nodiscard]] ObjectView AsBase(const ObjectView& object) const
        {
            if (!object.IsValid() || object.GetCppType() != m_derivedType || !CanCast())
            {
                return {};
            }
            return object.IsReadOnly() ? m_read(object.m_readAddress) : m_write(object.m_writeAddress);
        }

        // Public, unambiguous inheritance can use the default C++ conversion.
        template<typename Derived, typename Base>
        [[nodiscard]] static BaseInfo For(std::string name, bool isVirtual = false)
        {
            if constexpr (std::is_convertible_v<Derived*, Base*>)
            {
                return Bind<Derived, Base,
                    [](const Derived& object) -> const Base& { return static_cast<const Base&>(object); },
                    [](Derived& object) -> Base& { return static_cast<Base&>(object); }>(std::move(name), isVirtual);
            }
            else
            {
                return Describe<Derived, Base>(std::move(name), isVirtual);
            }
        }

        // Generated friends supply typed conversions when the base is not public.
        template<typename Derived, typename Base, auto ReadCast, auto WriteCast>
        [[nodiscard]] static BaseInfo Bind(std::string name, bool isVirtual = false)
        {
            static_assert(std::is_same_v<std::invoke_result_t<decltype(ReadCast), const Derived&>, const Base&>);
            static_assert(std::is_same_v<std::invoke_result_t<decltype(WriteCast), Derived&>, Base&>);
            auto base = Describe<Derived, Base>(std::move(name), isVirtual);
            base.m_read = +[](const void* object)
            {
                return ObjectView::From(std::invoke(ReadCast, *static_cast<const Derived*>(object)));
            };
            base.m_write = +[](void* object)
            {
                return ObjectView::From(std::invoke(WriteCast, *static_cast<Derived*>(object)));
            };
            return base;
        }

    private:
        template<typename Derived, typename Base>
        static BaseInfo Describe(std::string name, bool isVirtual)
        {
            static_assert(std::is_base_of_v<Base, Derived> && !std::is_same_v<Base, Derived>);
            static_assert(!std::is_const_v<Derived> && !std::is_volatile_v<Derived> &&
                !std::is_const_v<Base> && !std::is_volatile_v<Base>);
            BaseInfo base;
            base.TypeName = std::move(name);
            base.IsVirtual = isVirtual;
            base.m_derivedType = std::type_index{ typeid(Derived) };
            base.m_baseType = std::type_index{ typeid(Base) };
            return base;
        }

        std::type_index m_derivedType{ typeid(void) };
        std::type_index m_baseType{ typeid(void) };
        ObjectView (*m_read)(const void*) = nullptr;
        ObjectView (*m_write)(void*) = nullptr;
    };
}
