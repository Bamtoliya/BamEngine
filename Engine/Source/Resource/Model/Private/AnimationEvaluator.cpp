#include "AnimationEvaluator.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

vec3 AnimationEvaluator::InterpolatePosition(const vector<VectorKeyframe>& keys, f64 animTime)
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

quat AnimationEvaluator::InterpolateRotation(const vector<QuaternionKeyframe>& keys, f64 animTime)
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

vec3 AnimationEvaluator::InterpolateScale(const vector<VectorKeyframe>& keys, f64 animTime)
{
	if (keys.empty()) return vec3(1.0f);
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

void AnimationEvaluator::SampleLocalTRS(const Animation& animation, const unordered_map<wstring, const AnimationTrack*>& trackMap, const Bone& bone, f32 timeInSeconds, vec3& outPos, quat& outRot, vec3& outScale, bool isLooping)
{
	const f64 ticksPerSecond = animation.GetTicksPerSecond() > 0.0 ? animation.GetTicksPerSecond() : 25.0;
	f64 animTimeTicks = static_cast<f64>(timeInSeconds) * ticksPerSecond;
	const f64 durationTicks = animation.GetDuration();
	if (durationTicks > 0.0)
	{
		animTimeTicks = isLooping ? std::fmod(animTimeTicks, durationTicks) : std::clamp(animTimeTicks, 0.0, durationTicks);
	}
	const vec3 bindPos = vec3(bone.LocalTransform[3]);
	const quat bindRot = glm::toQuat(bone.LocalTransform);
	const vec3 bindScale = vec3(glm::length(vec3(bone.LocalTransform[0])), glm::length(vec3(bone.LocalTransform[1])), glm::length(vec3(bone.LocalTransform[2])));
	const auto it = trackMap.find(bone.Name);
	if (it != trackMap.end())
	{
		const auto* track = it->second;
		outPos = !track->PositionKeyframes.empty() ? InterpolatePosition(track->PositionKeyframes, animTimeTicks) : bindPos;
		outRot = !track->RotationKeyframes.empty() ? InterpolateRotation(track->RotationKeyframes, animTimeTicks) : bindRot;
		outScale = !track->ScaleKeyframes.empty() ? InterpolateScale(track->ScaleKeyframes, animTimeTicks) : bindScale;
	}
	else
	{
		outPos = bindPos;
		outRot = bindRot;
		outScale = bindScale;
	}
}
void AnimationEvaluator::EvaluateSkinningMatrices(const Animation& animation, const Skeleton& skeleton, f32 timeInSeconds, std::vector<mat4>& outBoneMatrices, bool isLooping)
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
		SampleLocalTRS(animation, trackMap, bone, timeInSeconds, pos, rot, scl, isLooping);
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