#pragma once

#include "Engine_Includes.h"
#include "Animation.h"
#include "Skeleton.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Engine
{
	struct RootMotionDelta
	{
		vec3 deltaPosition = vec3(0.0f);
		quat deltaRotation = glm::identity<quat>();
		bool valid = false;
	};

	namespace RootMotionInternal
	{
		inline vec3 SamplePosition(const std::vector<VectorKeyframe>& keys, f64 animTime)
		{
			if (keys.empty()) return vec3(0.0f);
			if (keys.size() == 1 || animTime <= keys.front().Time) return keys.front().Value;
			if (animTime >= keys.back().Time) return keys.back().Value;

			for (size_t i = 0; i + 1 < keys.size(); ++i)
			{
				if (animTime >= keys[i].Time && animTime <= keys[i + 1].Time)
				{
					const f64 dt = keys[i + 1].Time - keys[i].Time;
					const f32 factor = dt > 0.0 ? static_cast<f32>((animTime - keys[i].Time) / dt) : 0.0f;
					return glm::mix(keys[i].Value, keys[i + 1].Value, factor);
				}
			}
			return keys.back().Value;
		}

		inline quat SampleRotation(const std::vector<QuaternionKeyframe>& keys, f64 animTime)
		{
			if (keys.empty()) return glm::identity<quat>();
			if (keys.size() == 1 || animTime <= keys.front().Time) return glm::normalize(keys.front().Value);
			if (animTime >= keys.back().Time) return glm::normalize(keys.back().Value);

			for (size_t i = 0; i + 1 < keys.size(); ++i)
			{
				if (animTime >= keys[i].Time && animTime <= keys[i + 1].Time)
				{
					const f64 dt = keys[i + 1].Time - keys[i].Time;
					const f32 factor = dt > 0.0 ? static_cast<f32>((animTime - keys[i].Time) / dt) : 0.0f;
					return glm::normalize(glm::slerp(keys[i].Value, keys[i + 1].Value, factor));
				}
			}
			return glm::normalize(keys.back().Value);
		}

		inline const AnimationTrack* FindTrack(const Animation& clip, int32 rootBoneIndex, const Skeleton* skeleton = nullptr)
		{
			const auto& tracks = clip.GetTracks();
			if (tracks.empty()) return nullptr;

			if (skeleton && rootBoneIndex >= 0 && static_cast<size_t>(rootBoneIndex) < skeleton->GetBones().size())
			{
				const auto& boneName = skeleton->GetBones()[rootBoneIndex].Name;
				for (const auto& track : tracks)
				{
					if (track.TargetBoneName == boneName)
						return &track;
				}
			}

			if (rootBoneIndex >= 0 && static_cast<size_t>(rootBoneIndex) < tracks.size())
			{
				return &tracks[rootBoneIndex];
			}

			return &tracks.front();
		}

		inline const AnimationTrack* FindTrackByName(const Animation& clip, const std::wstring& boneName)
		{
			for (const auto& track : clip.GetTracks())
			{
				if (track.TargetBoneName == boneName)
					return &track;
			}
			return nullptr;
		}
	}

	inline RootMotionDelta ExtractRootMotionDelta(
		const Animation& clip,
		int32 rootBoneIndex,
		f32 prevTime,
		f32 currTime,
		bool looped,
		const Skeleton* skeleton = nullptr)
	{
		RootMotionDelta result;
		const auto* track = RootMotionInternal::FindTrack(clip, rootBoneIndex, skeleton);
		if (!track) return result;

		const f64 tps = clip.GetTicksPerSecond() > 0.0 ? clip.GetTicksPerSecond() : 25.0;
		const f32 duration = clip.GetDuration() > 0.0 ? static_cast<f32>(clip.GetDuration() / tps) : 0.0f;
		if (duration <= 0.0f) return result;

		// Normal advance step without wrap-around
		if (!looped || currTime >= prevTime)
		{
			const f64 t0 = std::clamp(static_cast<f64>(prevTime) * tps, 0.0, clip.GetDuration());
			const f64 t1 = std::clamp(static_cast<f64>(currTime) * tps, 0.0, clip.GetDuration());

			const vec3 pos0 = RootMotionInternal::SamplePosition(track->PositionKeyframes, t0);
			const vec3 pos1 = RootMotionInternal::SamplePosition(track->PositionKeyframes, t1);
			const quat rot0 = RootMotionInternal::SampleRotation(track->RotationKeyframes, t0);
			const quat rot1 = RootMotionInternal::SampleRotation(track->RotationKeyframes, t1);

			result.deltaPosition = pos1 - pos0;
			result.deltaRotation = rot1 * glm::inverse(rot0);
			result.valid = true;
			return result;
		}

		// Loop wrap-around: split [prevTime .. duration] + [0.0 .. currTime]
		const f64 tPrev = std::clamp(static_cast<f64>(prevTime) * tps, 0.0, clip.GetDuration());
		const f64 tDur  = clip.GetDuration();
		const f64 tZero = 0.0;
		const f64 tCurr = std::clamp(static_cast<f64>(currTime) * tps, 0.0, clip.GetDuration());

		// Segment 1: [prevTime .. duration]
		const vec3 posPrev = RootMotionInternal::SamplePosition(track->PositionKeyframes, tPrev);
		const vec3 posEnd  = RootMotionInternal::SamplePosition(track->PositionKeyframes, tDur);
		const quat rotPrev = RootMotionInternal::SampleRotation(track->RotationKeyframes, tPrev);
		const quat rotEnd  = RootMotionInternal::SampleRotation(track->RotationKeyframes, tDur);

		const vec3 deltaPos1 = posEnd - posPrev;
		const quat deltaRot1 = rotEnd * glm::inverse(rotPrev);

		// Segment 2: [0.0 .. currTime]
		const vec3 posStart = RootMotionInternal::SamplePosition(track->PositionKeyframes, tZero);
		const vec3 posCurr  = RootMotionInternal::SamplePosition(track->PositionKeyframes, tCurr);
		const quat rotStart = RootMotionInternal::SampleRotation(track->RotationKeyframes, tZero);
		const quat rotCurr  = RootMotionInternal::SampleRotation(track->RotationKeyframes, tCurr);

		const vec3 deltaPos2 = posCurr - posStart;
		const quat deltaRot2 = rotCurr * glm::inverse(rotStart);

		result.deltaPosition = deltaPos1 + deltaPos2;
		result.deltaRotation = deltaRot2 * deltaRot1;
		result.valid = true;
		return result;
	}

	inline RootMotionDelta ExtractRootMotionDelta(
		const Animation& clip,
		const std::string& rootBoneName,
		f32 prevTime,
		f32 currTime,
		bool looped)
	{
		const std::wstring wname{ Unicode::Utf8ToWide(rootBoneName) };
		RootMotionDelta result;
		const auto* track = RootMotionInternal::FindTrackByName(clip, wname);
		if (!track)
		{
			return ExtractRootMotionDelta(clip, 0, prevTime, currTime, looped, nullptr);
		}

		const f64 tps = clip.GetTicksPerSecond() > 0.0 ? clip.GetTicksPerSecond() : 25.0;
		const f32 duration = clip.GetDuration() > 0.0 ? static_cast<f32>(clip.GetDuration() / tps) : 0.0f;
		if (duration <= 0.0f) return result;

		if (!looped || currTime >= prevTime)
		{
			const f64 t0 = std::clamp(static_cast<f64>(prevTime) * tps, 0.0, clip.GetDuration());
			const f64 t1 = std::clamp(static_cast<f64>(currTime) * tps, 0.0, clip.GetDuration());

			const vec3 pos0 = RootMotionInternal::SamplePosition(track->PositionKeyframes, t0);
			const vec3 pos1 = RootMotionInternal::SamplePosition(track->PositionKeyframes, t1);
			const quat rot0 = RootMotionInternal::SampleRotation(track->RotationKeyframes, t0);
			const quat rot1 = RootMotionInternal::SampleRotation(track->RotationKeyframes, t1);

			result.deltaPosition = pos1 - pos0;
			result.deltaRotation = rot1 * glm::inverse(rot0);
			result.valid = true;
			return result;
		}

		const f64 tPrev = std::clamp(static_cast<f64>(prevTime) * tps, 0.0, clip.GetDuration());
		const f64 tDur  = clip.GetDuration();
		const f64 tZero = 0.0;
		const f64 tCurr = std::clamp(static_cast<f64>(currTime) * tps, 0.0, clip.GetDuration());

		const vec3 posPrev = RootMotionInternal::SamplePosition(track->PositionKeyframes, tPrev);
		const vec3 posEnd  = RootMotionInternal::SamplePosition(track->PositionKeyframes, tDur);
		const quat rotPrev = RootMotionInternal::SampleRotation(track->RotationKeyframes, tPrev);
		const quat rotEnd  = RootMotionInternal::SampleRotation(track->RotationKeyframes, tDur);

		const vec3 deltaPos1 = posEnd - posPrev;
		const quat deltaRot1 = rotEnd * glm::inverse(rotPrev);

		const vec3 posStart = RootMotionInternal::SamplePosition(track->PositionKeyframes, tZero);
		const vec3 posCurr  = RootMotionInternal::SamplePosition(track->PositionKeyframes, tCurr);
		const quat rotStart = RootMotionInternal::SampleRotation(track->RotationKeyframes, tZero);
		const quat rotCurr  = RootMotionInternal::SampleRotation(track->RotationKeyframes, tCurr);

		const vec3 deltaPos2 = posCurr - posStart;
		const quat deltaRot2 = rotCurr * glm::inverse(rotStart);

		result.deltaPosition = deltaPos1 + deltaPos2;
		result.deltaRotation = deltaRot2 * deltaRot1;
		result.valid = true;
		return result;
	}
}
