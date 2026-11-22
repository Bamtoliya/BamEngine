#include "BlendTree2D.h"
#include "ResourceManager.h"
#include <cmath>
#include <iostream>
#include <limits>
#include "AnimatorStateMachine.h"
#include "ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "AnimationIK.h"
#include "RootMotion.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "PhysicsSystem.h"
#include "GraphGameplayLibrary.h"

int main()
{
	using namespace Engine;

	const auto fail = [](const char* message)
		{
			std::cerr << message << '\n';
			return 1;
		};

	if (!ResourceManager::Create()) return fail("Resource manager initialization failed.");
	struct ResourceScope
	{
		~ResourceScope() { ResourceManager::Destroy(); }
	} scope;

	auto& manager = ResourceManager::Get();
	std::array<ResourceHandle<Animation>, 4> clips;
	for (size_t i = 0; i < clips.size(); ++i)
	{
		AnimationCreateDesc desc{};
		desc.Key = L"BlendTree2DCheck_" + std::to_wstring(i);
		desc.Path = desc.Key;
		desc.Name = desc.Key;
		desc.Duration = 1.0;
		desc.TicksPerSecond = 1.0;

		AnimationTrack track{};
		track.TargetBoneName = L"Root";
		VectorKeyframe key{};
		key.Value = vec3(static_cast<float>(i) * 10.0f, 0.0f, 0.0f);
		track.PositionKeyframes.push_back(key);
		desc.Tracks.push_back(track);

		auto* clip = Animation::Create(&desc);
		if (!clip) return fail("Animation creation failed.");

		clips[i] = manager.AddResource<Animation>(desc.Key, clip);
		if (!clips[i].IsValid()) return fail("Animation registration failed.");
	}

	BlendTree2D tree;
	tree.minX = -2.0f;
	tree.maxX = 2.0f;
	tree.minY = 10.0f;
	tree.maxY = 20.0f;
	tree.clip00 = clips[0];
	tree.clip10 = clips[1];
	tree.clip01 = clips[2];
	tree.clip11 = clips[3];

	std::array<BlendClipWeight2D, 4> output{};
	const auto expect = [&](float x, float y, const std::array<float, 4>& expected)
		{
			if (!tree.Evaluate(x, y, output)) return false;

			for (size_t i = 0; i < output.size(); ++i)
			{
				const auto* clip = expected[i] > 0.0f ? clips[i].Get() : nullptr;
				if (output[i].Clip != clip || std::abs(output[i].Weight - expected[i]) > 1e-6f)
					return false;
			}
			return true;
		};

	if (!expect(0.0f, 15.0f, { 0.25f, 0.25f, 0.25f, 0.25f }))
		return fail("Center weights are incorrect.");

	if (!expect(-1.0f, 17.5f, { 0.1875f, 0.0625f, 0.5625f, 0.1875f }))
		return fail("Asymmetric weights or corner order are incorrect.");

	SkeletonCreateDesc skeletonDesc{};
	skeletonDesc.Key = L"BlendTree2DCheck_Skeleton";
	Bone root{};
	root.Name = L"Root";
	skeletonDesc.Bones.push_back(root);

	auto* skeletonResource = Skeleton::Create(&skeletonDesc);
	if (!skeletonResource) return fail("Skeleton creation failed.");
	auto skeleton = manager.AddResource<Skeleton>(skeletonDesc.Key, skeletonResource);
	if (!skeleton.Get()) return fail("Skeleton registration failed.");

	auto controller = std::make_shared<AnimatorStateMachine>();
	controller->defaultState = "Single";

	AnimState single{};
	single.name = "Single";
	single.clip = clips[0];
	controller->states.push_back(single);

	AnimState blend{};
	blend.name = "Blend2D";
	blend.isBlendTree = true;
	blend.useBlendTree2D = true;
	blend.blendTree2D = tree;
	blend.blendTree2D.parameterX = "X";
	blend.blendTree2D.parameterY = "Y";
	controller->states.push_back(blend);

	AnimTransition transition{};
	transition.sourceState = "Single";
	transition.targetState = "Blend2D";
	transition.duration = 1.0f;
	controller->transitions.push_back(transition);

	AnimatorRuntimeState runtime;
	runtime.BindStateMachine(controller);
	runtime.SetFloat("X", 0.0f);
	runtime.SetFloat("Y", 15.0f);

	std::vector<BoneLocalPose> pose;
	runtime.UpdateLocalPose(0.5f, *skeleton.Get(), pose);
	if (pose.size() != 1 || std::abs(pose[0].Position.x - 7.5f) > 1e-5f)
		return fail("Transition did not blend into the evaluated 2D pose.");

	runtime.UpdateLocalPose(0.5f, *skeleton.Get(), pose);
	if (runtime.GetCurrentStateName() != "Blend2D" ||
		pose.size() != 1 || std::abs(pose[0].Position.x - 15.0f) > 1e-5f)
	{
		return fail("Transition completion did not preserve the 2D pose.");
	}

	TransitionCondition advance{};
	advance.parameterName = "Advance";
	advance.mode = EConditionMode::IfTrue;
	controller->transitions.front().conditions.push_back(advance);

	runtime.BindStateMachine(controller);
	runtime.SetTrigger("Advance");
	runtime.UpdateLocalPose(0.5f, *skeleton.Get(), pose);

	if (!runtime.GetParameters().GetTrigger("Advance") || runtime.IsTransitioning() || runtime.GetCurrentStateName() != "Single")
		return fail("An unplayable target consumed the transition trigger.");

	AnimState fallback = single;
	fallback.name = "Fallback";
	fallback.clip = clips[1];
	controller->states.push_back(fallback);

	AnimTransition fallbackTransition = controller->transitions.front();
	fallbackTransition.targetState = fallback.name;
	controller->transitions.push_back(fallbackTransition);

	runtime.UpdateLocalPose(0.5f, *skeleton.Get(), pose);
	if (!runtime.IsTransitioning() || runtime.GetParameters().GetTrigger("Advance") ||
		pose.size() != 1 || std::abs(pose[0].Position.x - 5.0f) > 1e-5f)
	{
		return fail("A playable transition was skipped after an unplayable target.");
	}

	AnimTransition anyState{};
	anyState.fromAnyState = true;
	anyState.targetState = fallback.name;
	anyState.duration = 0.25f;
	anyState.hasExitTime = true;
	anyState.conditions.push_back(advance);

	controller->transitions = { transition, anyState };
	for (const auto* initialState : { "Single", "Blend2D" })
	{
		controller->defaultState = initialState;
		runtime.BindStateMachine(controller);
		runtime.SetFloat("X", 0.0f);
		runtime.SetFloat("Y", 15.0f);
		runtime.SetTrigger("Advance");
		runtime.UpdateLocalPose(0.25f, *skeleton.Get(), pose);

		if (runtime.GetCurrentStateName() != fallback.name || runtime.GetParameters().GetTrigger("Advance"))
			return fail("Any State did not take priority or consume its trigger.");
	}

	runtime.SetTrigger("Advance");
	runtime.UpdateLocalPose(0.25f, *skeleton.Get(), pose);
	if (runtime.IsTransitioning() || !runtime.GetParameters().GetTrigger("Advance"))
		return fail("Any State attempted to re-enter its current target.");

	controller->transitions = { anyState };
	controller->transitions.front().conditions.clear();
	controller->defaultState = single.name;
	runtime.BindStateMachine(controller);
	runtime.UpdateLocalPose(0.25f, *skeleton.Get(), pose);
	if (runtime.IsTransitioning() || runtime.GetCurrentStateName() != single.name)
		return fail("Any State ran without a condition.");

	{
		const auto* engineRegistry = ReflectionRegistry::Create();
		if (!engineRegistry) return fail("Reflection initialization failed.");

		struct ReflectionScope
		{
			~ReflectionScope() { ReflectionRegistry::Destroy(); }
		} reflectionScope;

		const auto& types = engineRegistry->GetRegistry();
		const auto& codecs = engineRegistry->GetPropertyCodecs();
		const auto* type = types.FindType<AnimatorStateMachine>();
		if (!type) return fail("Animator controller reflection type was not registered.");

		constexpr auto legacyDocument = R"({
			"AnimatorController": {
				"defaultState": "Single",
				"states": [],
				"defaultParameters": [],
				"transitions": [{
					"sourceState": "Single",
					"targetState": "Blend2D",
					"duration": 1.0,
					"hasExitTime": false,
					"exitTime": 0.9,
					"conditions": []
				}]
			}
		})";

		archive::JsonArchive legacyReader;
		AnimatorStateMachine legacy;
		if (!legacyReader.Decode(legacyDocument) ||
			!archive_reflection::ReadObject(legacyReader, "AnimatorController", *type, legacy, &types, &codecs) ||
			legacy.transitions.size() != 1 || legacy.transitions.front().fromAnyState)
		{
			return fail("A legacy transition did not load as a regular transition.");
		}

		legacy.transitions.push_back(anyState);
		archive::JsonArchive writer;
		std::string document;
		if (!archive_reflection::WriteObject(writer, "AnimatorController", *type,
			reflection::ObjectView::From(legacy), &types, &codecs) || !writer.Encode(document))
		{
			return fail("Animator controller serialization failed.");
		}

		archive::JsonArchive reader;
		AnimatorStateMachine restored;
		if (!reader.Decode(document) ||
			!archive_reflection::ReadObject(reader, "AnimatorController", *type, restored, &types, &codecs) ||
			restored.transitions.size() != 2 || restored.transitions.front().fromAnyState)
		{
			return fail("Animator controller round-trip failed.");
		}

		const auto& restoredAnyState = restored.transitions.back();
		if (!restoredAnyState.fromAnyState || restoredAnyState.targetState != anyState.targetState ||
			restoredAnyState.conditions.size() != 1 ||
			restoredAnyState.conditions.front().parameterName != advance.parameterName ||
			restoredAnyState.conditions.front().mode != advance.mode)
		{
			return fail("Any State transition or its condition changed after reload.");
		}
	}

	tree.clip11.Reset();
	if (!expect(-10.0f, 0.0f, { 1.0f, 0.0f, 0.0f, 0.0f }))
		return fail("Boundary clamping failed.");

	if (tree.Evaluate(0.0f, 15.0f, output))
		return fail("A missing contributing clip was accepted.");

	for (const auto& sample : output)
	{
		if (sample.Clip || sample.Weight != 0.0f)
			return fail("Failure left a partial output.");
	}

	if (tree.Evaluate(std::numeric_limits<float>::quiet_NaN(), 15.0f, output))
		return fail("Non-finite input was accepted.");

	tree.maxX = tree.minX;
	if (tree.Evaluate(0.0f, 15.0f, output))
		return fail("A zero-width axis was accepted.");

	// ── 2-Bone IK Unit Tests ──
	{
		const vec3 rootPos(0.0f, 0.0f, 0.0f);
		const vec3 midPos(0.0f, 5.0f, 0.0f);
		const vec3 endPos(0.0f, 10.0f, 0.0f);
		const vec3 poleVec(0.0f, 5.0f, 5.0f); // bending forward in +Z

		// Reachable target within max reach (10.0)
		const vec3 reachableTarget(0.0f, 4.0f, 3.0f); // distance = 5.0
		const auto solReachable = SolveTwoBoneIK(rootPos, midPos, endPos, reachableTarget, poleVec, 1.0f);
		if (!solReachable.solved)
			return fail("2-Bone IK failed to solve reachable target.");

		if (glm::distance(solReachable.newEndPos, reachableTarget) > 1e-3f)
			return fail("2-Bone IK end-effector did not reach target within epsilon.");

		const float len1 = glm::distance(solReachable.newMidPos, rootPos);
		const float len2 = glm::distance(solReachable.newEndPos, solReachable.newMidPos);
		if (std::abs(len1 - 5.0f) > 1e-3f || std::abs(len2 - 5.0f) > 1e-3f)
			return fail("2-Bone IK violated bone length preservation.");

		// Unreachable target beyond max reach (20.0 > 10.0)
		const vec3 unreachableTarget(0.0f, 20.0f, 0.0f);
		const auto solUnreachable = SolveTwoBoneIK(rootPos, midPos, endPos, unreachableTarget, poleVec, 1.0f);
		if (!solUnreachable.solved)
			return fail("2-Bone IK failed to solve unreachable target.");

		if (std::isnan(solUnreachable.newEndPos.x) || std::isnan(solUnreachable.newEndPos.y) || std::isnan(solUnreachable.newEndPos.z))
			return fail("2-Bone IK produced NaN on unreachable target.");

		const float reachDist = glm::distance(solUnreachable.newEndPos, rootPos);
		if (reachDist > 10.0f || reachDist < 9.5f)
			return fail("2-Bone IK soft distance clamping failed for unreachable target.");

		const float uLen1 = glm::distance(solUnreachable.newMidPos, rootPos);
		const float uLen2 = glm::distance(solUnreachable.newEndPos, solUnreachable.newMidPos);
		if (std::abs(uLen1 - 5.0f) > 1e-3f || std::abs(uLen2 - 5.0f) > 1e-3f)
			return fail("2-Bone IK violated bone length preservation on unreachable target.");

		// Weight blending check (weight = 0.0 returns initial end position)
		const auto solZeroWeight = SolveTwoBoneIK(rootPos, midPos, endPos, reachableTarget, poleVec, 0.0f);
		if (glm::distance(solZeroWeight.newEndPos, endPos) > 1e-3f)
			return fail("2-Bone IK with zero weight did not preserve initial pose.");
	}

	// ── Look-At IK Unit Tests ──
	{
		const vec3 bonePos(0.0f, 10.0f, 0.0f);
		const quat boneRot = glm::identity<quat>();
		const vec3 fwd(0.0f, 0.0f, 1.0f);
		const vec3 up(0.0f, 1.0f, 0.0f);
		const float maxYaw = glm::radians(45.0f);
		const float maxPitch = glm::radians(30.0f);

		// Target within angle bounds
		const vec3 reachableTarget(2.0f, 10.0f, 10.0f); // yaw ~11.3 deg < 45 deg
		const auto solLookAt = SolveLookAtIK(bonePos, boneRot, reachableTarget, fwd, up, maxYaw, maxPitch, 1.0f);
		if (!solLookAt.solved)
			return fail("Look-At IK failed to solve.");

		const vec3 newFwd = solLookAt.rotationOffset * fwd;
		const vec3 expectedDir = glm::normalize(reachableTarget - bonePos);
		if (glm::distance(newFwd, expectedDir) > 1e-3f)
			return fail("Look-At IK forward direction did not align with target.");

		// Target beyond max yaw angle (e.g. 80 deg)
		const vec3 extremeTarget(100.0f, 10.0f, 10.0f);
		const auto solClamped = SolveLookAtIK(bonePos, boneRot, extremeTarget, fwd, up, maxYaw, maxPitch, 1.0f);
		if (!solClamped.solved)
			return fail("Look-At IK failed to solve clamped target.");

		const vec3 clampedFwd = solClamped.rotationOffset * fwd;
		const float clampedYaw = std::atan2(clampedFwd.x, clampedFwd.z);
		if (std::abs(clampedYaw - maxYaw) > 1e-3f)
			return fail("Look-At IK did not clamp yaw angle to max limit.");

		// Zero weight preserves orientation
		const auto solZeroWeight = SolveLookAtIK(bonePos, boneRot, reachableTarget, fwd, up, maxYaw, maxPitch, 0.0f);
		const vec3 zeroFwd = solZeroWeight.rotationOffset * fwd;
		if (glm::distance(zeroFwd, fwd) > 1e-3f)
			return fail("Look-At IK with zero weight did not preserve initial orientation.");
	}

	// ── Root Motion Extraction Unit Tests ──
	{
		AnimationCreateDesc rmDesc{};
		rmDesc.Key = L"RootMotionCheck_Clip";
		rmDesc.Path = rmDesc.Key;
		rmDesc.Name = rmDesc.Key;
		rmDesc.Duration = 2.0; // 2 seconds at 1 TPS
		rmDesc.TicksPerSecond = 1.0;

		AnimationTrack rootTrack{};
		rootTrack.TargetBoneName = L"Root";
		{
			VectorKeyframe k0{}; k0.Time = 0.0; k0.Value = vec3(0.0f, 0.0f, 0.0f);
			VectorKeyframe k1{}; k1.Time = 1.0; k1.Value = vec3(0.0f, 0.0f, 5.0f);
			VectorKeyframe k2{}; k2.Time = 2.0; k2.Value = vec3(0.0f, 0.0f, 10.0f);
			rootTrack.PositionKeyframes = { k0, k1, k2 };
		}
		rmDesc.Tracks.push_back(rootTrack);

		auto* rmClip = Animation::Create(&rmDesc);
		if (!rmClip) return fail("Root Motion test clip creation failed.");
		auto rmClipHandle = manager.AddResource<Animation>(rmDesc.Key, rmClip);
		if (!rmClipHandle.IsValid()) return fail("Root Motion test clip registration failed.");

		// Normal step: 0.2s -> 0.6s => delta = (0, 0, 3.0 - 1.0) = (0, 0, 2.0)
		const auto deltaNormal = ExtractRootMotionDelta(*rmClip, 0, 0.2f, 0.6f, true);
		if (!deltaNormal.valid)
			return fail("Root Motion normal extraction failed.");
		if (glm::distance(deltaNormal.deltaPosition, vec3(0.0f, 0.0f, 2.0f)) > 1e-3f)
			return fail("Root Motion normal delta position inaccurate.");

		// Loop wrap-around: 1.8s -> 0.2s (duration = 2.0s)
		// Segment 1: [1.8 .. 2.0] => pos(2.0) - pos(1.8) = 10.0 - 9.0 = 1.0
		// Segment 2: [0.0 .. 0.2] => pos(0.2) - pos(0.0) = 1.0 - 0.0 = 1.0
		// Total delta = 1.0 + 1.0 = 2.0
		const auto deltaWrap = ExtractRootMotionDelta(*rmClip, 0, 1.8f, 0.2f, true);
		if (!deltaWrap.valid)
			return fail("Root Motion loop wrap-around extraction failed.");
		if (glm::distance(deltaWrap.deltaPosition, vec3(0.0f, 0.0f, 2.0f)) > 1e-3f)
			return fail("Root Motion wrap-around delta position inaccurate.");
		if (deltaWrap.deltaPosition.z <= 0.0f)
			return fail("Root Motion wrap-around jumped backward.");
	}

	// ── Foot Ground IK & Visual Scripting IK Unit Tests ──
	{
		// 1. Verify ComponentEnableMember contract
		static_assert(requires { AnimationIKComponent::ComponentEnableMember; },
			"AnimationIKComponent must define ComponentEnableMember.");
		static_assert(AnimationIKComponent::ComponentEnableMember == &AnimationIKComponent::enabled,
			"ComponentEnableMember must point to AnimationIKComponent::enabled.");

		entt::registry testRegistry;
		const auto testEntity = testRegistry.create();
		auto& testIk = testRegistry.emplace<AnimationIKComponent>(testEntity);
		testIk.enabled = true;
		if (!IsComponentEnabled<AnimationIKComponent>(testRegistry, testEntity))
			return fail("IsComponentEnabled failed for enabled AnimationIKComponent.");
		if (TryGetEnabledComponent<AnimationIKComponent>(testRegistry, testEntity) != &testIk)
			return fail("TryGetEnabledComponent failed to retrieve enabled AnimationIKComponent.");

		testIk.enabled = false;
		if (IsComponentEnabled<AnimationIKComponent>(testRegistry, testEntity))
			return fail("IsComponentEnabled reported true for disabled AnimationIKComponent.");
		if (TryGetEnabledComponent<AnimationIKComponent>(testRegistry, testEntity) != nullptr)
			return fail("TryGetEnabledComponent returned non-null for disabled AnimationIKComponent.");

		// 2. Setup Scene, Skeleton, and Floor Collider
		auto* scene = Scene::Create();
		if (!scene) return fail("Scene creation failed for Foot Ground IK check.");
		scene->SetActive(true);

		struct SceneScope
		{
			Scene* scenePtr;
			~SceneScope() { Safe_Release(scenePtr); }
		} sceneScope{ scene };

		auto& world = scene->GetRegistry();

		// Floor entity: elevated box collider at height 0.2f
		// Box extents: size = (10, 0.4, 10), center = (0, 0.0, 0), top face = 0.2f
		const auto floorEntity = scene->CreateEntity().GetEntityHandle();
		auto& floorTrans = world.get_or_emplace<TransformComponent>(floorEntity);
		auto& floorWorldTrans = world.get_or_emplace<WorldTransformComponent>(floorEntity);
		(void)floorTrans.SetLocalPosition(vec3(0.0f, 0.0f, 0.0f));
		TransformSystem::UpdateRootWorld(floorTrans, floorWorldTrans);

		auto& floorCol = world.emplace<ColliderComponent>(floorEntity);
		BoxShape floorBox{};
		floorBox.Size = vec3(10.0f, 0.4f, 10.0f); // Top is at y = 0.0 + 0.2 = +0.2f
		(void)floorCol.SetShape(floorBox);

		// Character entity
		const auto charEntity = scene->CreateEntity().GetEntityHandle();
		auto& charTrans = world.get_or_emplace<TransformComponent>(charEntity);
		auto& charWorldTrans = world.get_or_emplace<WorldTransformComponent>(charEntity);
		(void)charTrans.SetLocalPosition(vec3(0.0f, 0.0f, 0.0f));
		TransformSystem::UpdateRootWorld(charTrans, charWorldTrans);

		auto& ikComp = world.emplace<AnimationIKComponent>(charEntity);
		ikComp.enabled = true;
		ikComp.enableFootGroundIK = true;
		ikComp.footHeightOffset = 0.05f;
		ikComp.raycastUpOffset = 0.5f;
		ikComp.raycastDownDistance = 1.2f;
		ikComp.maxPelvisOffset = 0.4f;
		ikComp.pelvisSmoothSpeed = 10.0f;
		ikComp.pelvisBoneName = "Pelvis";
		ikComp.leftFootChainIndex = 0;
		ikComp.rightFootChainIndex = 1;

		// 3. Setup Skeleton: Pelvis -> Thigh -> Calf -> Foot
		SkeletonCreateDesc ikSkelDesc{};
		ikSkelDesc.Key = L"FootIK_TestSkeleton";

		// Bone 0: Pelvis (parent -1, local pos y = 1.0)
		Bone bPelvis{};
		bPelvis.Name = L"Pelvis";
		bPelvis.ParentIndex = -1;
		bPelvis.LocalTransform = glm::translate(mat4(1.0f), vec3(0.0f, 1.0f, 0.0f));
		bPelvis.OffsetMatrix = glm::inverse(bPelvis.LocalTransform);
		ikSkelDesc.Bones.push_back(bPelvis);

		// Bone 1: Thigh (parent 0, local pos y = 0.0) -> global y = 1.0
		Bone bThigh{};
		bThigh.Name = L"Thigh";
		bThigh.ParentIndex = 0;
		bThigh.LocalTransform = mat4(1.0f);
		bThigh.OffsetMatrix = glm::inverse(bPelvis.LocalTransform * bThigh.LocalTransform);
		ikSkelDesc.Bones.push_back(bThigh);

		// Bone 2: Calf (parent 1, local pos y = -0.5) -> global y = 0.5
		Bone bCalf{};
		bCalf.Name = L"Calf";
		bCalf.ParentIndex = 1;
		bCalf.LocalTransform = glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, 0.0f));
		bCalf.OffsetMatrix = glm::inverse(bPelvis.LocalTransform * bThigh.LocalTransform * bCalf.LocalTransform);
		ikSkelDesc.Bones.push_back(bCalf);

		// Bone 3: Foot (parent 2, local pos y = -0.5) -> global y = 0.0
		Bone bFoot{};
		bFoot.Name = L"Foot";
		bFoot.ParentIndex = 2;
		bFoot.LocalTransform = glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, 0.0f));
		bFoot.OffsetMatrix = glm::inverse(bPelvis.LocalTransform * bThigh.LocalTransform * bCalf.LocalTransform * bFoot.LocalTransform);
		ikSkelDesc.Bones.push_back(bFoot);

		auto* ikSkelResource = Skeleton::Create(&ikSkelDesc);
		if (!ikSkelResource) return fail("Foot IK Skeleton creation failed.");
		auto ikSkeleton = manager.AddResource<Skeleton>(ikSkelDesc.Key, ikSkelResource);
		if (!ikSkeleton.Get()) return fail("Foot IK Skeleton registration failed.");

		// Chain 0: Thigh -> Calf -> Foot
		TwoBoneIKChain chain{};
		chain.rootBoneName = "Thigh";
		chain.midBoneName = "Calf";
		chain.endBoneName = "Foot";
		chain.poleVector = vec3(0.0f, 0.5f, 1.0f);
		chain.weight = 1.0f;
		chain.enabled = true;
		ikComp.twoBoneChains.push_back(chain);

		// Initial local pose matching default bind pose
		std::vector<BoneLocalPose> localPose(4);
		localPose[0].Position = vec3(0.0f, 1.0f, 0.0f); // Pelvis
		localPose[1].Position = vec3(0.0f, 0.0f, 0.0f); // Thigh
		localPose[2].Position = vec3(0.0f, -0.5f, 0.0f); // Calf
		localPose[3].Position = vec3(0.0f, -0.5f, 0.0f); // Foot

		std::vector<mat4> globalPose;
		const mat4 entityWorldMatrix = charWorldTrans.worldMatrix;

		// 4. Test UpdateFootGroundIK
		UpdateFootGroundIK(*scene, charEntity, *ikSkeleton.Get(), localPose, globalPose, ikComp, entityWorldMatrix, 0.0f);

		// Floor top is at y = 0.2f. Foot height offset is 0.05f.
		// Expected target foot Y = 0.2f + 0.05f = 0.25f.
		const float expectedFootY = 0.2f + 0.05f;
		if (std::abs(ikComp.twoBoneChains[0].targetPosition.y - expectedFootY) > 1e-3f)
			return fail("UpdateFootGroundIK did not update foot targetPosition.y to floorTopY + footHeightOffset.");
		if (!ikComp.twoBoneChains[0].targetInWorldSpace)
			return fail("UpdateFootGroundIK did not set targetInWorldSpace to true.");

		// 5. Test ApplyAnimationIK moves the foot bone to the ground target
		std::vector<mat4> skinningMatrices;
		ApplyAnimationIK(*ikSkeleton.Get(), localPose, skinningMatrices, ikComp, entityWorldMatrix);

		// Forward kinematics to check new global position of Foot (bone 3)
		std::vector<mat4> solvedGlobals(4);
		for (size_t i = 0; i < 4; ++i)
		{
			const mat4 localM = glm::translate(mat4(1.0f), localPose[i].Position) *
				glm::toMat4(localPose[i].Rotation) * glm::scale(mat4(1.0f), localPose[i].Scale);
			const int32 parent = ikSkeleton->GetBones()[i].ParentIndex;
			solvedGlobals[i] = (parent >= 0) ? solvedGlobals[parent] * localM : localM;
		}

		const vec3 solvedFootWorld = vec3(entityWorldMatrix * solvedGlobals[3][3]);
		if (std::abs(solvedFootWorld.y - expectedFootY) > 1e-2f)
			return fail("ApplyAnimationIK did not move the foot bone to the ground target.");

		// 6. Test uneven/lower ground pelvis offset
		// Move floor lower: top at y = -0.1f -> expected desiredY = -0.05f -> delta = -0.05f
		(void)floorTrans.SetLocalPosition(vec3(0.0f, -0.3f, 0.0f)); // center -0.3, size 0.4 -> top = -0.1f
		TransformSystem::UpdateRootWorld(floorTrans, floorWorldTrans);

		// Reset local pose
		localPose[0].Position = vec3(0.0f, 1.0f, 0.0f);
		localPose[0].Rotation = glm::identity<quat>();
		localPose[1].Position = vec3(0.0f, 0.0f, 0.0f);
		localPose[1].Rotation = glm::identity<quat>();
		localPose[2].Position = vec3(0.0f, -0.5f, 0.0f);
		localPose[2].Rotation = glm::identity<quat>();
		localPose[3].Position = vec3(0.0f, -0.5f, 0.0f);
		localPose[3].Rotation = glm::identity<quat>();

		UpdateFootGroundIK(*scene, charEntity, *ikSkeleton.Get(), localPose, globalPose, ikComp, entityWorldMatrix, 0.0f);
		const float lowerExpectedY = -0.1f + 0.05f; // -0.05f
		if (std::abs(ikComp.twoBoneChains[0].targetPosition.y - lowerExpectedY) > 1e-3f)
			return fail("UpdateFootGroundIK did not update targetPosition for lower ground.");

		if (ikComp.currentPelvisOffset >= 0.0f || std::abs(ikComp.currentPelvisOffset - (-0.05f)) > 1e-3f)
			return fail("UpdateFootGroundIK did not lower currentPelvisOffset for lower ground.");

		if (std::abs(localPose[0].Position.y - (1.0f - 0.05f)) > 1e-3f)
			return fail("UpdateFootGroundIK did not apply currentPelvisOffset to pelvis local pose.");

		// 7. Test Visual Scripting IK helper functions in GraphGameplayLibrary
		const uint64 entityId = static_cast<uint64>(static_cast<uint32>(charEntity));
		GraphGameplayLibrary::SetTwoBoneIKTarget(*scene, entityId, 0, vec3(0.0f, 0.3f, 0.0f), 0.8f);
		if (std::abs(ikComp.twoBoneChains[0].targetPosition.y - 0.3f) > 1e-4f ||
			std::abs(ikComp.twoBoneChains[0].weight - 0.8f) > 1e-4f)
			return fail("GraphGameplayLibrary::SetTwoBoneIKTarget failed to modify chain.");

		GraphGameplayLibrary::SetLookAtIKTarget(*scene, entityId, 0, vec3(0.0f, 1.0f, 5.0f), 0.7f);

		GraphGameplayLibrary::SetFootGroundIKEnabled(*scene, entityId, false, 0.08f);
		if (ikComp.enableFootGroundIK || std::abs(ikComp.footHeightOffset - 0.08f) > 1e-4f)
			return fail("GraphGameplayLibrary::SetFootGroundIKEnabled failed to update settings.");

		GraphGameplayLibrary::SetAnimationIKEnabled(*scene, entityId, false);
		if (ikComp.enabled)
			return fail("GraphGameplayLibrary::SetAnimationIKEnabled failed to disable IK.");
	}

	std::cout << "BlendTree2D, 2-Bone IK, Look-At IK, Root Motion, and Foot Ground IK checks passed.\n";
	return 0;
}