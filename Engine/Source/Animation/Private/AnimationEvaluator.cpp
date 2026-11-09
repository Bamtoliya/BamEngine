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

void AnimationEvaluator::EvaluateLocalPose(const Animation& animation, const Skeleton& skeleton,
	f32 timeInSeconds, std::vector<BoneLocalPose>& outPose, bool isLooping, float loopBlendDuration)
{
	const auto& bones = skeleton.GetBones();
	outPose.resize(bones.size());

	unordered_map<wstring, const AnimationTrack*> trackMap;
	for (const auto& track : animation.GetTracks())
		trackMap[track.TargetBoneName] = &track;

	for (size_t i = 0; i < bones.size(); ++i)
	{
		auto& pose = outPose[i];
		SampleLocalTRS(animation, trackMap, bones[i], timeInSeconds,
			pose.Position, pose.Rotation, pose.Scale, isLooping, loopBlendDuration);
	}
}

BoneLocalPose AnimationEvaluator::BlendBoneLocalPose(const BoneLocalPose& from, const BoneLocalPose& to, f32 weight)
{
	return {
		glm::mix(from.Position, to.Position, weight),
		glm::normalize(glm::slerp(from.Rotation, to.Rotation, weight)),
		glm::mix(from.Scale, to.Scale, weight)
	};
}

void AnimationEvaluator::EvaluateBlendedLocalPose(const Animation& clipA, f32 timeA,
	const Animation& clipB, f32 timeB, f32 alpha, const Skeleton& skeleton,
	std::vector<BoneLocalPose>& outPose, bool isLoopingA, bool isLoopingB)
{
	const f32 weight = std::clamp(alpha, 0.0f, 1.0f);
	if (weight <= 0.0f)
	{
		EvaluateLocalPose(clipA, skeleton, timeA, outPose, isLoopingA);
		return;
	}
	if (weight >= 1.0f)
	{
		EvaluateLocalPose(clipB, skeleton, timeB, outPose, isLoopingB);
		return;
	}

	const auto& bones = skeleton.GetBones();
	outPose.resize(bones.size());

	unordered_map<wstring, const AnimationTrack*> trackMapA;
	unordered_map<wstring, const AnimationTrack*> trackMapB;
	for (const auto& track : clipA.GetTracks())
		trackMapA[track.TargetBoneName] = &track;
	for (const auto& track : clipB.GetTracks())
		trackMapB[track.TargetBoneName] = &track;

	for (size_t i = 0; i < bones.size(); ++i)
	{
		BoneLocalPose a, b;
		SampleLocalTRS(clipA, trackMapA, bones[i], timeA,
			a.Position, a.Rotation, a.Scale, isLoopingA);
		SampleLocalTRS(clipB, trackMapB, bones[i], timeB,
			b.Position, b.Rotation, b.Scale, isLoopingB);

		outPose[i] = BlendBoneLocalPose(a, b, weight);
	}
}

void AnimationEvaluator::ApplyMaskedLocalPose(const Animation& layerClip, f32 layerTime,
	const std::vector<float>& boneMaskWeights, f32 layerAlpha, const Skeleton& skeleton,
	std::vector<BoneLocalPose>& inOutPose, bool isLoopingLayer)
{
	const auto& bones = skeleton.GetBones();
	if (inOutPose.size() != bones.size())
	{
		inOutPose.clear();
		return;
	}

	const f32 alpha = std::clamp(layerAlpha, 0.0f, 1.0f);
	if (alpha <= 0.0f) return;

	unordered_map<wstring, const AnimationTrack*> trackMap;
	for (const auto& track : layerClip.GetTracks())
		trackMap[track.TargetBoneName] = &track;

	for (size_t i = 0; i < bones.size(); ++i)
	{
		const float mask = i < boneMaskWeights.size() ? boneMaskWeights[i] : 0.0f;
		const float weight = std::clamp(mask * alpha, 0.0f, 1.0f);
		if (weight <= 0.0f) continue;

		BoneLocalPose layer;
		SampleLocalTRS(layerClip, trackMap, bones[i], layerTime,
			layer.Position, layer.Rotation, layer.Scale, isLoopingLayer);

		inOutPose[i] = BlendBoneLocalPose(inOutPose[i], layer, weight);
	}
}

void AnimationEvaluator::BuildSkinningMatrices(const Skeleton& skeleton,
	const std::vector<BoneLocalPose>& localPose, std::vector<mat4>& outBoneMatrices)
{
	const auto& bones = skeleton.GetBones();
	if (localPose.size() != bones.size())
	{
		outBoneMatrices.clear();
		return;
	}

	outBoneMatrices.resize(bones.size());

	// 첫 순회에서는 오프셋을 적용하지 않은 계층 변환을 보관한다.
	for (size_t i = 0; i < bones.size(); ++i)
	{
		const auto& pose = localPose[i];
		const mat4 local = glm::translate(mat4(1.0f), pose.Position) *
			glm::toMat4(pose.Rotation) * glm::scale(mat4(1.0f), pose.Scale);
		const int32 parent = bones[i].ParentIndex;
		if (parent >= 0 && static_cast<size_t>(parent) < i)
			outBoneMatrices[i] = outBoneMatrices[parent] * local;
		else
			outBoneMatrices[i] = local;
	}

	// 자식의 계층 변환 계산이 끝난 뒤 스키닝 오프셋을 적용한다.
	for (size_t i = 0; i < bones.size(); ++i)
		outBoneMatrices[i] *= bones[i].OffsetMatrix;
}

void AnimationEvaluator::EvaluateBindPoseSkinningMatrices(const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices)
{
	const auto& bones = skeleton.GetBones();
	outBoneMatrices.resize(bones.size());

	for (size_t i = 0; i < bones.size(); ++i)
	{
		const int32 parent = bones[i].ParentIndex;
		if (parent >= 0 && static_cast<size_t>(parent) < i)
			outBoneMatrices[i] = outBoneMatrices[parent] * bones[i].LocalTransform;
		else
			outBoneMatrices[i] = bones[i].LocalTransform;
	}

	for (size_t i = 0; i < bones.size(); ++i)
		outBoneMatrices[i] *= bones[i].OffsetMatrix;
}

void AnimationEvaluator::EvaluateSkinningMatrices(const Animation& animation, const Skeleton& skeleton,
	f32 timeInSeconds, std::vector<mat4>& outBoneMatrices, bool isLooping, float loopBlendDuration)
{
	std::vector<BoneLocalPose> pose;
	EvaluateLocalPose(animation, skeleton, timeInSeconds, pose, isLooping, loopBlendDuration);
	BuildSkinningMatrices(skeleton, pose, outBoneMatrices);
}

void AnimationEvaluator::EvaluateBlendedSkinningMatrices(const Animation& clipA, f32 timeA,
	const Animation& clipB, f32 timeB, f32 alpha, const Skeleton& skeleton,
	std::vector<mat4>& outBoneMatrices, bool isLoopingA, bool isLoopingB)
{
	std::vector<BoneLocalPose> pose;
	EvaluateBlendedLocalPose(clipA, timeA, clipB, timeB, alpha, skeleton, pose, isLoopingA, isLoopingB);
	BuildSkinningMatrices(skeleton, pose, outBoneMatrices);
}

void AnimationEvaluator::EvaluateMaskedSkinningMatrices(const Animation& baseClip, f32 baseTime,
	const Animation& layerClip, f32 layerTime, const std::vector<float>& boneMaskWeights,
	f32 layerAlpha, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices,
	bool isLoopingBase, bool isLoopingLayer)
{
	std::vector<BoneLocalPose> pose;
	EvaluateLocalPose(baseClip, skeleton, baseTime, pose, isLoopingBase);
	ApplyMaskedLocalPose(layerClip, layerTime, boneMaskWeights, layerAlpha, skeleton, pose, isLoopingLayer);
	BuildSkinningMatrices(skeleton, pose, outBoneMatrices);
}