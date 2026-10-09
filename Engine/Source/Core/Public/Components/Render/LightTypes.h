#pragma once

#include <reflection/Annotations.h>
#include "Types.h"
#include "Interface/EnumBit.h"

namespace Engine
{
	ENUM()
		enum class ELightType
	{
		Point = 0,
		Directional = 1,
		Spot = 2,
		Sky = 3
	};

	ENUM()
		enum class EAttenuationMode : uint8
	{
		Coefficients = 0,
		InverseSquare = 1,
		Disabled = 2
	};

	ENUM(META("Bitmask", true))
		enum class ELightFlags
	{
		None = 0,
		CastShadows = 1 << 0,
		UseInForwardRendering = 1 << 1,
		UseInDeferredRendering = 1 << 2,
		Volumetric = 1 << 3,
		AffectDiffuse = 1 << 4,
		AffectSpecular = 1 << 5
	};

	ENABLE_BITMASK_OPERATORS(ELightFlags)
}