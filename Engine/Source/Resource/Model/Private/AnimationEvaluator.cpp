#include "AnimationEvaluator.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

vec3 AnimationEvaluator::InterpolatePosition(const vector<VectorKeyframe>& keys, f64 animTime, f64 duration, bool isLooping)
{
	if (keys.empty()) return vec3(0.0f);
	if (keys.size() == 1) return keys.front().Value;

	// 1. 일반 구간 보간
	if (animTime >= keys.front().Time && animTime <= keys.back().Time)
	{
		for (size_t i = 0; i + 1 < keys.size(); ++i)
		{
			if (animTime >= keys[i].Time && animTime <= keys[i + 1].Time)
			{
				const f64 dt = keys[i + 1].Time - keys[i].Time;
				const f32 factor = dt > 0.0 ? static_cast<f32>((animTime - keys[i].Time) / dt) : 0.0f;
				return glm::mix(keys[i].Value, keys[i + 1].Value, factor);
			}
		}
	}

	// 2. 루프 Wrap-around 보간: 마지막 키 -> duration -> 첫 번째 키
	if (isLooping && duration > 0.0)
	{
		const f64 wrapDt = (duration - keys.back().Time) + keys.front().Time;
		if (wrapDt > 0.0001)
		{
			const f64 elapsed = (animTime >= keys.back().Time) ? (animTime - keys.back().Time) : ((duration - keys.back().Time) + animTime);
			const f32 factor = std::clamp(static_cast<f32>(elapsed / wrapDt), 0.0f, 1.0f);
			return glm::mix(keys.back().Value, keys.front().Value, factor);
		}
	}

	return (animTime <= keys.front().Time) ? keys.front().Value : keys.back().Value;
}

quat AnimationEvaluator::InterpolateRotation(const vector<QuaternionKeyframe>& keys, f64 animTime, f64 duration, bool isLooping)
{
	if (keys.empty()) return glm::identity<quat>();
	if (keys.size() == 1) return glm::normalize(keys.front().Value);

	// 1. 일반 구간 보간
	if (animTime >= keys.front().Time && animTime <= keys.back().Time)
	{
		for (size_t i = 0; i + 1 < keys.size(); ++i)
		{
			if (animTime >= keys[i].Time && animTime <= keys[i + 1].Time)
			{
				const f64 dt = keys[i + 1].Time - keys[i].Time;
				const f32 factor = dt > 0.0 ? static_cast<f32>((animTime - keys[i].Time) / dt) : 0.0f;
				return glm::normalize(glm::slerp(keys[i].Value, keys[i + 1].Value, factor));
			}
		}
	}

	// 2. 루프 Wrap-around 보간: 마지막 키 -> duration -> 첫 번째 키
	if (isLooping && duration > 0.0)
	{
		const f64 wrapDt = (duration - keys.back().Time) + keys.front().Time;
		if (wrapDt > 0.0001)
		{
			const f64 elapsed = (animTime >= keys.back().Time) ? (animTime - keys.back().Time) : ((duration - keys.back().Time) + animTime);
			const f32 factor = std::clamp(static_cast<f32>(elapsed / wrapDt), 0.0f, 1.0f);
			return glm::normalize(glm::slerp(keys.back().Value, keys.front().Value, factor));
		}
	}

	return (animTime <= keys.front().Time) ? glm::normalize(keys.front().Value) : glm::normalize(keys.back().Value);
}

vec3 AnimationEvaluator::InterpolateScale(const vector<VectorKeyframe>& keys, f64 animTime, f64 duration, bool isLooping)
{
	if (keys.empty()) return vec3(1.0f);
	if (keys.size() == 1) return keys.front().Value;

	// 1. 일반 구간 보간
	if (animTime >= keys.front().Time && animTime <= keys.back().Time)
	{
		for (size_t i = 0; i + 1 < keys.size(); ++i)
		{
			if (animTime >= keys[i].Time && animTime <= keys[i + 1].Time)
			{
				const f64 dt = keys[i + 1].Time - keys[i].Time;
				const f32 factor = dt > 0.0 ? static_cast<f32>((animTime - keys[i].Time) / dt) : 0.0f;
				return glm::mix(keys[i].Value, keys[i + 1].Value, factor);
			}
		}
	}

	// 2. 루프 Wrap-around 보간: 마지막 키 -> duration -> 첫 번째 키
	if (isLooping && duration > 0.0)
	{
		const f64 wrapDt = (duration - keys.back().Time) + keys.front().Time;
		if (wrapDt > 0.0001)
		{
			const f64 elapsed = (animTime >= keys.back().Time) ? (animTime - keys.back().Time) : ((duration - keys.back().Time) + animTime);
			const f32 factor = std::clamp(static_cast<f32>(elapsed / wrapDt), 0.0f, 1.0f);
			return glm::mix(keys.back().Value, keys.front().Value, factor);
		}
	}

	return (animTime <= keys.front().Time) ? keys.front().Value : keys.back().Value;
}

void AnimationEvaluator::SampleLocalTRS(const Animation& animation, const unordered_map<wstring, const AnimationTrack*>& trackMap, const Bone& bone, f32 timeInSeconds, vec3& outPos, quat& outRot, vec3& outScale, bool isLooping, float loopBlendDuration)
{
	const f64 ticksPerSecond = animation.GetTicksPerSecond() > 0.0 ? animation.GetTicksPerSecond() : 25.0;
	const f64 durationTicks = animation.GetDuration();
	const f32 durationSec = (durationTicks > 0.0 && ticksPerSecond > 0.0) ? static_cast<f32>(durationTicks / ticksPerSecond) : 0.0f;
	f64 animTimeTicks = static_cast<f64>(timeInSeconds) * ticksPerSecond;
	if (durationTicks > 0.0)
	{
		animTimeTicks = isLooping ? std::fmod(animTimeTicks, durationTicks) : std::clamp(animTimeTicks, 0.0, durationTicks);
	}
	const vec3 bindPos = vec3(bone.LocalTransform[3]);
	const quat bindRot = glm::toQuat(bone.LocalTransform);
	const vec3 bindScale = vec3(glm::length(vec3(bone.LocalTransform[0])), glm::length(vec3(bone.LocalTransform[1])), glm::length(vec3(bone.LocalTransform[2])));
	const auto it = trackMap.find(bone.Name);
	if (it == trackMap.end())
	{
		outPos = bindPos;
		outRot = bindRot;
		outScale = bindScale;
		return;
	}
	const auto* track = it->second;
	outPos = !track->PositionKeyframes.empty() ? InterpolatePosition(track->PositionKeyframes, animTimeTicks, durationTicks, isLooping) : bindPos;
	outRot = !track->RotationKeyframes.empty() ? InterpolateRotation(track->RotationKeyframes, animTimeTicks, durationTicks, isLooping) : bindRot;
	outScale = !track->ScaleKeyframes.empty() ? InterpolateScale(track->ScaleKeyframes, animTimeTicks, durationTicks, isLooping) : bindScale;
	// [Loop-End Crossfade]: 루프 종료 전 0.25초 동안 시작 포즈(0.0초)로 부드럽게 블렌딩하여 툭 끊김을 원천 제거
	if (isLooping && durationSec > 0.2f && loopBlendDuration > 0.001f)
	{
		const f32 blendWindow = std::min(loopBlendDuration, durationSec * 0.25f);
		const f32 currentTimeInLoop = static_cast<f32>(std::fmod(timeInSeconds, durationSec));
		const f32 blendStartTime = durationSec - blendWindow;
		if (currentTimeInLoop >= blendStartTime)
		{
			const f32 alpha = glm::clamp((currentTimeInLoop - blendStartTime) / blendWindow, 0.0f, 1.0f);
			const vec3 startPos = !track->PositionKeyframes.empty() ? InterpolatePosition(track->PositionKeyframes, 0.0, durationTicks, false) : bindPos;
			const quat startRot = !track->RotationKeyframes.empty() ? InterpolateRotation(track->RotationKeyframes, 0.0, durationTicks, false) : bindRot;
			const vec3 startScale = !track->ScaleKeyframes.empty() ? InterpolateScale(track->ScaleKeyframes, 0.0, durationTicks, false) : bindScale;
			outPos = glm::mix(outPos, startPos, alpha);
			outRot = glm::normalize(glm::slerp(outRot, startRot, alpha));
			outScale = glm::mix(outScale, startScale, alpha);
		}
	}
}

void AnimationEvaluator::EvaluateSkinningMatrices(const Animation& animation, const Skeleton& skeleton, f32 timeInSeconds, std::vector<mat4>& outBoneMatrices, bool isLooping, float loopBlendDuration)
{
	const auto& bones = skeleton.GetBones();
	const uint32 boneCount = static_cast<uint32>(bones.size());
	if (boneCount == 0) { outBoneMatrices.clear(); return; }
	if (outBoneMatrices.size() != boneCount) outBoneMatrices.resize(boneCount, glm::identity<mat4>());
	const auto& tracks = animation.GetTracks();
	unordered_map<wstring, const AnimationTrack*> trackMap;
	for (const auto& track : tracks) trackMap[track.TargetBoneName] = &track;
	std::vector<mat4> combinedTransforms(boneCount, glm::identity<mat4>());
	for (uint32 i = 0; i < boneCount; ++i)
	{
		const auto& bone = bones[i];
		vec3 pos; quat rot; vec3 scl;
		SampleLocalTRS(animation, trackMap, bone, timeInSeconds, pos, rot, scl, isLooping, loopBlendDuration);
		const mat4 localTransform = glm::translate(mat4(1.0f), pos) * glm::toMat4(rot) * glm::scale(mat4(1.0f), scl);
		if (bone.ParentIndex >= 0 && static_cast<uint32>(bone.ParentIndex) < boneCount)
			combinedTransforms[i] = combinedTransforms[bone.ParentIndex] * localTransform;
		else
			combinedTransforms[i] = localTransform;
		outBoneMatrices[i] = combinedTransforms[i] * bone.OffsetMatrix;
	}
}
void AnimationEvaluator::EvaluateBlendedSkinningMatrices(const Animation& clipA, f32 timeA, const Animation& clipB, f32 timeB, f32 alpha, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices, bool isLoopingA, bool isLoopingB)
{
	const auto& bones = skeleton.GetBones();
	const uint32 boneCount = static_cast<uint32>(bones.size());
	if (boneCount == 0) { outBoneMatrices.clear(); return; }
	if (outBoneMatrices.size() != boneCount) outBoneMatrices.resize(boneCount, glm::identity<mat4>());
	const f32 clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
	if (clampedAlpha <= 0.0f) { EvaluateSkinningMatrices(clipA, skeleton, timeA, outBoneMatrices, isLoopingA); return; }
	if (clampedAlpha >= 1.0f) { EvaluateSkinningMatrices(clipB, skeleton, timeB, outBoneMatrices, isLoopingB); return; }
	unordered_map<wstring, const AnimationTrack*> trackMapA;
	for (const auto& track : clipA.GetTracks()) trackMapA[track.TargetBoneName] = &track;
	unordered_map<wstring, const AnimationTrack*> trackMapB;
	for (const auto& track : clipB.GetTracks()) trackMapB[track.TargetBoneName] = &track;
	std::vector<mat4> combinedTransforms(boneCount, glm::identity<mat4>());
	for (uint32 i = 0; i < boneCount; ++i)
	{
		const auto& bone = bones[i];
		vec3 posA, posB; quat rotA, rotB; vec3 sclA, sclB;
		SampleLocalTRS(clipA, trackMapA, bone, timeA, posA, rotA, sclA, isLoopingA);
		SampleLocalTRS(clipB, trackMapB, bone, timeB, posB, rotB, sclB, isLoopingB);
		const vec3 blendedPos = glm::mix(posA, posB, clampedAlpha);
		const quat blendedRot = glm::normalize(glm::slerp(rotA, rotB, clampedAlpha));
		const vec3 blendedScl = glm::mix(sclA, sclB, clampedAlpha);
		const mat4 localTransform = glm::translate(mat4(1.0f), blendedPos) * glm::toMat4(blendedRot) * glm::scale(mat4(1.0f), blendedScl);
		if (bone.ParentIndex >= 0 && static_cast<uint32>(bone.ParentIndex) < boneCount)
			combinedTransforms[i] = combinedTransforms[bone.ParentIndex] * localTransform;
		else
			combinedTransforms[i] = localTransform;
		outBoneMatrices[i] = combinedTransforms[i] * bone.OffsetMatrix;
	}
}
void AnimationEvaluator::EvaluateMaskedSkinningMatrices(const Animation& baseClip, f32 baseTime, const Animation& layerClip, f32 layerTime, const std::vector<float>& boneMaskWeights, f32 layerAlpha, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices, bool isLoopingBase, bool isLoopingLayer)
{
	const auto& bones = skeleton.GetBones();
	const uint32 boneCount = static_cast<uint32>(bones.size());
	if (boneCount == 0) { outBoneMatrices.clear(); return; }
	if (outBoneMatrices.size() != boneCount) outBoneMatrices.resize(boneCount, glm::identity<mat4>());

	const f32 clampedLayerAlpha = std::clamp(layerAlpha, 0.0f, 1.0f);
	if (clampedLayerAlpha <= 0.0f) { EvaluateSkinningMatrices(baseClip, skeleton, baseTime, outBoneMatrices, isLoopingBase); return; }

	unordered_map<wstring, const AnimationTrack*> trackMapBase;
	for (const auto& track : baseClip.GetTracks()) trackMapBase[track.TargetBoneName] = &track;
	unordered_map<wstring, const AnimationTrack*> trackMapLayer;
	for (const auto& track : layerClip.GetTracks()) trackMapLayer[track.TargetBoneName] = &track;

	std::vector<mat4> combinedTransforms(boneCount, glm::identity<mat4>());
	for (uint32 i = 0; i < boneCount; ++i)
	{
		const auto& bone = bones[i];
		const float maskWeight = (i < boneMaskWeights.size()) ? boneMaskWeights[i] : 0.0f;
		const float finalWeight = std::clamp(maskWeight * clampedLayerAlpha, 0.0f, 1.0f);

		vec3 pos; quat rot; vec3 scl;
		if (finalWeight <= 0.0f)
		{
			SampleLocalTRS(baseClip, trackMapBase, bone, baseTime, pos, rot, scl, isLoopingBase);
		}
		else if (finalWeight >= 1.0f)
		{
			SampleLocalTRS(layerClip, trackMapLayer, bone, layerTime, pos, rot, scl, isLoopingLayer);
		}
		else
		{
			vec3 posA, posB; quat rotA, rotB; vec3 sclA, sclB;
			SampleLocalTRS(baseClip, trackMapBase, bone, baseTime, posA, rotA, sclA, isLoopingBase);
			SampleLocalTRS(layerClip, trackMapLayer, bone, layerTime, posB, rotB, sclB, isLoopingLayer);
			pos = glm::mix(posA, posB, finalWeight);
			rot = glm::normalize(glm::slerp(rotA, rotB, finalWeight));
			scl = glm::mix(sclA, sclB, finalWeight);
		}

		const mat4 localTransform = glm::translate(mat4(1.0f), pos) * glm::toMat4(rot) * glm::scale(mat4(1.0f), scl);
		if (bone.ParentIndex >= 0 && static_cast<uint32>(bone.ParentIndex) < boneCount)
			combinedTransforms[i] = combinedTransforms[bone.ParentIndex] * localTransform;
		else
			combinedTransforms[i] = localTransform;

		outBoneMatrices[i] = combinedTransforms[i] * bone.OffsetMatrix;
	}
}