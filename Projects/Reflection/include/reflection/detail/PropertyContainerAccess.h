#pragma once

#include <reflection/ObjectView.h>
#include <reflection/ValueView.h>
#include <reflection/PropertyAccessResult.h>
#include <reflection/EnumValue.h>

#include <cstddef>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace reflection::detail
{
    struct PropertyContainerAccess
    {
        std::type_index ElementType{ typeid(void) };
        bool ReadOnly = false;
        bool ObjectElement = false;

        // 읽기 콜백은 컨테이너 주소를 받습니다.
        std::size_t(*Size)(const void*) = nullptr;
        ValueView(*ReadElement)(const void*, std::size_t) = nullptr;

        // 수정 콜백은 소유 객체 주소를 받습니다.
        void (*WriteElement)(void*, std::size_t, const void*) = nullptr;
        PropertyAccessError(*WriteEnumElement)(void*, std::size_t, const EnumValue&) = nullptr;
        ObjectView(*EditElementObject)(void*, std::size_t) = nullptr;

        PropertyAccessError(*Resize)(void*, std::size_t) = nullptr;
        void (*Clear)(void*) = nullptr;
        PropertyAccessError(*Append)(void*, const void*) = nullptr;
        PropertyAccessError(*AppendEnumElement)(void*, const EnumValue&) = nullptr;
        PropertyAccessError(*Insert)(void*, std::size_t, const void*) = nullptr;
        PropertyAccessError(*InsertEnumElement)(void*, std::size_t, const EnumValue&) = nullptr;
        void (*Erase)(void*, std::size_t) = nullptr;
    };

    template<typename T>
    struct VectorTraits
    {
        static constexpr bool IsVector = false;
    };

    template<typename Element, typename Allocator>
    struct VectorTraits<std::vector<Element, Allocator>>
    {
        static constexpr bool IsVector = true;
        using ElementType = Element;
    };

    template<typename T>
    struct SupportsPropertyCopyConstruction : std::bool_constant<std::is_copy_constructible_v<T>>
    {
    };

    template<typename Element, typename Allocator>
    struct SupportsPropertyCopyConstruction<std::vector<Element, Allocator>>
        : SupportsPropertyCopyConstruction<Element>
    {
    };

    template<typename T>
    struct SupportsPropertyCopyAssignment : std::bool_constant<std::is_assignable_v<T&, const std::remove_cv_t<T>&>>
    {
    };

    template<typename Element, typename Allocator>
    struct SupportsPropertyCopyAssignment<std::vector<Element, Allocator>>
        : std::bool_constant<
        SupportsPropertyCopyConstruction<Element>::value&& SupportsPropertyCopyAssignment<Element>::value>
    {
    };
}