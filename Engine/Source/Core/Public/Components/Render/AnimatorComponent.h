#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Animation.h"
#include "AnimatorStateMachine.h"
#include <vector>
#include <memory>
#include <string>

namespace Engine
{
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

		void Play() { isPlaying = true; }
		void Pause() { isPlaying = false; }
		void Stop() { isPlaying = false; currentTime = 0.0f; }

		// 컨트롤러 파일 로드 및 상태 머신 바인딩
		bool LoadController(const std::string& path)
		{
			auto sm = std::make_shared<AnimatorStateMachine>();
			if (!sm->LoadFromFile(path)) return false;
			stateMachineAsset = sm;
			controllerPath = path;
			runtimeState.BindStateMachine(stateMachineAsset);
			return true;
		}

		void SetFloat(const std::string& name, float value) { runtimeState.SetFloat(name, value); }
		void SetInt(const std::string& name, int32 value) { runtimeState.SetInt(name, value); }
		void SetBool(const std::string& name, bool value) { runtimeState.SetBool(name, value); }
		void SetTrigger(const std::string& name) { runtimeState.SetTrigger(name); }
	};
}