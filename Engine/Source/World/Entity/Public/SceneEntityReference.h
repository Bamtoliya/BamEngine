#pragma once

#include <cstdint>

namespace Engine
{
	// An IDComponent::id in the owning scene. Zero represents an unassigned entity.
	struct SceneEntityReference
	{
		std::uint64_t PersistentId = 0;

		bool operator==(const SceneEntityReference&) const = default;
	};
}
