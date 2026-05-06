#pragma once
#include <type_traits>

template<typename T>
constexpr bool EnableBitMask(T) { return false; }

#define ENABLE_BITMASK_OPERATORS(x) \
	constexpr bool EnableBitMask(x) { return true; }

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum operator|(Enum lhs, Enum rhs)
{
	using T = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum operator&(Enum lhs, Enum rhs)
{
	using T = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum operator^(Enum lhs, Enum rhs)
{
	using T = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
}

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum operator~(Enum rhs)
{
	using T = std::underlying_type_t<Enum>;
	return static_cast<Enum>(~static_cast<T>(rhs));
}

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum& operator|=(Enum& lhs, Enum rhs)
{
	lhs = lhs | rhs;
	return lhs;
}

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum& operator&=(Enum& lhs, Enum rhs)
{
	lhs = lhs & rhs;
	return lhs;
}

template<typename Enum>
	requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
inline constexpr Enum& operator^=(Enum& lhs, Enum rhs)
{
	lhs = lhs ^ rhs;
	return lhs;
}

namespace Engine
{
	template<typename Enum>
		requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
	inline constexpr bool HasFlag(Enum value, Enum flag)
	{
		return (value & flag) == flag;
	}

	template<typename Enum>
		requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
	inline constexpr void AddFlag(Enum& value, Enum flag)
	{
		value |= flag;
	}

	template<typename Enum>
		requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
	inline constexpr void RemoveFlag(Enum& value, Enum flag)
	{
		value &= ~flag;
	}

	template<typename Enum>
		requires (std::is_enum_v<Enum>&& EnableBitMask(Enum{}))
	inline constexpr void SetFlag(Enum& value, Enum flag, bool on)
	{
		if (on)
			AddFlag(value, flag);
		else
			RemoveFlag(value, flag);
	}
}
