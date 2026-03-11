#pragma once
#include "Engine_Includes.h"

BEGIN(Engine)

struct ContainerAccessor
{
	size_t(*GetSize)(const void* container) = nullptr;
	void(*Clear)(void* container) = nullptr;
	void(*ForEach)(void* container, void (*callback)(void* element, void* key, void* userData), void* userData) = nullptr;
	
	void (*Add)(void* container, const void* valuePtr) = nullptr;
	void (*Remove)(void* container, const void* valuePtr) = nullptr;
	void* (*GetElement)(void* container, size_t index) = nullptr;

	std::vector<void*>(*GetElements)(void* container) = nullptr;
	void (*Resize)(void* container, size_t size) = nullptr;

	void (*AddPair)(void* container) = nullptr;
	void* (*GetValue)(void* container, const void* keyPtr) = nullptr;
};

template<typename ContainerType, typename ElementType>
struct LinearContainerAccessor
{
	static size_t GetSize_Impl(const void* container) {
		return static_cast<const ContainerType*>(container)->size();
	}
	static void Clear_Impl(void* container) {
		static_cast<ContainerType*>(container)->clear();
	}
	static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* userData) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		for (size_t i = 0; i < cont->size(); ++i) {
			callback(&(*cont)[i], nullptr, userData);
		}
	}
	static void* GetElement_Impl(void* container, size_t index) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		if (index < cont->size()) return &(*cont)[index];
		return nullptr;
	}
	static void Add_Impl(void* container, const void* valuePtr) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		if (valuePtr) cont->push_back(*static_cast<const ElementType*>(valuePtr));
		else cont->emplace_back();
	}
	static void Remove_Impl(void* container, const void* valuePtr) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		for (auto it = cont->begin(); it != cont->end(); ++it) {
			if (&(*it) == valuePtr) {
				cont->erase(it);
				break;
			}
		}
	}
	static std::vector<void*> GetElements_Impl(void* container) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		std::vector<void*> elements;
		elements.reserve(cont->size());
		for (size_t i = 0; i < cont->size(); ++i) {
			elements.push_back(const_cast<void*>(static_cast<const void*>(&(*cont)[i])));
		}
		return elements;
	}
	static void Resize_Impl(void* container, size_t newSize) {
		static_cast<ContainerType*>(container)->resize(newSize);
	}

	static constexpr ContainerAccessor Get()
	{
		ContainerAccessor accessor{};
		accessor.GetSize = &GetSize_Impl;
		accessor.Clear = &Clear_Impl;
		accessor.ForEach = &ForEach_Impl;
		accessor.GetElement = &GetElement_Impl;
		accessor.Add = &Add_Impl;
		accessor.Remove = &Remove_Impl;
		accessor.GetElements = &GetElements_Impl;
		accessor.Resize = &Resize_Impl;
		return accessor;
	}
};

template<typename ElementType>
struct ListAccessor
{
	using ContainerType = std::list<ElementType>;

	static size_t GetSize_Impl(const void* container) { return static_cast<const ContainerType*>(container)->size(); }
	static void Clear_Impl(void* container) { static_cast<ContainerType*>(container)->clear(); }
	static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* userData) {
		ContainerType* lst = static_cast<ContainerType*>(container);
		for (auto& element : *lst) { callback(&element, nullptr, userData); }
	}
	static void Add_Impl(void* container, const void* valuePtr) {
		ContainerType* lst = static_cast<ContainerType*>(container);
		if (valuePtr) lst->push_back(*static_cast<const ElementType*>(valuePtr));
		else lst->emplace_back();
	}
	static void Remove_Impl(void* container, const void* valuePtr) {
		ContainerType* lst = static_cast<ContainerType*>(container);
		for (auto it = lst->begin(); it != lst->end(); ++it) {
			if (&(*it) == valuePtr) { lst->erase(it); break; }
		}
	}
	static void Resize_Impl(void* container, size_t size) {
		static_cast<ContainerType*>(container)->resize(size);
	}
	static void* GetElement_Impl(void* container, size_t index) {
		ContainerType* lst = static_cast<ContainerType*>(container);
		if (index < lst->size()) {
			auto it = lst->begin();
			std::advance(it, index);
			return &(*it);
		}
		return nullptr;
	}
	static std::vector<void*> GetElements_Impl(void* container) {
		ContainerType* lst = static_cast<ContainerType*>(container);
		std::vector<void*> elements;
		elements.reserve(lst->size());
		for (auto& element : *lst) {
			elements.push_back(const_cast<void*>(static_cast<const void*>(&element)));
		}
		return elements;
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
	static size_t GetSize_Impl(const void* container) { return static_cast<const ContainerType*>(container)->size(); }
	static void Clear_Impl(void* container) { static_cast<ContainerType*>(container)->clear(); }
	static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* userData) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		for (auto& element : *cont) {
			callback(const_cast<void*>(static_cast<const void*>(&element)), nullptr, userData);
		}
	}
	static void Add_Impl(void* container, const void* valuePtr) {
		auto* cont = static_cast<ContainerType*>(container);
		if (valuePtr) cont->insert(*static_cast<const ElementType*>(valuePtr));
		else cont->emplace();
	}
	static void Remove_Impl(void* container, const void* valuePtr) {
		auto* cont = static_cast<ContainerType*>(container);
		cont->erase(*static_cast<const ElementType*>(valuePtr));
	}
	static std::vector<void*> GetElements_Impl(void* container) {
		ContainerType* cont = static_cast<ContainerType*>(container);
		std::vector<void*> elements;
		elements.reserve(cont->size());
		for (auto& element : *cont) {
			elements.push_back(const_cast<void*>(static_cast<const void*>(&element)));
		}
		return elements;
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

	static size_t GetSize_Impl(const void* container) { return static_cast<const ContainerType*>(container)->size(); }
	static void Clear_Impl(void* container) { static_cast<ContainerType*>(container)->clear(); }
	static void ForEach_Impl(void* container, void (*callback)(void*, void*, void*), void* userData) {
		ContainerType* map = static_cast<ContainerType*>(container);
		for (auto& pair : *map) {
			callback(&pair.second, const_cast<void*>(static_cast<const void*>(&pair.first)), userData);
		}
	}
	static void AddPair_Impl(void* container) {
		ContainerType* map = static_cast<ContainerType*>(container);
		map->emplace(K(), V());
	}
	static void Remove_Impl(void* container, const void* keyPtr) {
		ContainerType* map = static_cast<ContainerType*>(container);
		map->erase(*static_cast<const K*>(keyPtr));
	}
	static void* GetValue_Impl(void* container, const void* keyPtr) {
		ContainerType* map = static_cast<ContainerType*>(container);
		const K* key = static_cast<const K*>(keyPtr);
		auto it = map->find(*key);
		return (it != map->end()) ? &it->second : nullptr;
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
		return accessor;
	}
};
END