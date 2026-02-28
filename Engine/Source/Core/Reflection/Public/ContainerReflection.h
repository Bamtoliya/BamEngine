#pragma once
#include "ReflectionTypes.h"
#include <map>
#include <unordered_map>

BEGIN(Engine)
template<typename ContainerType, typename ElementType>
struct LinearContainerAccessor
{
	static ContainerAccessor Get()
	{
		ContainerAccessor accessor;

		accessor.GetSize = [](const void* container) -> size_t {
			return static_cast<const ContainerType*>(container)->size();
			};

		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func) {
			ContainerType* cont = static_cast<ContainerType*>(container);
			for (size_t i = 0; i < cont->size(); ++i) {
				func(&(*cont)[i], nullptr);
			}
			};

		accessor.GetElement = [](void* container, size_t index) -> void* {
			ContainerType* cont = static_cast<ContainerType*>(container);
			if (index < cont->size()) return &(*cont)[index];
			return nullptr;
			};

		// 1. [버그 수정] 넘겨받은 기본값(valuePtr)을 활용하여 추가
		accessor.Add = [](void* container, const void* valuePtr) {
			ContainerType* cont = static_cast<ContainerType*>(container);
			if (valuePtr) {
				cont->push_back(*static_cast<const ElementType*>(valuePtr));
			}
			else {
				cont->emplace_back();
			}
			};

		// 2. [버그 수정] Remove 함수 추가 (요소 주소값으로 컨테이너 내부를 검색하여 삭제)
		accessor.Remove = [](void* container, const void* valuePtr) {
			ContainerType* cont = static_cast<ContainerType*>(container);
			for (auto it = cont->begin(); it != cont->end(); ++it) {
				if (&(*it) == valuePtr) {
					cont->erase(it);
					break;
				}
			}
			};

		// 3. [에러 원인 수정] GetElements 람다 구현 추가!
		accessor.GetElements = [](void* container) -> vector<void*> {
			ContainerType* cont = static_cast<ContainerType*>(container);
			vector<void*> elements;
			elements.reserve(cont->size());
			for (size_t i = 0; i < cont->size(); ++i) {
				elements.push_back(const_cast<void*>(static_cast<const void*>(&(*cont)[i])));
			}
			return elements;
			};

		accessor.Resize = [](void* container, size_t newSize) {
			static_cast<ContainerType*>(container)->resize(newSize);
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

		accessor.GetSize = [](const void* container) -> size_t {
			return static_cast<const std::list<ElementType>*>(container)->size();
			};

		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func) {
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			for (auto& element : *lst) {
				func(&element, nullptr);
			}
			};

		// [버그 수정] 넘겨받은 기본값(valuePtr) 적용
		accessor.Add = [](void* container, const void* valuePtr) {
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			if (valuePtr) {
				lst->push_back(*static_cast<const ElementType*>(valuePtr));
			}
			else {
				lst->emplace_back();
			}
			};

		// [버그 수정] Remove 함수 추가
		accessor.Remove = [](void* container, const void* valuePtr) {
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			for (auto it = lst->begin(); it != lst->end(); ++it) {
				if (&(*it) == valuePtr) {
					lst->erase(it);
					break;
				}
			}
			};

		accessor.Resize = [](void* container, size_t size) {
			static_cast<std::list<ElementType>*>(container)->resize(size);
			};

		accessor.GetElement = [](void* container, size_t index) -> void* {
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			if (index < lst->size()) {
				auto it = lst->begin();
				std::advance(it, index);
				return &(*it);
			}
			return nullptr;
			};

		// [에러 원인 수정] GetElements 람다 구현 추가!
		accessor.GetElements = [](void* container) -> vector<void*> {
			std::list<ElementType>* lst = static_cast<std::list<ElementType>*>(container);
			vector<void*> elements;
			elements.reserve(lst->size());
			for (auto& element : *lst) {
				elements.push_back(const_cast<void*>(static_cast<const void*>(&element)));
			}
			return elements;
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

template<typename ContainerType>
struct MapAccessor
{
	using K = typename ContainerType::key_type;
	using V = typename ContainerType::mapped_type;
	static ContainerAccessor Get()
	{
		ContainerAccessor accessor;
		accessor.GetSize = [](const void* container) -> size_t
		{
			const ContainerType* map = static_cast<const ContainerType*>(container);
			return map->size();
		};

		accessor.ForEach = [](void* container, std::function<void(void*, void*)> func)
		{
			ContainerType* map = static_cast<ContainerType*>(container);
			for (auto& pair : *map)
			{
				func(&pair.second, const_cast<void*>(static_cast<const void*>(&pair.first)));
			}
			};

		accessor.AddPair = [](void* container)
		{
			ContainerType* map = static_cast<ContainerType*>(container);
			map->emplace(K(), V());
		};

		accessor.GetValue = [](void* container, const void* keyPtr) -> void*
			{
				ContainerType* map = static_cast<ContainerType*>(container);
				const K* key = static_cast<const K*>(keyPtr);
				auto it = map->find(*key);
				return (it != map->end()) ? &it->second : nullptr;
			};

		return accessor;
	}
};
END