#pragma once

#include "Engine_Includes.h"
#include "ComponentAnnotations.h"
#include "Skeleton.h"
#include "AnimationEvaluator.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Engine
{
	struct TwoBoneIKSolution
	{
		vec3 newMidPos = vec3(0.0f);
		vec3 newEndPos = vec3(0.0f);
		quat rootRotationOffset = glm::identity<quat>();
		quat midRotationOffset = glm::identity<quat>();
		bool solved = false;
	};

	struct LookAtIKSolution
	{
		quat rotationOffset = glm::identity<quat>();
		bool solved = false;
	};

	namespace AnimationIKInternal
	{
		inline quat RotationBetweenVectors(const vec3& from, const vec3& to)
		{
			const float lenFrom = glm::length(from);
			const float lenTo = glm::length(to);
			if (lenFrom < 1e-6f || lenTo < 1e-6f)
				return glm::identity<quat>();

			const vec3 u = from / lenFrom;
			const vec3 v = to / lenTo;

			const float dotVal = glm::clamp(glm::dot(u, v), -1.0f, 1.0f);
			if (dotVal > 0.999999f)
				return glm::identity<quat>();

			if (dotVal < -0.999999f)
			{
				vec3 ortho = std::abs(u.x) < 0.8f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
				vec3 axis = glm::normalize(glm::cross(u, ortho));
				return glm::angleAxis(glm::pi<float>(), axis);
			}

			const float angle = std::acos(dotVal);
			const vec3 axis = glm::normalize(glm::cross(u, v));
			return glm::angleAxis(angle, axis);
		}
	}

	inline TwoBoneIKSolution SolveTwoBoneIK(
		const vec3& rootPos,
		const vec3& midPos,
		const vec3& endPos,
		const vec3& targetPos,
		const vec3& poleVector,
		f32 weight = 1.0f,
		f32 softDistanceRatio = 0.95f)
	{
		TwoBoneIKSolution solution;
		const vec3 vRootMid = midPos - rootPos;
		const vec3 vMidEnd = endPos - midPos;
		const float len1 = glm::length(vRootMid);
		const float len2 = glm::length(vMidEnd);

		if (len1 < 1e-4f || len2 < 1e-4f)
			return solution;

		const float maxDist = len1 + len2;
		const float minDist = std::abs(len1 - len2);

		const vec3 vRootTarget = targetPos - rootPos;
		const float dist = glm::length(vRootTarget);
		const vec3 dir = (dist > 1e-4f) ? (vRootTarget / dist) : glm::normalize(vRootMid);

		// Soft distance clamping near max extension
		const float ratio = std::clamp(softDistanceRatio, 0.5f, 0.999f);
		const float softDist = maxDist * ratio;
		float clampedDist = dist;
		if (dist > softDist)
		{
			const float da = maxDist - softDist;
			clampedDist = softDist + da * (1.0f - std::exp(-(dist - softDist) / da));
		}
		clampedDist = glm::clamp(clampedDist, minDist + 1e-4f, maxDist - 1e-4f);
		const vec3 effectiveTarget = rootPos + dir * clampedDist;

		// Law of cosines for triangle angles
		const float cosAlpha = glm::clamp((len1 * len1 + clampedDist * clampedDist - len2 * len2) / (2.0f * len1 * clampedDist), -1.0f, 1.0f);
		const float cosBeta = glm::clamp((len1 * len1 + len2 * len2 - clampedDist * clampedDist) / (2.0f * len1 * len2), -1.0f, 1.0f);
		const float angleAlpha = std::acos(cosAlpha);

		// Determine bend plane using pole vector
		const vec3 poleDir = poleVector - rootPos;
		vec3 planeNormal = glm::cross(dir, poleDir);
		if (glm::length2(planeNormal) < 1e-6f)
		{
			planeNormal = glm::cross(dir, midPos - rootPos);
			if (glm::length2(planeNormal) < 1e-6f)
			{
				planeNormal = (std::abs(dir.y) < 0.9f) ? glm::cross(dir, vec3(0.0f, 1.0f, 0.0f)) : glm::cross(dir, vec3(1.0f, 0.0f, 0.0f));
			}
		}
		planeNormal = glm::normalize(planeNormal);
		const vec3 bendDir = glm::normalize(glm::cross(planeNormal, dir));

		// Solved positions preserving exact bone lengths
		const vec3 solvedMid = rootPos + dir * (len1 * std::cos(angleAlpha)) + bendDir * (len1 * std::sin(angleAlpha));
		const vec3 solvedEnd = effectiveTarget;

		// Rotation offsets
		const vec3 vRootSolvedMid = solvedMid - rootPos;
		const quat rotRoot = AnimationIKInternal::RotationBetweenVectors(vRootMid, vRootSolvedMid);
		const vec3 rotatedMidEnd = rotRoot * vMidEnd;
		const vec3 vSolvedMidEnd = solvedEnd - solvedMid;
		const quat rotMid = AnimationIKInternal::RotationBetweenVectors(rotatedMidEnd, vSolvedMidEnd);

		const float blendWeight = std::clamp(weight, 0.0f, 1.0f);
		solution.rootRotationOffset = glm::slerp(glm::identity<quat>(), rotRoot, blendWeight);
		solution.midRotationOffset = glm::slerp(glm::identity<quat>(), rotMid, blendWeight);

		solution.newMidPos = rootPos + solution.rootRotationOffset * vRootMid;
		solution.newEndPos = solution.newMidPos + (solution.rootRotationOffset * solution.midRotationOffset) * vMidEnd;
		solution.solved = true;
		return solution;
	}

	inline LookAtIKSolution SolveLookAtIK(
		const vec3& bonePos,
		const quat& boneRot,
		const vec3& targetPos,
		const vec3& localForward = vec3(0.0f, 0.0f, 1.0f),
		const vec3& localUp = vec3(0.0f, 1.0f, 0.0f),
		float maxYaw = 1.22173f, // ~70 deg
		float maxPitch = 0.872665f, // ~50 deg
		float weight = 1.0f)
	{
		LookAtIKSolution solution;
		const vec3 toTarget = targetPos - bonePos;
		const float dist = glm::length(toTarget);
		if (dist < 1e-4f)
		{
			solution.solved = true;
			return solution;
		}

		const vec3 worldTargetDir = toTarget / dist;
		const vec3 localTargetDir = glm::inverse(boneRot) * worldTargetDir;

		const vec3 fwd = glm::normalize(localForward);
		vec3 up = glm::normalize(localUp);
		const vec3 right = glm::normalize(glm::cross(up, fwd));
		up = glm::cross(fwd, right);

		const float yaw = std::atan2(glm::dot(localTargetDir, right), glm::dot(localTargetDir, fwd));
		const float pitch = std::asin(glm::clamp(glm::dot(localTargetDir, up), -1.0f, 1.0f));

		const float clampedYaw = glm::clamp(yaw, -std::abs(maxYaw), std::abs(maxYaw));
		const float clampedPitch = glm::clamp(pitch, -std::abs(maxPitch), std::abs(maxPitch));

		const quat qYaw = glm::angleAxis(clampedYaw, up);
		const quat qPitch = glm::angleAxis(-clampedPitch, right);
		const vec3 clampedLocalDir = qYaw * qPitch * fwd;
		const vec3 clampedWorldDir = boneRot * clampedLocalDir;

		const vec3 currentWorldFwd = boneRot * fwd;
		const quat rotOffset = AnimationIKInternal::RotationBetweenVectors(currentWorldFwd, clampedWorldDir);

		const float blendWeight = std::clamp(weight, 0.0f, 1.0f);
		solution.rotationOffset = glm::slerp(glm::identity<quat>(), rotOffset, blendWeight);
		solution.solved = true;
		return solution;
	}

	STRUCT()
	struct TwoBoneIKChain
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string rootBoneName;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string midBoneName;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string endBoneName;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec3 targetPosition = vec3(0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec3 poleVector = vec3(0.0f, 0.0f, 1.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING)
		float weight = 1.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		bool targetInWorldSpace = true;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		bool enabled = true;
	};

	STRUCT()
	struct LookAtIKTarget
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::string boneName;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec3 targetPosition = vec3(0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec3 forwardAxis = vec3(0.0f, 0.0f, 1.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec3 upAxis = vec3(0.0f, 1.0f, 0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING)
		float maxYawAngle = 1.22173f;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		float maxPitchAngle = 0.872665f;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		float weight = 1.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		bool targetInWorldSpace = true;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		bool enabled = true;
	};

	STRUCT(COMPONENT, CATEGORY("CATEGORY_RENDER"))
	struct AnimationIKComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, ALLOW_MISSING)
		bool enabled = true;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::vector<TwoBoneIKChain> twoBoneChains;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		std::vector<LookAtIKTarget> lookAtTargets;
	};

	inline void ApplyAnimationIK(
		const Skeleton& skeleton,
		std::vector<mat4>& localPose,
		std::vector<mat4>& globalPose,
		const AnimationIKComponent& ikComp,
		const mat4& entityWorldMatrix)
	{
		const auto& bones = skeleton.GetBones();
		if (bones.empty() || !ikComp.enabled)
			return;

		if (localPose.size() != bones.size())
			return;

		if (globalPose.size() != bones.size())
			globalPose.resize(bones.size());

		// Forward kinematics: build model-space global pose from local matrices
		for (size_t i = 0; i < bones.size(); ++i)
		{
			const int32 parent = bones[i].ParentIndex;
			if (parent >= 0 && static_cast<size_t>(parent) < i)
				globalPose[i] = globalPose[parent] * localPose[i];
			else
				globalPose[i] = localPose[i];
		}

		const mat4 invEntityWorld = glm::inverse(entityWorldMatrix);

		// 1. Solve Two-Bone IK chains
		for (const auto& chain : ikComp.twoBoneChains)
		{
			if (!chain.enabled || chain.weight <= 0.0f) continue;

			const std::wstring wRoot(chain.rootBoneName.begin(), chain.rootBoneName.end());
			const std::wstring wMid(chain.midBoneName.begin(), chain.midBoneName.end());
			const std::wstring wEnd(chain.endBoneName.begin(), chain.endBoneName.end());

			const uint32 rootIdx = skeleton.GetBoneIndex(wRoot);
			const uint32 midIdx = skeleton.GetBoneIndex(wMid);
			const uint32 endIdx = skeleton.GetBoneIndex(wEnd);

			if (rootIdx >= bones.size() || midIdx >= bones.size() || endIdx >= bones.size())
				continue;

			const vec3 rootPos = vec3(globalPose[rootIdx][3]);
			const vec3 midPos = vec3(globalPose[midIdx][3]);
			const vec3 endPos = vec3(globalPose[endIdx][3]);

			vec3 targetPos = chain.targetPosition;
			vec3 polePos = chain.poleVector;
			if (chain.targetInWorldSpace)
			{
				targetPos = vec3(invEntityWorld * vec4(chain.targetPosition, 1.0f));
				polePos = vec3(invEntityWorld * vec4(chain.poleVector, 1.0f));
			}

			const auto sol = SolveTwoBoneIK(rootPos, midPos, endPos, targetPos, polePos, chain.weight);
			if (!sol.solved) continue;

			// Apply root rotation offset
			const mat4 rootRotM = glm::toMat4(sol.rootRotationOffset);
			globalPose[rootIdx] = rootRotM * globalPose[rootIdx];

			const int32 rootParent = bones[rootIdx].ParentIndex;
			if (rootParent >= 0 && static_cast<size_t>(rootParent) < rootIdx)
				localPose[rootIdx] = glm::inverse(globalPose[rootParent]) * globalPose[rootIdx];
			else
				localPose[rootIdx] = globalPose[rootIdx];

			// Update mid global from updated root
			globalPose[midIdx] = globalPose[rootIdx] * localPose[midIdx];

			// Apply mid rotation offset
			const mat4 midRotM = glm::toMat4(sol.midRotationOffset);
			globalPose[midIdx] = midRotM * globalPose[midIdx];
			localPose[midIdx] = glm::inverse(globalPose[rootIdx]) * globalPose[midIdx];

			// Update subsequent descendant hierarchy
			for (size_t i = midIdx + 1; i < bones.size(); ++i)
			{
				const int32 p = bones[i].ParentIndex;
				if (p >= 0 && static_cast<size_t>(p) < i)
					globalPose[i] = globalPose[p] * localPose[i];
			}
		}

		// 2. Solve Look-At IK targets
		for (const auto& lookAt : ikComp.lookAtTargets)
		{
			if (!lookAt.enabled || lookAt.weight <= 0.0f) continue;

			const std::wstring wBone(lookAt.boneName.begin(), lookAt.boneName.end());
			const uint32 boneIdx = skeleton.GetBoneIndex(wBone);
			if (boneIdx >= bones.size()) continue;

			const vec3 bonePos = vec3(globalPose[boneIdx][3]);
			const quat boneRot = glm::quat_cast(globalPose[boneIdx]);

			vec3 targetPos = lookAt.targetPosition;
			if (lookAt.targetInWorldSpace)
			{
				targetPos = vec3(invEntityWorld * vec4(lookAt.targetPosition, 1.0f));
			}

			const auto sol = SolveLookAtIK(bonePos, boneRot, targetPos, lookAt.forwardAxis, lookAt.upAxis,
				lookAt.maxYawAngle, lookAt.maxPitchAngle, lookAt.weight);
			if (!sol.solved) continue;

			const mat4 rotM = glm::toMat4(sol.rotationOffset);
			globalPose[boneIdx] = rotM * globalPose[boneIdx];

			const int32 p = bones[boneIdx].ParentIndex;
			if (p >= 0 && static_cast<size_t>(p) < boneIdx)
				localPose[boneIdx] = glm::inverse(globalPose[p]) * globalPose[boneIdx];
			else
				localPose[boneIdx] = globalPose[boneIdx];

			for (size_t i = boneIdx + 1; i < bones.size(); ++i)
			{
				const int32 parent = bones[i].ParentIndex;
				if (parent >= 0 && static_cast<size_t>(parent) < i)
					globalPose[i] = globalPose[parent] * localPose[i];
			}
		}
	}

	inline void ApplyAnimationIK(
		const Skeleton& skeleton,
		std::vector<BoneLocalPose>& localPose,
		std::vector<mat4>& skinningMatrices,
		const AnimationIKComponent& ikComp,
		const mat4& entityWorldMatrix)
	{
		const auto& bones = skeleton.GetBones();
		if (bones.empty() || localPose.size() != bones.size() || !ikComp.enabled)
			return;

		std::vector<mat4> localMats(bones.size());
		for (size_t i = 0; i < bones.size(); ++i)
		{
			localMats[i] = glm::translate(mat4(1.0f), localPose[i].Position) *
				glm::toMat4(localPose[i].Rotation) * glm::scale(mat4(1.0f), localPose[i].Scale);
		}
		std::vector<mat4> globalMats(bones.size());

		ApplyAnimationIK(skeleton, localMats, globalMats, ikComp, entityWorldMatrix);

		for (size_t i = 0; i < bones.size(); ++i)
		{
			localPose[i].Rotation = glm::quat_cast(localMats[i]);
		}
		AnimationEvaluator::BuildSkinningMatrices(skeleton, localPose, skinningMatrices);
	}
}
