#pragma once

#include <entt/core/type_info.hpp>
#include <type_traits>

namespace Engine
{
	class ITypeIdentity
	{
	public:
		virtual ~ITypeIdentity() = default;
		virtual entt::id_type GetTypeID() const noexcept = 0;
	};
}

#define DECLARE_TYPE_ID() \
public: \
	entt::id_type GetTypeID() const noexcept override \
	{ \
		using IdentityType = std::remove_cvref_t<decltype(*this)>; \
		return entt::type_hash<IdentityType>::value(); \
	}