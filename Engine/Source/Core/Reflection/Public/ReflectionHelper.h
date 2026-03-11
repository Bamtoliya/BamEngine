#pragma once 

#include "Engine_Includes.h"
#include "Types.h"


BEGIN(Engine)
// FNV-1a 64-bit Hash Function for compile-time string hashing
consteval uint64 CompileTimeHash(string_view str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	for (char c : str)
	{
		hash ^= static_cast<uint64>(c);
		hash *= 1099511628211ull;
	}
	return hash;
} 

consteval uint64 CompileTimeHash(wstring_view str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	for (wchar_t c : str)
	{
		hash ^= static_cast<uint64>(c);
		hash *= 1099511628211ull;
	}
	return hash;
}

consteval uint64 CompileTimeHash(const char* str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	while (*str)
	{
		hash ^= static_cast<uint64>(*str++);
		hash *= 1099511628211ull;
	}
	return hash;
}



consteval uint64 CompileTimeHash(const wchar* str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	while (*str)
	{
		hash ^= static_cast<uint64>(*str++);
		hash *= 1099511628211ull;
	}
	return hash;
}

constexpr uint64 RunTimeHash(string_view str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	for (char c : str)
	{
		hash ^= static_cast<uint64>(c);
		hash *= 1099511628211ull;
	}
	return hash;
}

constexpr uint64 RunTimeHash(wstring_view str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	for (wchar_t c : str)
	{
		hash ^= static_cast<uint64>(c);
		hash *= 1099511628211ull;
	}
	return hash;
}

constexpr uint64 RunTimeHash(const char* str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	while (*str)
	{
		hash ^= static_cast<uint64>(*str++);
		hash *= 1099511628211ull;
	}
	return hash;
}

constexpr uint64 RunTimeHash(const wchar* str) noexcept
{
	uint64 hash = 14695981039346656037ull;
	while (*str)
	{
		hash ^= static_cast<uint64>(*str++);
		hash *= 1099511628211ull;
	}
	return hash;
}

END