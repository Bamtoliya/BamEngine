#pragma once

#include "reflection/runtime/ContainerAccessor.h"

#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace reflection
{
    template<typename ContainerType, typename ElementType>
    struct LinearContainerAccessor
    {
        static std::size_t GetSize_Impl(const void* container)
        {
            return static_cast<const ContainerType*>(container)->size();
        }

        static void Clear_Impl(void* container)
        {
            static_cast<ContainerType*>(container)->clear();
        }

        static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* user_data)
        {
            auto* cont = static_cast<ContainerType*>(container);
            for (std::size_t index = 0; index < cont->size(); ++index)
            {
                callback(&(*cont)[index], nullptr, user_data);
            }
        }

        static void Add_Impl(void* container, const void* value_ptr)
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (value_ptr)
            {
                cont->push_back(*static_cast<const ElementType*>(value_ptr));
            }
            else
            {
                cont->emplace_back();
            }
        }

        static void Remove_Impl(void* container, const void* value_ptr)
        {
            auto* cont = static_cast<ContainerType*>(container);
            for (auto it = cont->begin(); it != cont->end(); ++it)
            {
                if (&(*it) == value_ptr)
                {
                    cont->erase(it);
                    break;
                }
            }
        }

        static void* GetElement_Impl(void* container, std::size_t index)
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (index < cont->size())
            {
                return &(*cont)[index];
            }
            return nullptr;
        }

        static std::vector<void*> GetElements_Impl(void* container)
        {
            auto* cont = static_cast<ContainerType*>(container);
            std::vector<void*> result;
            result.reserve(cont->size());

            for (std::size_t index = 0; index < cont->size(); ++index)
            {
                result.push_back(const_cast<void*>(static_cast<const void*>(&(*cont)[index])));
            }

            return result;
        }

        static void Resize_Impl(void* container, std::size_t new_size)
        {
            static_cast<ContainerType*>(container)->resize(new_size);
        }

        static constexpr ContainerAccessor Get()
        {
            ContainerAccessor accessor{};
            accessor.GetSize = &GetSize_Impl;
            accessor.Clear = &Clear_Impl;
            accessor.ForEach = &ForEach_Impl;
            accessor.Add = &Add_Impl;
            accessor.Remove = &Remove_Impl;
            accessor.GetElement = &GetElement_Impl;
            accessor.GetElements = &GetElements_Impl;
            accessor.Resize = &Resize_Impl;
            return accessor;
        }
    };

    template<typename ElementType>
    struct ListAccessor
    {
        using ContainerType = std::list<ElementType>;

        static std::size_t GetSize_Impl(const void* container)
        {
            return static_cast<const ContainerType*>(container)->size();
        }

        static void Clear_Impl(void* container)
        {
            static_cast<ContainerType*>(container)->clear();
        }

        static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* user_data)
        {
            auto* list = static_cast<ContainerType*>(container);
            for (auto& element : *list)
            {
                callback(&element, nullptr, user_data);
            }
        }

        static void Add_Impl(void* container, const void* value_ptr)
        {
            auto* list = static_cast<ContainerType*>(container);
            if (value_ptr)
            {
                list->push_back(*static_cast<const ElementType*>(value_ptr));
            }
            else
            {
                list->emplace_back();
            }
        }

        static void Remove_Impl(void* container, const void* value_ptr)
        {
            auto* list = static_cast<ContainerType*>(container);
            for (auto it = list->begin(); it != list->end(); ++it)
            {
                if (&(*it) == value_ptr)
                {
                    list->erase(it);
                    break;
                }
            }
        }

        static void Resize_Impl(void* container, std::size_t size)
        {
            static_cast<ContainerType*>(container)->resize(size);
        }

        static void* GetElement_Impl(void* container, std::size_t index)
        {
            auto* list = static_cast<ContainerType*>(container);
            if (index >= list->size())
            {
                return nullptr;
            }

            auto it = list->begin();
            std::advance(it, static_cast<std::ptrdiff_t>(index));
            return &(*it);
        }

        static std::vector<void*> GetElements_Impl(void* container)
        {
            auto* list = static_cast<ContainerType*>(container);
            std::vector<void*> result;
            result.reserve(list->size());

            for (auto& element : *list)
            {
                result.push_back(const_cast<void*>(static_cast<const void*>(&element)));
            }

            return result;
        }

        static constexpr ContainerAccessor Get()
        {
            ContainerAccessor accessor{};
            accessor.GetSize = &GetSize_Impl;
            accessor.Clear = &Clear_Impl;
            accessor.ForEach = &ForEach_Impl;
            accessor.Add = &Add_Impl;
            accessor.Remove = &Remove_Impl;
            accessor.Resize = &Resize_Impl;
            accessor.GetElement = &GetElement_Impl;
            accessor.GetElements = &GetElements_Impl;
            return accessor;
        }
    };

    template<typename ContainerType, typename ElementType>
    struct SetAccessor
    {
        static std::size_t GetSize_Impl(const void* container)
        {
            return static_cast<const ContainerType*>(container)->size();
        }

        static void Clear_Impl(void* container)
        {
            static_cast<ContainerType*>(container)->clear();
        }

        static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* user_data)
        {
            auto* cont = static_cast<ContainerType*>(container);
            for (auto& element : *cont)
            {
                callback(const_cast<void*>(static_cast<const void*>(&element)), nullptr, user_data);
            }
        }

        static void Add_Impl(void* container, const void* value_ptr)
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (value_ptr)
            {
                cont->insert(*static_cast<const ElementType*>(value_ptr));
            }
            else
            {
                cont->emplace();
            }
        }

        static void Remove_Impl(void* container, const void* value_ptr)
        {
            auto* cont = static_cast<ContainerType*>(container);
            cont->erase(*static_cast<const ElementType*>(value_ptr));
        }

        static std::vector<void*> GetElements_Impl(void* container)
        {
            auto* cont = static_cast<ContainerType*>(container);
            std::vector<void*> result;
            result.reserve(cont->size());

            for (auto& element : *cont)
            {
                result.push_back(const_cast<void*>(static_cast<const void*>(&element)));
            }

            return result;
        }

        static constexpr ContainerAccessor Get()
        {
            ContainerAccessor accessor{};
            accessor.GetSize = &GetSize_Impl;
            accessor.Clear = &Clear_Impl;
            accessor.ForEach = &ForEach_Impl;
            accessor.Add = &Add_Impl;
            accessor.Remove = &Remove_Impl;
            accessor.GetElements = &GetElements_Impl;
            return accessor;
        }
    };

    template<typename ContainerType>
    struct MapAccessor
    {
        using K = typename ContainerType::key_type;
        using V = typename ContainerType::mapped_type;

        static std::size_t GetSize_Impl(const void* container)
        {
            return static_cast<const ContainerType*>(container)->size();
        }

        static void Clear_Impl(void* container)
        {
            static_cast<ContainerType*>(container)->clear();
        }

        static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* user_data)
        {
            auto* map = static_cast<ContainerType*>(container);
            for (auto& pair : *map)
            {
                callback(&pair.second, const_cast<void*>(static_cast<const void*>(&pair.first)), user_data);
            }
        }

        static void AddPair_Impl(void* container)
        {
            auto* map = static_cast<ContainerType*>(container);
            map->emplace(K{}, V{});
        }

        static void Remove_Impl(void* container, const void* key_ptr)
        {
            auto* map = static_cast<ContainerType*>(container);
            map->erase(*static_cast<const K*>(key_ptr));
        }

        static void* GetValue_Impl(void* container, const void* key_ptr)
        {
            auto* map = static_cast<ContainerType*>(container);
            const auto it = map->find(*static_cast<const K*>(key_ptr));
            if (it == map->end())
            {
                return nullptr;
            }
            return &it->second;
        }

        static void* AddAndGetElement_Impl(void* container, const void* key_ptr)
        {
            auto& map = *static_cast<ContainerType*>(container);
            return &map[*static_cast<const K*>(key_ptr)];
        }

        static constexpr ContainerAccessor Get()
        {
            ContainerAccessor accessor{};
            accessor.GetSize = &GetSize_Impl;
            accessor.Clear = &Clear_Impl;
            accessor.ForEach = &ForEach_Impl;
            accessor.AddPair = &AddPair_Impl;
            accessor.Remove = &Remove_Impl;
            accessor.GetValue = &GetValue_Impl;
            accessor.AddAndGetElement = &AddAndGetElement_Impl;
            return accessor;
        }
    };
}