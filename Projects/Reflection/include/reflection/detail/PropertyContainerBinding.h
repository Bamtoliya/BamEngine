#pragma once

#include <reflection/detail/PropertyContainerAccess.h>
#include <reflection/detail/EnumValueConversion.h>

#include <type_traits>
#include <typeindex>
#include <utility>
#include <iterator>

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
        else if constexpr (ArrayTraits<Value>::IsArray)
        {
            using Element = typename ArrayTraits<Value>::ElementType;

            static_assert(!std::is_volatile_v<Element>, "Volatile array elements are not supported");

            access.ElementType = std::type_index{ typeid(Element) };
            access.ReadOnly = std::is_const_v<MemberValue> || std::is_const_v<Element>;
            access.ObjectElement = std::is_class_v<Element> || std::is_union_v<Element>;

            access.Size = +[](const void* value) -> std::size_t
                {
                    return static_cast<const Value*>(value)->size();
                };

            access.ReadElement = +[](const void* value, std::size_t index) -> ValueView
                {
                    const auto* container = static_cast<const Value*>(value);
                    return ValueView::From((*container)[index]);
                };

            if constexpr (!std::is_const_v<MemberValue> && !std::is_const_v<Element>)
            {
                if constexpr (SupportsPropertyCopyAssignment<Element>::value)
                {
                    access.WriteElement = +[](void* object, std::size_t index, const void* value)
                        {
                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            container[index] = *static_cast<const Element*>(value);
                        };
                }

                if constexpr (std::is_class_v<Element> || std::is_union_v<Element>)
                {
                    access.EditElementObject = +[](void* object, std::size_t index) -> ObjectView
                        {
                            auto* owner = static_cast<Owner*>(object);
                            return ObjectView::From((owner->*Member)[index]);
                        };
                }

                if constexpr (std::is_enum_v<Element>)
                {
                    using Underlying = std::underlying_type_t<Element>;

                    if constexpr (!std::is_convertible_v<Element, Underlying>)
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
                                (owner->*Member)[index] = *converted;
                                return PropertyAccessError::None;
                            };
                    }
                }
            }
        }
        else if constexpr (SetTraits<Value>::IsSet)
            {
                using Element = typename SetTraits<Value>::ElementType;

                access.ElementType = std::type_index(typeid(Element));
                access.ReadOnly = std::is_const_v<MemberValue>;
                access.ObjectElement = std::is_class_v<Element> || std::is_union_v<Element>;

                access.Size = +[](const void* value) -> std::size_t
                    {
                        return static_cast<const Value*>(value)->size();
                    };

                access.ReadElement = +[](const void* value, std::size_t index) -> ValueView
                    {
                        const auto* container = static_cast<const Value*>(value);
                        using Difference = typename Value::difference_type;
                        const auto position = std::next(container->begin(), static_cast<Difference>(index));
                        return ValueView::From(*position);
                    };

                access.FindSetElement = +[](const void* value, const void* element) -> ValueView
                    {
                        const auto* container = static_cast<const Value*>(value);
                        const auto position = container->find(*static_cast<const Element*>(element));

                        if (position == container->end())
                        {
                            return {};
                        }

                        return ValueView::From(*position);
                    };

                if constexpr (!std::is_const_v<MemberValue>)
                {
                    access.Clear = +[](void* object)
                        {
                            auto* owner = static_cast<Owner*>(object);
                            (owner->*Member).clear();
                        };

                    access.EraseSetElement = +[](void* object, const void* element) -> PropertyAccessError
                        {
                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            const auto position = container.find(*static_cast<const Element*>(element));

                            if (position == container.end())
                            {
                                return PropertyAccessError::ElementNotFound;
                            }

                            container.erase(position);
                            return PropertyAccessError::None;
                        };

                    if constexpr (SupportsPropertyCopyConstruction<Element>::value)
                    {
                        access.InsertSetElement = +[](void* object, const void* element) -> PropertyAccessError
                            {
                                auto* owner = static_cast<Owner*>(object);
                                auto& container = owner->*Member;
                                const auto& typedElement = *static_cast<const Element*>(element);

                                if (container.size() >= container.max_size())
                                {
                                    if (container.find(typedElement) != container.end())
                                    {
                                        return PropertyAccessError::ElementAlreadyExists;
                                    }

                                    return PropertyAccessError::SizeOutOfRange;
                                }

                                const auto result = container.insert(typedElement);
                                return result.second ? PropertyAccessError::None : PropertyAccessError::ElementAlreadyExists;
                            };
                    }
                }
        }
        else if constexpr (MapTraits<Value>::IsMap)
        {
            using Traits = MapTraits<Value>;
            using Key = typename Traits::KeyType;
            using Mapped = typename Traits::MappedType;

            access.ElementType = std::type_index(typeid(typename Traits::ElementType));
            access.KeyType = std::type_index(typeid(Key));
            access.MappedType = std::type_index(typeid(Mapped));
            access.ReadOnly = std::is_const_v<MemberValue>;
            access.ObjectMapped = std::is_class_v<Mapped> || std::is_union_v<Mapped>;

            access.Size = +[](const void* value) -> std::size_t
                {
                    return static_cast<const Value*>(value)->size();
                };

            access.ReadElement = +[](const void* value, std::size_t index) -> ValueView
                {
                    const auto* container = static_cast<const Value*>(value);
                    using Difference = typename Value::difference_type;
                    const auto position = std::next(container->begin(), static_cast<Difference>(index));
                    return ValueView::From(*position);
                };

            access.ReadMapKey = +[](const void* value, std::size_t index) -> ValueView
                {
                    const auto* container = static_cast<const Value*>(value);
                    using Difference = typename Value::difference_type;
                    const auto position = std::next(container->begin(), static_cast<Difference>(index));
                    return ValueView::From(position->first);
                };

            access.ReadMapValue = +[](const void* value, std::size_t index) -> ValueView
                {
                    const auto* container = static_cast<const Value*>(value);
                    using Difference = typename Value::difference_type;
                    const auto position = std::next(container->begin(), static_cast<Difference>(index));
                    return ValueView::From(position->second);
                };

            access.FindMapValue = +[](const void* value, const void* key) -> ValueView
                {
                    const auto* container = static_cast<const Value*>(value);
                    const auto position = container->find(*static_cast<const Key*>(key));

                    if (position == container->end())
                    {
                        return {};
                    }

                    return ValueView::From(position->second);
                };

            if constexpr (!std::is_const_v<MemberValue>)
            {
                if constexpr ((std::is_class_v<Mapped> || std::is_union_v<Mapped>) &&
                    !std::is_const_v<Mapped> && !std::is_volatile_v<Mapped>)
                {
                    access.EditMapValueObject = +[](void* object, const void* key) -> PropertyObjectResult
                        {
                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            const auto position = container.find(*static_cast<const Key*>(key));

                            if (position == container.end())
                            {
                                return { PropertyAccessError::KeyNotFound, {} };
                            }

                            return { PropertyAccessError::None, ObjectView::From(position->second) };
                        };
                }

                access.Clear = +[](void* object)
                    {
                        auto* owner = static_cast<Owner*>(object);
                        (owner->*Member).clear();
                    };

                access.EraseMapEntry = +[](void* object, const void* key) -> PropertyAccessError
                    {
                        auto* owner = static_cast<Owner*>(object);
                        auto& container = owner->*Member;
                        const auto position = container.find(*static_cast<const Key*>(key));

                        if (position == container.end())
                        {
                            return PropertyAccessError::KeyNotFound;
                        }

                        container.erase(position);
                        return PropertyAccessError::None;
                    };

                if constexpr (SupportsPropertyCopyConstruction<Key>::value &&
                    SupportsPropertyCopyConstruction<Mapped>::value)
                {
                    access.InsertMapEntry = +[](void* object, const void* key, const void* value) -> PropertyAccessError
                        {
                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            const auto& typedKey = *static_cast<const Key*>(key);

                            if (container.size() >= container.max_size())
                            {
                                if (container.find(typedKey) != container.end())
                                {
                                    return PropertyAccessError::KeyAlreadyExists;
                                }

                                return PropertyAccessError::SizeOutOfRange;
                            }

                            const auto result = container.try_emplace(typedKey, *static_cast<const Mapped*>(value));
                            return result.second ? PropertyAccessError::None : PropertyAccessError::KeyAlreadyExists;
                        };
                }

                if constexpr (SupportsPropertyCopyAssignment<Mapped>::value)
                {
                    access.WriteMapValue = +[](void* object, const void* key, const void* value) -> PropertyAccessError
                        {
                            auto* owner = static_cast<Owner*>(object);
                            auto& container = owner->*Member;
                            const auto position = container.find(*static_cast<const Key*>(key));

                            if (position == container.end())
                            {
                                return PropertyAccessError::KeyNotFound;
                            }

                            position->second = *static_cast<const Mapped*>(value);
                            return PropertyAccessError::None;
                        };
                }
            }
        }

        if constexpr (SetTraits<Value>::IsSet || MapTraits<Value>::IsMap)
        {
            access.ForEachElement = +[](const void* value, void* context,
                PropertyContainerAccess::ElementVisitor visitor)
                {
                    const auto* container = static_cast<const Value*>(value);
                    std::size_t index = 0;

                    for (const auto& element : *container)
                    {
                        if (!visitor(context, index, ValueView::From(element)))
                        {
                            break;
                        }

                        ++index;
                    }
                };
        }

        if constexpr (MapTraits<Value>::IsMap)
        {
            access.ForEachMapEntry = +[](const void* value, void* context,
                PropertyContainerAccess::MapEntryVisitor visitor)
                {
                    const auto* container = static_cast<const Value*>(value);
                    std::size_t index = 0;

                    for (const auto& [key, mapped] : *container)
                    {
                        if (!visitor(context, index, ValueView::From(key), ValueView::From(mapped)))
                        {
                            break;
                        }

                        ++index;
                    }
                };
        }

        return access;
    }
}