#pragma once

#include "Engine_Includes.h"
#include "Animation.h"
#include "ResourceHandle.h"
#include "ReflectionMacro.h"
#include <array>
#include <vector>

BEGIN(Engine)

struct BlendClipWeight2D
{
	const Animation* Clip = nullptr;
	float Weight = 0.0f;
};

STRUCT()
struct ENGINE_API BlendSample2D
{
	REFLECT_BODY()

	PROPERTY(EDITABLE)
		float x = 0.0f;

	PROPERTY(EDITABLE)
		float y = 0.0f;

	PROPERTY(EDITABLE)
		ResourceHandle<Animation> clip;
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
		std::vector<BlendSample2D> samples;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string parameterX;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string parameterY;

	[[nodiscard]] bool Evaluate(float x, float y, std::array<BlendClipWeight2D, 4>& out) const;
	[[nodiscard]] bool Evaluate(float x, float y, std::vector<BlendClipWeight2D>& out) const;

private:
	[[nodiscard]] bool EvaluateGrid(float x, float y, std::array<BlendClipWeight2D, 4>& out) const;
	[[nodiscard]] bool EvaluateGradientBand(float x, float y, std::vector<BlendClipWeight2D>& out) const;
};

END