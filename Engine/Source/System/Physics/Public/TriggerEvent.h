#pragma once
#include <entt/entt.hpp>
namespace Engine
{
	enum class ETriggerEventType : uint8_t
	{
		Enter,
		Stay,
		Exit
	};

	struct TriggerEvent
	{
		entt::entity entityA = entt::null;
		entt::entity entityB = entt::null;
		ETriggerEventType type = ETriggerEventType::Enter;
	};
}