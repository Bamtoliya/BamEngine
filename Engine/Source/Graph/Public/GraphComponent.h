#pragma once

#include "Component.h"
#include <functional>
#include <unordered_map>
#include <string>

BEGIN(Engine)

CLASS()
class ENGINE_API GraphComponent : public Component
{
	REFLECT_CLASS()

public:
	GraphComponent() {}
	virtual ~GraphComponent() = default;

public:
	// Event Name (e.g., "OnUpdate", "OnDamage") -> C++ Native Function mapping
	std::unordered_map<std::string, std::function<void(GameObject*, void*)>> m_EventDispatchers;

	// Trigger a specific event, executing the compiled native code
	void TriggerEvent(const std::string& eventName, void* eventData = nullptr)
	{
		auto it = m_EventDispatchers.find(eventName);
		if (it != m_EventDispatchers.end())
		{
			it->second(GetOwner(), eventData);
		}
	}
};

END
