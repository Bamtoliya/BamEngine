#pragma once

#include "GraphAnnotations.h"
#include "GraphMathLibrary.h"
#include "GraphVectorLibrary.h"
#include <cmath>

namespace Engine::GraphLogic
{

// These are ordinary functions. Reflection supplies their Logic node schemas.
FUNCTION(GRAPH_PURE("Math.AbsFloat"), DISPLAY_NAME("Abs (Float)"), CATEGORY("Math/Float"),
	GRAPH_PIN_DEFAULT("value", 0.0f), TOOLTIP("Return the absolute value."))
inline float AbsFloat(float value)
{
	return std::abs(value);
}

FUNCTION(GRAPH_PURE("Logic.Xor"), DISPLAY_NAME("Xor"), CATEGORY("Logic/Bool"),
	GRAPH_PIN_DEFAULT("a", false), GRAPH_PIN_DEFAULT("b", false))
inline bool Xor(bool a, bool b)
{
	return a != b;
}

}
