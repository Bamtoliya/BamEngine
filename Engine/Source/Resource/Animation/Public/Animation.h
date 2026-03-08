#pragma once

#include "Base.h"
#include "Resource.h"

struct Keyframe
{
	f64 Time;
};

struct VectorKeyframe : public Keyframe
{
	vec3 Value;
};

struct QuaternionKeyframe : public Keyframe
{
	quat Value;
};

struct AnimationTrack
{
	wstring TargetBoneName;
	vector<VectorKeyframe> PositionKeyframes;
	vector<QuaternionKeyframe> RotationKeyframes;
	vector<VectorKeyframe> ScaleKeyframes;
};

BEGIN(Engine)
class ENGINE_API Animation : public Base
{
#pragma region Constructor&Destructor
private:
	Animation() = default;
	virtual ~Animation() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Animation* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

private:
	wstring m_Name = { L"" };
	f64 m_Duration = { 0.0f };
	f64 m_TicksPerSecond = { 0.0f };
	vector<AnimationTrack> m_Tracks;
};
END