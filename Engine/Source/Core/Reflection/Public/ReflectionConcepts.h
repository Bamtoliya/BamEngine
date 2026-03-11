#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)

template<typename T>
concept ReflectableClass = is_class_v<T>;

template<typename T>
concept ConcreteClass = std::is_class_v<T> && !std::is_abstract_v<T>;

template<typename T>
concept AbstractClass = std::is_class_v<T> && std::is_abstract_v<T>;

template<typename T>
concept ReflectableContainer = requires(T a)
{
	{ a.begin() } -> std::forward_iterator;
	{ a.end() } -> std::forward_iterator;
	typename T::value_type;
} && !is_convertible_v<T, std::string_view>;

END