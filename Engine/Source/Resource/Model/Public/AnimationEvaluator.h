#pragma once

#include "Engine_Includes.h"
#include "Animation.h"
#include "Skeleton.h"

BEGIN(Engine)

class ENGINE_API AnimationEvaluator
{
public:
	// 현재 시간(초)에 맞춰 스켈레톤의 각 뼈에 대한 최종 스키닝 행렬(Combined * Offset)을 계산합니다.
	static void EvaluateSkinningMatrices(const Animation& animation, const Skeleton& skeleton, f32 timeInSeconds, std::vector<mat4>& outBoneMatrices);

	// 두 애니메이션 클립의 포즈를 alpha(0.0~1.0) 가중치로 선형/구면선형 보간하여 최종 스키닝 행렬을 계산합니다.
	static void EvaluateBlendedSkinningMatrices(const Animation& clipA, f32 timeA, const Animation& clipB, f32 timeB, f32 alpha, const Skeleton& skeleton, std::vector<mat4>& outBoneMatrices);

private:
	static vec3 InterpolatePosition(const vector<VectorKeyframe>& keys, f64 animTime);
	static quat InterpolateRotation(const vector<QuaternionKeyframe>& keys, f64 animTime);
	static vec3 InterpolateScale(const vector<VectorKeyframe>& keys, f64 animTime);

	// 단일 클립에서 특정 시간의 뼈 로컬 TRS 샘플링 헬퍼
	static void SampleLocalTRS(const Animation& animation, const unordered_map<wstring, const AnimationTrack*>& trackMap, const Bone& bone, f32 timeInSeconds, vec3& outPos, quat& outRot, vec3& outScale);
};

END