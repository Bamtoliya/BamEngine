#pragma once

#include "Types.h"
#include <string_view>
#include <cstddef>

namespace Engine
{
	template<typename T> requires std::is_pointer_v<T>
	void Safe_Delete(T& Pointer)
	{
		if (nullptr != Pointer)
		{
			delete Pointer;
			Pointer = nullptr;
		}
	}

	template<typename T>
	void Safe_Delete_Array(T& Pointer)
	{
		if (nullptr != Pointer)
		{
			delete[] Pointer;
			Pointer = nullptr;
		}
	}

	template<typename T> requires std::is_pointer_v<T>
	int32 Safe_AddRef(T& Instance)
	{
		int32 iRefCnt = { 0 };

		if (nullptr != Instance)
			iRefCnt = Instance->AddRef();

		return iRefCnt;
	}

	template<typename T> requires std::is_pointer_v<T>
	int32 Safe_Release(T& Instance)
	{
		int32 iRefCnt = { 0 };

		if (nullptr != Instance)
		{
			iRefCnt = Instance->Release();
			if (0 == iRefCnt)
				Instance = nullptr;
		}
		return iRefCnt;
	}

	inline wstring StrToWStr(const string& str)
	{
		return std::filesystem::path((const char8_t*)str.c_str()).wstring();
	}

	inline string WStrToStr(const wstring& wstr)
	{
		auto u8str = std::filesystem::path(wstr).u8string();
		return string(u8str.begin(), u8str.end());
	}

	inline wstring NormalizePath(wstring path)
	{
		std::replace(path.begin(), path.end(), L'\\', L'/');
		return path;
	}
	inline string NormalizePath(string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		return path;
	}

	// FNV-1a 64-bit Hash Function for compile-time string hashing
	consteval uint64 CompileTimeHash(std::string_view str) noexcept
	{
		uint64 hash = 14695981039346656037ull;
		for (char c : str)
		{
			hash ^= static_cast<uint64>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	consteval uint64 CompileTimeHash(std::wstring_view str) noexcept
	{
		uint64 hash = 14695981039346656037ull;
		for (wchar_t c : str)
		{
			hash ^= static_cast<uint64>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	constexpr uint64 RunTimeHash(std::string_view str) noexcept
	{
		uint64 hash = 14695981039346656037ull;
		for (char c : str)
		{
			hash ^= static_cast<uint64>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	constexpr uint64 RunTimeHash(std::wstring_view str) noexcept
	{
		uint64 hash = 14695981039346656037ull;
		for (wchar_t c : str)
		{
			hash ^= static_cast<uint64>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}
}

inline void HashCombine(std::size_t& seed, std::size_t value)
{
	seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}