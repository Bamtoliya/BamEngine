#pragma once

#include <reflection/detail/PropertyContainerAccess.h>
#include <reflection/detail/EnumValueConversion.h>

#include <type_traits>
#include <typeindex>
#include <utility>

namespace reflection::detail
{
    template<auto Member, typename Owner>
    PropertyContainerAccess MakePropertyContainerAccess()
    {
        using MemberValue = std::remove_reference_t<decltype(std::declval<Owner&>().*Member)>;
        using Value = std::remove_cv_t<MemberValue>;

        static_assert(!std::is_volatile_v<MemberValue>, "Volatile properties are not supported");

        PropertyContainerAccess access;

        if constexpr (VectorTraits<Value>::IsVector)
        {
            using Element = typename VectorTraits<Value>::ElementType;

            access.ElementType = std::type_index(typeid(Element));
            access.ReadOnly = std::is_const_v<MemberValue>;
            access.ObjectElement = std::is_class_v<Element> || std::is_union_v<Element>;

            access.Size = +[](const void* value) -> std::size_t
                {
                    const auto* container = static_cast<const Value*>(value);
                    return container->size();
                };

            if constexpr ((std::is_class_v<Element> || std::is_union_v<Element>) &&
                !std::is_const_v<MemberValue>)
            {
                access.EditElementObject = +[](void* object, std::size_t index) -> ObjectView
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;
                        return ObjectView::From(container[index]);
                    };
            }

            if constexpr (!std::is_const_v<MemberValue> &&
                std::is_default_constructible_v<Element> && std::is_move_constructible_v<Element>)
            {
                access.Resize = +[](void* object, std::size_t size) -> PropertyAccessError
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;

                        if (size > container.max_size())
                        {
                            return PropertyAccessError::SizeOutOfRange;
                        }

                        container.resize(size);
                        return PropertyAccessError::None;
                    };
            }

            if constexpr (!std::is_const_v<MemberValue>)
            {
                access.Clear = +[](void* object)
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;
                        container.clear();
                    };
            }

            if constexpr (!std::is_const_v<MemberValue> &&
                SupportsPropertyCopyConstruction<Element>::value && std::is_move_constructible_v<Element>)
            {
                access.Append = +[](void* object, const void* value) -> PropertyAccessError
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;

                        if (container.size() >= container.max_size())
                        {
                            return PropertyAccessError::SizeOutOfRange;
                        }

                        container.push_back(*static_cast<const Element*>(value));
                        return PropertyAccessError::None;
                    };
            }

            if constexpr (!std::is_const_v<MemberValue> &&
                SupportsPropertyCopyConstruction<Element>::value &&
                SupportsPropertyCopyAssignment<Element>::value &&
                std::is_move_constructible_v<Element> && std::is_move_assignable_v<Element>)
            {
                access.Insert = +[](void* object, std::size_t index, const void* value) -> PropertyAccessError
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;

                        if (container.size() >= container.max_size())
                        {
                            return PropertyAccessError::SizeOutOfRange;
                        }

                        // 같은 벡터의 요소를 입력으로 받아도 이동 전에 값을 보관합니다.
                        const Element insertedValue = *static_cast<const Element*>(value);

                        using Difference = typename Value::difference_type;
                        const auto position = container.begin() + static_cast<Difference>(index);

                        container.insert(position, insertedValue);
                        return PropertyAccessError::None;
                    };
            }

            if constexpr (!std::is_const_v<MemberValue> && std::is_move_assignable_v<Element>)
            {
                access.Erase = +[](void* object, std::size_t index)
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;

                        using Difference = typename Value::difference_type;
                        const auto position = container.begin() + static_cast<Difference>(index);

                        container.erase(position);
                    };
            }

            if constexpr (std::is_enum_v<Element>)
            {
                using Underlying = std::underlying_type_t<Element>;

                if constexpr (!std::is_const_v<MemberValue> && !std::is_convertible_v<Element, Underlying>)
                {
                    access.WriteEnumElement =
                        +[](void* object, std::size_t index, const EnumValue& value) -> PropertyAccessError
                        {
                            const auto converted = ConvertEnumValue<Element>(value);
                            if (!converted)
                            {
                                return PropertyAccessError::ValueOutOfRange;
                            }

                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            container[index] = *converted;
                            return PropertyAccessError::None;
                        };

                    access.AppendEnumElement =
                        +[](void* object, const EnumValue& value) -> PropertyAccessError
                        {
                            const auto converted = ConvertEnumValue<Element>(value);
                            if (!converted)
                            {
                                return PropertyAccessError::ValueOutOfRange;
                            }

                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;

                            if (container.size() >= container.max_size())
                            {
                                return PropertyAccessError::SizeOutOfRange;
                            }

                            container.push_back(*converted);
                            return PropertyAccessError::None;
                        };

                    access.InsertEnumElement =
                        +[](void* object, std::size_t index, const EnumValue& value) -> PropertyAccessError
                        {
                            const auto converted = ConvertEnumValue<Element>(value);
                            if (!converted)
                            {
                                return PropertyAccessError::ValueOutOfRange;
                            }

                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;

                            if (container.size() >= container.max_size())
                            {
                                return PropertyAccessError::SizeOutOfRange;
                            }

                            using Difference = typename Value::difference_type;
                            const auto position = container.begin() + static_cast<Difference>(index);

                            container.insert(position, *converted);
                            return PropertyAccessError::None;
                        };
                }
            }

            if constexpr (!std::is_same_v<Element, bool>)
            {
                access.ReadElement = +[](const void* value, std::size_t index) -> ValueView
                    {
                        const auto* container = static_cast<const Value*>(value);
                        return ValueView::From((*container)[index]);
                    };

                if constexpr (!std::is_const_v<MemberValue> && SupportsPropertyCopyAssignment<Element>::value)
                {
                    access.WriteElement = +[](void* object, std::size_t index, const void* value)
                        {
                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            container[index] = *static_cast<const Element*>(value);
                        };
                }
            }
        }

        return access;
    }
}