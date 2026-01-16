#pragma once
#include <type_traits>

namespace Engine
{
	template<typename Enum>
	struct EnableBitMaskOperators
	{
		static constexpr bool enable = false;
	};

	// 매크로: 이 매크로를 사용한 Enum만 비트 연산자가 오버로딩됨
#define ENABLE_BITMASK_OPERATORS(x) template<> struct EnableBitMaskOperators<x> { static constexpr bool enable = true; };

	// | (OR)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum operator|(Enum lhs, Enum rhs)
	{
		using T = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<T>(lhs) | static_cast<T>(rhs));
	}

	// & (AND)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum operator&(Enum lhs, Enum rhs)
	{
		using T = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<T>(lhs) & static_cast<T>(rhs));
	}

	// ^ (XOR)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum operator^(Enum lhs, Enum rhs)
	{
		using T = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
	}

	// ~ (NOT)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum operator~(Enum rhs)
	{
		using T = std::underlying_type_t<Enum>;
		return static_cast<Enum>(~static_cast<T>(rhs));
	}

	// |= (OR Assign)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum& operator|=(Enum& lhs, Enum rhs)
	{
		lhs = lhs | rhs;
		return lhs;
	}

	// &= (AND Assign)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum& operator&=(Enum& lhs, Enum rhs)
	{
		lhs = lhs & rhs;
		return lhs;
	}

	// ^= (XOR Assign)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr Enum& operator^=(Enum& lhs, Enum rhs)
	{
		lhs = lhs ^ rhs;
		return lhs;
	}

	// Helper: 플래그 체크 함수 (HasFlag)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr bool HasFlag(Enum value, Enum flag)
	{
		return (value & flag) == flag;
	}

	// AddFlag: 특정 비트 켜기 (Reference로 원본 수정)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr void AddFlag(Enum& value, Enum flag)
	{
		value |= flag;
	}

	// RemoveFlag: 특정 비트 끄기 (Reference로 원본 수정)
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr void RemoveFlag(Enum& value, Enum flag)
	{
		value &= ~flag;
	}

	// SetFlag: 조건에 따라 비트 켜거나 끄기
	template<typename Enum>
		requires EnableBitMaskOperators<Enum>::enable
	inline constexpr void SetFlag(Enum& value, Enum flag, bool on)
	{
		if (on)
			AddFlag(value, flag);
		else
			RemoveFlag(value, flag);
	}
}