#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Engine
{
	// [Character Types]
	// char : 시스템 기본 문자 (Legacy 호환용, 보통 1byte)
	// char8_t (C++20) : UTF-8 문자 (명확한 8비트)
	using ansichar = char;			// 기존 C 스타일 문자열용
	using char8 = char8_t;			// C++20 UTF-8 전용 (u8"문자열")
	using char16 = char16_t;		// UTF-16 (Windows Wide char, u"문자열")
	using char32 = char32_t;		// UTF-32 (Linux/Unix Wide char, U"문자열")
	using wchar = wchar_t;			// 플랫폼 종속적 Wide char (Win: 16bit, Linux: 32bit)

	// [Bytes]
	using byte = std::uint8_t;

	// [Integers]
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = std::int64_t;

	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = std::uint64_t;

	// [Size & Pointers]
	using sizet = std::size_t;
	using intptr = std::intptr_t;
	using uintptr = std::uintptr_t;

	// [Floating Point]
	using f32 = float;
	using f64 = double;

	// [String Objects]
	using string = std::string;				// base: char (ansichar)
	using u8string = std::u8string;			// base: char8_t (C++20 Standard)
	using wstring = std::wstring;			// base: wchar_t
	using u16string = std::u16string;		// base: char16_t
	using u32string = std::u32string;		// base: char32_t

	// [Math (GLM)]
	using vec2 = glm::vec2;
	using vec3 = glm::vec3;
	using vec4 = glm::vec4;
	using mat3 = glm::mat3;
	using mat4 = glm::mat4;
	using quat = glm::quat;
}