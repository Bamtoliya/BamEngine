#pragma once

#include "Base.h"
#include "Resource.h"

struct Keyframe
{
	f64 Time;
	bool operator==(const Keyframe& other) const {
		return Time == other.Time;
	}
};

struct VectorKeyframe : public Keyframe
{
	vec3 Value;
	bool operator==(const VectorKeyframe& other) const {
		return Time == other.Time && Value == other.Value;
	}
};

struct QuaternionKeyframe : public Keyframe
{
	quat Value;
	bool operator==(const QuaternionKeyframe& other) const {
		return Time == other.Time &&
			Value == Value;
	}
};

struct AnimationTrack
{
	wstring TargetBoneName;
	vector<VectorKeyframe> PositionKeyframes;
	vector<QuaternionKeyframe> RotationKeyframes;
	vector<VectorKeyframe> ScaleKeyframes;

	bool operator==(const AnimationTrack& other) const {
		return TargetBoneName == other.TargetBoneName &&
			PositionKeyframes == other.PositionKeyframes &&
			RotationKeyframes == other.RotationKeyframes &&
			ScaleKeyframes == other.ScaleKeyframes;
	}
};

struct tagAnimationCreateDesc : public tagResourceCreateDesc
{
	wstring Name;
	f64 Duration = 0.0;
	f64 TicksPerSecond = 0.0;
	vector<AnimationTrack> Tracks;
};

BEGIN(Engine)
CLASS()
class ENGINE_API Animation : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Animation)
#pragma region Constructor&Destructor
private:
	Animation() : Resource(EResourceType::Animation) {}
	virtual ~Animation() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Animation* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion


#pragma region Getter
public:
	const wstring& GetName() const { return m_Name; }
	f64 GetDuration() const { return m_Duration; }
	f64 GetTicksPerSecond() const { return m_TicksPerSecond; }
	const vector<AnimationTrack>& GetTracks() const { return m_Tracks; }
#pragma endregion


private:
	PROPERTY()
	wstring m_Name = { L"" };

	PROPERTY()
	f64 m_Duration = { 0.0f };

	PROPERTY()
	f64 m_TicksPerSecond = { 0.0f };

	PROPERTY()
	vector<AnimationTrack> m_Tracks;
};
END