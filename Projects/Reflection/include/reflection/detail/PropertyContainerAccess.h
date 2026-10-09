#pragma once

#include <reflection/ObjectView.h>
#include <reflection/ValueView.h>
#include <reflection/PropertyAccessResult.h>
#include <reflection/EnumValue.h>

#include <cstddef>
#include <type_traits>
#include <typeindex>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <array>

namespace reflection::detail
{
    struct PropertyContainerAccess
    {
        using ElementVisitor = bool (*)(void*, std::size_t, const ValueView&);
        using MapEntryVisitor = bool (*)(void*, std::size_t, const ValueView&, const ValueView&);

        std::type_index ElementType{ typeid(void) };
        std::type_index KeyType{ typeid(void) };
        std::type_index MappedType{ typeid(void) };
        bool ReadOnly = false;
        bool ObjectElement = false;
        bool ObjectMapped = false;

        // 읽기 콜백은 컨테이너 주소를 받습니다.
        std::size_t(*Size)(const void*) = nullptr;
        ValueView(*ReadElement)(const void*, std::size_t) = nullptr;
        ValueView(*FindSetElement)(const void*, const void*) = nullptr;

        ValueView(*ReadMapKey)(const void*, std::size_t) = nullptr;
        ValueView(*ReadMapValue)(const void*, std::size_t) = nullptr;
        ValueView(*FindMapValue)(const void*, const void*) = nullptr;

        void (*ForEachElement)(const void*, void*, ElementVisitor) = nullptr;
        void (*ForEachMapEntry)(const void*, void*, MapEntryVisitor) = nullptr;

        // 수정 콜백은 소유 객체 주소를 받습니다.
        void (*WriteElement)(void*, std::size_t, const void*) = nullptr;
        PropertyAccessError(*WriteMapValue)(void*, const void*, const void*) = nullptr;
        PropertyObjectResult(*EditMapValueObject)(void*, const void*) = nullptr;
        PropertyAccessError(*InsertMapEntry)(void*, const void*, const void*) = nullptr;
        PropertyAccessError(*EraseMapEntry)(void*, const void*) = nullptr;
        PropertyAccessError(*InsertSetElement)(void*, const void*) = nullptr;
        PropertyAccessError(*EraseSetElement)(void*, const void*) = nullptr;
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
    struct ArrayTraits
    {
        static constexpr bool IsArray = false;
    };

    template<typename Element, std::size_t N>
    struct ArrayTraits<std::array<Element, N>>
    {
        static constexpr bool IsArray = true;
        using ElementType = Element;
    };

    template<typename T>
    struct SetTraits
    {
        static constexpr bool IsSet = false;
    };

    template<typename Element, typename Compare, typename Allocator>
    struct SetTraits<std::set<Element, Compare, Allocator>>
    {
        static constexpr bool IsSet = true;
        static constexpr bool IsUnordered = false;
        using ElementType = Element;
    };

    template<typename Element, typename Hash, typename Equal, typename Allocator>
    struct SetTraits<std::unordered_set<Element, Hash, Equal, Allocator>>
    {
        static constexpr bool IsSet = true;
        static constexpr bool IsUnordered = true;
        using ElementType = Element;
    };

    template<typename T>
    struct MapTraits
    {
        static constexpr bool IsMap = false;
    };

    template<typename Key, typename Mapped, typename Compare, typename Allocator>
    struct MapTraits<std::map<Key, Mapped, Compare, Allocator>>
    {
        static constexpr bool IsMap = true;
        static constexpr bool IsUnordered = false;
        using KeyType = Key;
        using MappedType = Mapped;
        using ElementType = std::pair<const Key, Mapped>;
    };

    template<typename Key, typename Mapped, typename Hash, typename Equal, typename Allocator>
    struct MapTraits<std::unordered_map<Key, Mapped, Hash, Equal, Allocator>>
    {
        static constexpr bool IsMap = true;
        static constexpr bool IsUnordered = true;
        using KeyType = Key;
        using MappedType = Mapped;
        using ElementType = std::pair<const Key, Mapped>;
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

    template<typename Element, std::size_t N>
    struct SupportsPropertyCopyConstruction<std::array<Element, N>>
        : std::bool_constant<N == 0 || SupportsPropertyCopyConstruction<Element>::value>
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

    template<typename Element, std::size_t N>
    struct SupportsPropertyCopyAssignment<std::array<Element, N>>
        : std::bool_constant<N == 0 || SupportsPropertyCopyAssignment<Element>::value>
    {
    };

    template<typename Element, typename Compare, typename Allocator>
    struct SupportsPropertyCopyConstruction<std::set<Element, Compare, Allocator>>
        : SupportsPropertyCopyConstruction<Element>
    {
    };

    template<typename Element, typename Hash, typename Equal, typename Allocator>
    struct SupportsPropertyCopyConstruction<std::unordered_set<Element, Hash, Equal, Allocator>>
        : SupportsPropertyCopyConstruction<Element>
    {
    };

    template<typename Element, typename Compare, typename Allocator>
    struct SupportsPropertyCopyAssignment<std::set<Element, Compare, Allocator>>
        : SupportsPropertyCopyConstruction<Element>
    {
    };

    template<typename Element, typename Hash, typename Equal, typename Allocator>
    struct SupportsPropertyCopyAssignment<std::unordered_set<Element, Hash, Equal, Allocator>>
        : SupportsPropertyCopyConstruction<Element>
    {
    };

    template<typename Key, typename Mapped, typename Compare, typename Allocator>
    struct SupportsPropertyCopyConstruction<std::map<Key, Mapped, Compare, Allocator>>
        : std::bool_constant<SupportsPropertyCopyConstruction<Key>::value&&
        SupportsPropertyCopyConstruction<Mapped>::value>
    {
    };

    template<typename Key, typename Mapped, typename Hash, typename Equal, typename Allocator>
    struct SupportsPropertyCopyConstruction<std::unordered_map<Key, Mapped, Hash, Equal, Allocator>>
        : std::bool_constant<SupportsPropertyCopyConstruction<Key>::value&&
        SupportsPropertyCopyConstruction<Mapped>::value>
    {
    };

    template<typename Key, typename Mapped, typename Compare, typename Allocator>
    struct SupportsPropertyCopyAssignment<std::map<Key, Mapped, Compare, Allocator>>
        : SupportsPropertyCopyConstruction<std::map<Key, Mapped, Compare, Allocator>>
    {
    };

    template<typename Key, typename Mapped, typename Hash, typename Equal, typename Allocator>
    struct SupportsPropertyCopyAssignment<std::unordered_map<Key, Mapped, Hash, Equal, Allocator>>
        : SupportsPropertyCopyConstruction<std::unordered_map<Key, Mapped, Hash, Equal, Allocator>>
    {
    };
}