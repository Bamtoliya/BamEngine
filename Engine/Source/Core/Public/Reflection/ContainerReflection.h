#pragma once
#include "ReflectionTypes.h"
#include <map>

BEGIN(Engine)


template<typename ContainerType, typename ElementType>
struct LinearContainerAccessor
{
	static ContainerAccessor Get()
	{
		ContainerAccessor accessor;
		accessor.GetSize = [](const void* container) -> size_t
		{
			const ContainerType* cont = static_cast<const ContainerType*>(container);
			return cont->size();
		};

		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func)
		{
			ContainerType* cont = static_cast<ContainerType*>(container);
			for (size_t i = 0; i < cont->size(); ++i)
			{
				func(&(*cont)[i], nullptr);
			}
		};

		accessor.GetElement = [](void* container, size_t index) -> void*
		{
			ContainerType* cont = static_cast<ContainerType*>(container);
			if (index < cont->size())
			{
				return &(*cont)[index];
			}
			return nullptr;
		};

		accessor.Add = [](void* container)
		{
			ContainerType* cont = static_cast<ContainerType*>(container);
			cont->emplace_back();
		};

		accessor.Resize = [](void* container, size_t newSize)
		{
			ContainerType* cont = static_cast<ContainerType*>(container);
			cont->resize(newSize);
		};

		return accessor;
	}
};

template<typename ElementType>
struct ListAccessor
{
	static ContainerAccessor Get()
	{
		ContainerAccessor accessor;
		accessor.GetSize = [](const void* container) -> size_t
		{
			const std::list<ElementType>* lst = static_cast<const std::list<ElementType>*>(container);
			return lst->size();
		};
		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func)
		{
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			for (auto& element : *lst)
			{
				func(&element, nullptr);
			}
		};
		accessor.Add = [](void* container)
		{
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			lst->emplace_back();
		};
		accessor.Resize = [](void* container, size_t size)
		{
			static_cast<std::list<ElementType>*>(container)->resize(size);
		};

		accessor.GetElement = [](void* container, size_t index) -> void*
		{
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			if (index < lst->size())
			{
				auto it = lst->begin();
				std::advance(it, index);
				return &(*it);
			}
			return nullptr;
			};
		return accessor;
	}
};


template<typename ContainerType, typename ElementType>
struct SetAccessor
{
	static ContainerAccessor Get()
	{
		ContainerAccessor accessor;
		accessor.GetSize = [](const void* container) -> size_t
		{
			const ContainerType* cont = static_cast<const ContainerType*>(container);
			return cont->size();
		};
		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func)
		{
			ContainerType* cont = static_cast<ContainerType*>(container);
			for (auto& element : *cont)
			{
				func(const_cast<void*>(static_cast<const void*>(&element)), nullptr);
			}
		};
		accessor.Add = [](void* container, const void* valuePtr)
		{
			auto* cont = static_cast<ContainerType*>(container);
			const ElementType* value = static_cast<const ElementType*>(valuePtr);
			cont->insert(*value);
		};
		accessor.Remove = [](void* container, const void* valuePtr)
		{
			auto* cont = static_cast<ContainerType*>(container);
			const ElementType* value = static_cast<const ElementType*>(valuePtr);
			cont->erase(*value);
			};

		accessor.GetElements = [](void* container) -> vector<void*>
		{
			ContainerType* cont = static_cast<ContainerType*>(container);
			vector<void*> elements;
			elements.reserve(cont->size());
			for (auto& element : *cont)
			{
				elements.push_back(const_cast<void*>(static_cast<const void*>(&element)));
			}
			return elements;
			};
		return accessor;

	}
};

template<typename K, typename V>
struct MapAccessor
{
	static ContainerAccessor Get()
	{
		ContainerAccessor accessor;
		accessor.GetSize = [](const void* container) -> size_t
		{
			const std::map<K, V>* map = static_cast<const std::map<K, V>*>(container);
			return map->size();
		};

		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func)
		{
			std::map<K, V>* map = static_cast<std::map<K, V>*>(container);
			for (auto& pair : *map)
			{
				func(&pair.second, const_cast<void*>(static_cast<const void*>(&pair.first)));
			}
			};

		accessor.AddPair = [](void* container)
		{
			std::map<K, V>* map = static_cast<std::map<K, V>*>(container);
			map->emplace(K(), V());
		};
	}
};

END