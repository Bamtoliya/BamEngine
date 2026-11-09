#pragma once

#include "Engine_Includes.h"
#include "ResourceHandle.h"
#include "Animation.h"
#include "ReflectionMacro.h"
#include <string>
#include <vector>

BEGIN(Engine)

STRUCT()
struct ENGINE_API BlendSample1D
{
	REFLECT_BODY()

	PROPERTY(EDITABLE)
	float threshold = 0.0f;

	PROPERTY(EDITABLE)
	ResourceHandle<Animation> clip;
};

CLASS()
class ENGINE_API BlendTree1D
{
	REFLECT_BODY()

public:
	PROPERTY(EDITABLE)
		std::string name;

	PROPERTY(EDITABLE)
		std::string parameterName;

	PROPERTY(EDITABLE)
		std::vector<BlendSample1D> samples;

	void AddSample(float threshold, const ResourceHandle<Animation>& clip);
	void SortSamples();
	bool Evaluate(float paramValue, const Animation*& outClipA, const Animation*& outClipB, float& outAlpha) const;
};

END