#include "BlendTree2D.h"
#include "ResourceManager.h"
#include <cmath>
#include <iostream>
#include <limits>
#include "AnimatorStateMachine.h"
#include "Reflection/ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

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

	std::cout << "BlendTree2D weight and failure checks passed.\n";
}