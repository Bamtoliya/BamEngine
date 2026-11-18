#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "ActionTimelineAsset.h"
#include <functional>
#include <string>
#include <vector>

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ACTION_TIMELINE"), CATEGORY("CATEGORY_RENDER"))
	struct ActionTimelineComponent
	{
		REFLECT_BODY()

		// 런타임 캐시 (직렬화 대상 아님 - timelinePath가 단일 진실의 원천)
		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_TIMELINE_ASSET"))
		ResourceHandle<ActionTimelineAsset> timelineAsset;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TIMELINE_PATH"))
		std::string timelinePath;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_RATE"))
			f32 playRate = 1.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
			bool isLooping = false;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_PLAYING"))
			bool isPlaying = false;

		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_CURRENT_TIME"))
			f32 currentTime = 0.0f;

		// 직전 틱 시간 (구간 내 노티파이 누락 없는 정확한 검출용)
		f32 prevTime = 0.0f;

		// 지속형 노티파이 해제 판정용 (런타임 전용): 재생 세션 번호와 세션 안에서 누적한 재생 시간(루프해도 줄지 않음)
		uint32 playSerial = 0;
		f32 playElapsed = 0.0f;

		// 런타임 캐시
		std::string loadedTimelinePath;

		// 타임라인을 작성한 클립 (런타임 전용). 재생 세션마다 에셋의 ClipName으로 다시 찾아 Animator에 건다.
		ResourceHandle<Animation> boundClip;
		uint32 boundClipSerial = 0xFFFFFFFFu; // 아직 찾지 않음

		// ── 게임플레이 이벤트 콜백 ──
		using ActionNotifyCallback = std::function<void(entt::entity entity, const AnimNotifyData& notify)>;
		std::vector<ActionNotifyCallback> notifyCallbacks;

		using ActionShotCallback = std::function<void(entt::entity entity, const TimelineCameraTrack& shot, float weight)>;
		std::vector<ActionShotCallback> shotCallbacks;

		void RegisterNotifyCallback(ActionNotifyCallback cb) { notifyCallbacks.push_back(std::move(cb)); }
		void RegisterShotCallback(ActionShotCallback cb) { shotCallbacks.push_back(std::move(cb)); }

		// ── 런타임 제어 API ──
		void Play(const std::string& path = "")
		{
			if (!path.empty()) timelinePath = path;
			currentTime = 0.0f;
			prevTime = 0.0f;
			playElapsed = 0.0f;
			++playSerial;
			isPlaying = true;
		}

		void Pause() { isPlaying = false; }
		void Resume() { isPlaying = true; }
		void Stop() { isPlaying = false; currentTime = 0.0f; prevTime = 0.0f; playElapsed = 0.0f; ++playSerial; }
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SOCKET_ATTACHMENT"), CATEGORY("CATEGORY_CORE"))
		struct SocketAttachmentComponent
	{
		REFLECT_BODY()
			PROPERTY(EDITABLE)
			entt::entity targetEntity = entt::null;
		PROPERTY(EDITABLE)
			std::string boneName = "";
		PROPERTY(EDITABLE)
			vec3 offsetPosition = vec3(0.0f);
		PROPERTY(EDITABLE)
			vec3 offsetRotation = vec3(0.0f);
		PROPERTY(EDITABLE)
			vec3 scale = vec3(1.0f);
		PROPERTY(EDITABLE)
			bool autoDestroyOnTargetDead = true;
	};
	// ── 액션 타임라인에서 스폰된 1회성 VFX의 자동 소멸 수명 주기 컴포넌트 ──
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ACTION_VFX_LIFECYCLE"), CATEGORY("CATEGORY_RENDER"))
	struct ActionVFXLifecycleComponent
	{
		REFLECT_BODY()
		PROPERTY(EDITABLE)
		bool autoDestroy = true;
	};

	// ── 지속형 VFX 노티파이: 소유 타임라인의 해제 시점까지 유지한다 (런타임 생성 전용) ──
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ACTION_VFX_HOLD"), CATEGORY("CATEGORY_RENDER"))
		struct ActionVFXHoldComponent
	{
		REFLECT_BODY()
			PROPERTY(READONLY, NOSERIALIZE)
			entt::entity ownerEntity = entt::null;
		PROPERTY(READONLY, NOSERIALIZE)
			uint32 playSerial = 0;
		PROPERTY(READONLY, NOSERIALIZE)
			f32 releaseElapsed = 0.0f;
	};
}