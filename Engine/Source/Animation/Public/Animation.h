#pragma once

#include "Base.h"
#include "Resource.h"
#include "Types.h"

STRUCT()
struct Keyframe
{
	REFLECT_BODY()

	PROPERTY()
	Engine::f64 Time = 0.0;

	bool operator==(const Keyframe& other) const {
		return Time == other.Time;
	}
};

STRUCT()
struct VectorKeyframe : public Keyframe
{
	REFLECT_BODY()

	PROPERTY()
	Engine::vec3 Value = Engine::vec3(0.0f);

	bool operator==(const VectorKeyframe& other) const {
		return Time == other.Time && Value == other.Value;
	}
};

STRUCT()
struct QuaternionKeyframe : public Keyframe
{
	REFLECT_BODY()

	PROPERTY()
	Engine::quat Value = glm::identity<Engine::quat>();

	bool operator==(const QuaternionKeyframe& other) const {
		return Time == other.Time &&
			Value == other.Value;
	}
};

STRUCT()
struct AnimationTrack
{
	REFLECT_BODY()

	PROPERTY()
	Engine::wstring TargetBoneName;

	PROPERTY()
	std::vector<VectorKeyframe> PositionKeyframes;

	PROPERTY()
	std::vector<QuaternionKeyframe> RotationKeyframes;

	PROPERTY()
	std::vector<VectorKeyframe> ScaleKeyframes;

	bool operator==(const AnimationTrack& other) const {
		return TargetBoneName == other.TargetBoneName &&
			PositionKeyframes == other.PositionKeyframes &&
			RotationKeyframes == other.RotationKeyframes &&
			ScaleKeyframes == other.ScaleKeyframes;
	}
};

struct AnimationCreateDesc : public ResourceCreateDesc
{
	Engine::wstring Name;
	Engine::f64 Duration = 0.0;
	Engine::f64 TicksPerSecond = 0.0;
	std::vector<AnimationTrack> Tracks;
};

BEGIN(Engine)
CLASS()
class ENGINE_API Animation : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Animation)
#pragma region Constructor&Destructor
private:
	using DESC = AnimationCreateDesc;
	Animation() : Resource(EResourceType::Animation) {}
	virtual ~Animation() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static Animation* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion


#pragma region Getter
public:
	const wstring& GetName() const { return m_Name; }
	f64 GetDuration() const { return m_Duration; }
	f64 GetTicksPerSecond() const { return m_TicksPerSecond; }
	const std::vector<AnimationTrack>& GetTracks() const { return m_Tracks; }
#pragma endregion


private:
	PROPERTY()
	wstring m_Name = { L"" };

	PROPERTY()
	f64 m_Duration = { 0.0f };

	PROPERTY()
	f64 m_TicksPerSecond = { 0.0f };

	PROPERTY()
	std::vector<AnimationTrack> m_Tracks;
};
END
