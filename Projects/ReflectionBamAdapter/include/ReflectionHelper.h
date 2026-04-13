#pragma once 

#include "Engine_Includes.h"

BEGIN(Engine)
template<typename T>
void PropertyCopy(void* dst, const void* src)
{
	*static_cast<T*>(dst) = *static_cast<const T*>(src);
}

template<typename T>
bool PropertyEqual(const void* a, const void* b)
{
	if constexpr (requires(const T & x, const T & y) { { x == y } -> std::convertible_to<bool>; })
		return *static_cast<const T*>(a) == *static_cast<const T*>(b);
	else
		return std::memcmp(a, b, sizeof(T)) == 0;
}

template<typename Ret, typename Class, typename... Args>
void* InvokeMemFn(Ret(Class::* fn)(Args...), void* instance, void** args)
{
	return[&]<size_t... I>(std::index_sequence<I...>) -> void* {
		if constexpr (std::is_void_v<Ret>) {
			(static_cast<Class*>(instance)->*fn)(
				*static_cast<std::remove_cvref_t<Args>*>(args[I])...);
			return nullptr;
		}
		else {
			static thread_local std::remove_cvref_t<Ret> _ret;
			_ret = (static_cast<Class*>(instance)->*fn)(
				*static_cast<std::remove_cvref_t<Args>*>(args[I])...);
			return &_ret;
		}
	}(std::index_sequence_for<Args...>{});
}

template<typename Ret, typename Class, typename... Args>
void* InvokeMemFn(Ret(Class::* fn)(Args...) const, void* instance, void** args)
{
	return[&]<size_t... I>(std::index_sequence<I...>) -> void* {
		if constexpr (std::is_void_v<Ret>) {
			(static_cast<Class*>(instance)->*fn)(
				*static_cast<std::remove_cvref_t<Args>*>(args[I])...);
			return nullptr;
		}
		else {
			static thread_local std::remove_cvref_t<Ret> _ret;
			_ret = (static_cast<Class*>(instance)->*fn)(
				*static_cast<std::remove_cvref_t<Args>*>(args[I])...);
			return &_ret;
		}
	}(std::index_sequence_for<Args...>{});
}

template<auto Fn>
void* AutoThunk(void* instance, void** args)
{
	return InvokeMemFn(Fn, instance, args);
}

END