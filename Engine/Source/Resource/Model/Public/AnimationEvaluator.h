#pragma once

#include "Engine_Includes.h"
#include "Animation.h"
#include "Skeleton.h"

BEGIN(Engine)
struct BoneLocalPose
{
	vec3 Position = vec3(0.0f);
	quat Rotation = glm::identity<quat>();
	vec3 Scale = vec3(1.0f);
};
class ENGINE_API AnimationEvaluator
{
public:
	// 현재 시간(초)에 맞춰 스켈레톤의 각 뼈에 대한 최종 스키닝 행렬(Combined * Offset)을 계산합니다.
	static void EvaluateSkinningMatrices(const Animation& animation, const Skeleton& skeleton, f32 timeInSeconds, std::vector<mat4>& outBoneMatrices, bool isLooping = true, float loopBlendDuration = 0.05f);
	// 스켈레톤의 바인드 포즈(T-포즈)에 대한 최종 스키닝 행렬(GlobalBind * Offset)을 계산합니다.
	static void EvaluateBindPoseSkinningMatrices(const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices);
	// 두 애니메이션 클립의 포즈를 alpha(0.0~1.0) 가중치로 선형/구면선형 보간하여 최종 스키닝 행렬을 계산합니다.
	static void EvaluateBlendedSkinningMatrices(const Animation& clipA, f32 timeA, const Animation& clipB, f32 timeB, f32 alpha, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices, bool isLoopingA = true, bool isLoopingB = true);
	// 베이스 클립과 오버레이 레이어 클립을 본 마스크 가중치(0.0~1.0) 및 레이어 알파로 합성하여 최종 스키닝 행렬을 계산합니다.
	static void EvaluateMaskedSkinningMatrices(const Animation& baseClip, f32 baseTime, const Animation& layerClip, f32 layerTime, const std::vector<float>& boneMaskWeights, f32 layerAlpha, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices, bool isLoopingBase = true, bool isLoopingLayer = true);

	static void EvaluateLocalPose(const Animation& animation, const Skeleton& skeleton, f32 timeInSeconds, std::vector<BoneLocalPose>& outPose, bool isLooping = true, float loopBlendDuration = 0.05f);
	static BoneLocalPose BlendBoneLocalPose(const BoneLocalPose& from, const BoneLocalPose& to, f32 weight);

	static void EvaluateBlendedLocalPose(const Animation& clipA, f32 timeA, const Animation& clipB, f32 timeB, f32 alpha, const Skeleton& skeleton, std::vector<BoneLocalPose>& outPose, bool isLoopingA = true, bool isLoopingB = true);

	static void ApplyMaskedLocalPose(const Animation& layerClip, f32 layerTime, const std::vector<float>& boneMaskWeights, f32 layerAlpha, const Skeleton& skeleton, std::vector<BoneLocalPose>& inOutPose, bool isLoopingLayer = true);

	static void BuildSkinningMatrices(const Skeleton& skeleton, const std::vector<BoneLocalPose>& localPose, std::vector<mat4>& outBoneMatrices);
private:
	static vec3 InterpolatePosition(const vector<VectorKeyframe>& keys, f64 animTime, f64 duration = 0.0, bool isLooping = false);
	static quat InterpolateRotation(const vector<QuaternionKeyframe>& keys, f64 animTime, f64 duration = 0.0, bool isLooping = false);
	static vec3 InterpolateScale(const vector<VectorKeyframe>& keys, f64 animTime, f64 duration = 0.0, bool isLooping = false);
	// 단일 클립에서 특정 시간의 뼈 로컬 TRS 샘플링 헬퍼
	static void SampleLocalTRS(const Animation& animation, const unordered_map<wstring, const AnimationTrack*>& trackMap, const Bone& bone, f32 timeInSeconds, vec3& outPos, quat& outRot, vec3& outScale, bool isLooping = true, float loopBlendDuration = 0.05f);
};

END