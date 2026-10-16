#pragma once

#include "GraphAnnotations.h"
#include <cstdint>

namespace graph_checks
{

using GraphFloat = float;

inline int Pulses = 0;

FUNCTION(GRAPH_PURE("Check.HasWork"))
inline bool HasWork()
{
	return Pulses < 3;
}

FUNCTION(GRAPH_PURE("Check.ReadPulses"))
inline float ReadPulses()
{
	return static_cast<float>(Pulses);
}

FUNCTION(GRAPH_CALLABLE("Check.Pulse"))
inline void Pulse()
{
	if (Pulses < 3) ++Pulses;
}

FUNCTION(GRAPH_PURE("Check.Int32"), GRAPH_PIN_DEFAULT("value", 0))
inline int Int32(int value)
{
	return value;
}

FUNCTION(GRAPH_PURE("Check.Add"), DISPLAY_NAME("Add"), CATEGORY("Check/Math"), TOOLTIP("Add two floats."),
	GRAPH_PIN_KEY("left", "a"), GRAPH_PIN_NAME("left", "Left"),
	GRAPH_PIN_DEFAULT("left", 0.0f), GRAPH_PIN_DEFAULT("right", 1.0f))
inline GraphFloat Add(float left, float right)
{
	return left + right;
}

STRUCT()
struct Counter
{
	REFLECT_BODY()

	PROPERTY(GRAPH_READWRITE, GRAPH_ID("Check.Counter.Value"), DISPLAY_NAME("Value"), CATEGORY("Check/State"))
	std::int64_t Value = 0;

	PROPERTY(GRAPH_READONLY, GRAPH_ID("Check.Counter.Enabled"))
	const bool Enabled = true;

	FUNCTION(GRAPH_CALLABLE("Check.Counter.Assign"), GRAPH_PIN_DEFAULT("amount", 1))
	void Assign(std::int64_t amount)
	{
		Value = amount;
	}

	FUNCTION(GRAPH_PURE("Check.Counter.Positive"))
	bool IsPositive() const
	{
		return Value > 0;
	}

	FUNCTION(GRAPH_PURE("Check.Counter.Twice"), GRAPH_PIN_DEFAULT("value", 0.0f))
	static float Twice(float value)
	{
		return value * 2.0f;
	}
};

}
