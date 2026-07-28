#pragma once

#include "Types.h"
#include "Delegate.h"
#include "Macro.h"
#include "ReflectionMacro.h"
#include "EventInterface.h"

BEGIN(Engine)
struct EventParameter
{
	reflection::EPropertyType Type;

	int32 IntValue;
	f32 FloatValue;
	string StringValue;
	bool BoolValue;
};

struct EventBinding
{
	uint64 ID;
	uint64 TargetGameObjectID;
	string ComponentTypeName;
	string MethodName;
	vector<EventParameter> Parameters;
};

CLASS()
class ENGINE_API DynamicEvent
{
	REFLECT_STRUCT()
public:
	void Invoke() const;
	EResult AddEvent(const string& componentTypeName, const string& methodName)
	{
		EventBinding binding;
		binding.ID = ++NextID;
		binding.ComponentTypeName = componentTypeName;
		binding.MethodName = methodName;
		Bindings.push_back(binding);
		return EResult::Success;
	}
	EResult RemoveEvent(uint64 id)
	{
		auto it = std::remove_if(Bindings.begin(), Bindings.end(), [id](const EventBinding& binding) {
			return binding.ID == id;
		});
		if (it != Bindings.end())
		{
			Bindings.erase(it, Bindings.end());
			return EResult::Success;
		}
		return EResult::Fail;
	}
private:
	vector<EventBinding> Bindings;
	uint64 NextID = 0;
};
END