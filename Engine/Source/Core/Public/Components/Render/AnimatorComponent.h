#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Animation.h"
#include "AnimatorStateMachine.h"
#include "BoneMask.h"

namespace Engine
{
	STRUCT()
		struct ENGINE_API AnimatorLayer
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, ALLOW_MISSING)
			std::string name = "UpperBody";

		PROPERTY(EDITABLE, ALLOW_MISSING)
			float weight = 0.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING)
			BoneMask mask;

		PROPERTY(EDITABLE, ALLOW_MISSING)
			ResourceHandle<Animation> clip;

		PROPERTY(EDITABLE, ALLOW_MISSING)
			float playRate = 1.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING)
			bool isLooping = false;

		PROPERTY(EDITABLE, ALLOW_MISSING)
			bool isPlaying = false;

		PROPERTY(READONLY, NOSERIALIZE)
			float currentTime = 0.0f;
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ANIMATOR"), CATEGORY("CATEGORY_RENDER"))
	struct AnimatorComponent
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANIMATION_CLIP"))
			ResourceHandle<Animation> currentClip;

		// 애니메이터 컨트롤러 파일 경로 (.animator)
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CONTROLLER_PATH"))
			std::string controllerPath;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_RATE"))
			f32 playRate = 1.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
			bool isLooping = true;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_PLAYING"))
			bool isPlaying = true;

		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_CURRENT_TIME"))
			f32 currentTime = 0.0f;

		// 셰이더 및 스키닝에 사용할 최종 뼈 변환 행렬 목록
		PROPERTY(READONLY, NOSERIALIZE)
			std::vector<mat4> finalBoneMatrices;

		// 애니메이션 상태 머신 런타임 인스턴스 (비직렬화)
		AnimatorRuntimeState runtimeState;
		std::shared_ptr<AnimatorStateMachine> stateMachineAsset;
		std::string loadedControllerPath;

		using AnimNotifyCallback = std::function<void(entt::entity entity, const AnimNotifyEvent& event)>;
		std::vector<AnimNotifyCallback> notifyCallbacks;
		void RegisterNotifyCallback(AnimNotifyCallback callback)
		{
			notifyCallbacks.push_back(std::move(callback));
		}

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_ANIMATOR_LAYERS"))
		std::vector<AnimatorLayer> layers;
		AnimatorLayer* FindLayer(const std::string& name)
		{
			for (auto& layer : layers) { if (layer.name == name) return &layer; }
			return nullptr;
		}
		const AnimatorLayer* FindLayer(const std::string& name) const
		{
			for (const auto& layer : layers) { if (layer.name == name) return &layer; }
			return nullptr;
		}
		void PlayLayer(const std::string& name, ResourceHandle<Animation> clip, float weight = 1.0f, bool isLooping = false)
		{
			auto* layer = FindLayer(name);
			if (!layer)
			{
				layers.push_back(AnimatorLayer{ name, weight, BoneMask{ L"Spine", true }, clip, 1.0f, isLooping, true, 0.0f });
				return;
			}
			layer->clip = clip;
			layer->weight = weight;
			layer->isLooping = isLooping;
			layer->isPlaying = true;
			layer->currentTime = 0.0f;
		}
		void StopLayer(const std::string& name)
		{
			if (auto* layer = FindLayer(name)) { layer->isPlaying = false; layer->currentTime = 0.0f; }
		}
		void SetLayerWeight(const std::string& name, float weight)
		{
			if (auto* layer = FindLayer(name)) layer->weight = std::clamp(weight, 0.0f, 1.0f);
		}
		float GetLayerWeight(const std::string& name) const
		{
			const auto* layer = FindLayer(name);
			return layer ? layer->weight : 0.0f;
		}

		void Play() { isPlaying = true; }
		void Pause() { isPlaying = false; }
		void Stop()
		{
			isPlaying = false;
			currentTime = 0.0f;
			if (runtimeState.HasStateMachine()) runtimeState.Reset();
		}
		void SetTime(float time)
		{
			currentTime = std::max(0.0f, time);
			if (runtimeState.HasStateMachine()) runtimeState.SetStateTime(currentTime);
		}

		// 컨트롤러 파일 로드 및 상태 머신 바인딩
		bool LoadController(const std::string& path)
		{
			auto sm = std::make_shared<AnimatorStateMachine>();
			if (!sm->LoadFromFile(path)) return false;
			stateMachineAsset = sm;
			controllerPath = path;
			loadedControllerPath = path;
			runtimeState.BindStateMachine(stateMachineAsset);
			return true;
		}

		void SetFloat(const std::string& name, float value) { runtimeState.SetFloat(name, value); }
		void SetInt(const std::string& name, int32 value) { runtimeState.SetInt(name, value); }
		void SetBool(const std::string& name, bool value) { runtimeState.SetBool(name, value); }
		void SetTrigger(const std::string& name) { runtimeState.SetTrigger(name); }
	};
}