#pragma once

#include "Engine_Includes.h"
#include "Animation.h"
#include "ResourceHandle.h"
#include "Reflection/ReflectionMacro.h"
#include <array>

BEGIN(Engine)

struct BlendClipWeight2D
{
	const Animation* Clip = nullptr;
	float Weight = 0.0f;
};

CLASS()
class ENGINE_API BlendTree2D
{
	REFLECT_BODY()

public:
	PROPERTY(EDITABLE)
		float minX = 0.0f;

	PROPERTY(EDITABLE)
		float maxX = 1.0f;

	PROPERTY(EDITABLE)
		float minY = 0.0f;

	PROPERTY(EDITABLE)
		float maxY = 1.0f;

	PROPERTY(EDITABLE)
		ResourceHandle<Animation> clip00;

	PROPERTY(EDITABLE)
		ResourceHandle<Animation> clip10;

	PROPERTY(EDITABLE)
		ResourceHandle<Animation> clip01;

	PROPERTY(EDITABLE)
		ResourceHandle<Animation> clip11;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string parameterX;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string parameterY;

	[[nodiscard]] bool Evaluate(float x, float y, std::array<BlendClipWeight2D, 4>& out) const;
};

END