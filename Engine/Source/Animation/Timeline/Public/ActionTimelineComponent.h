#pragma once

#include "Engine_API.h"
#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "ActionTimelineAsset.h"
#include <entt/entity/entity.hpp>
#include <functional>
#include <string>
#include <vector>

namespace Engine
{
	// 타임라인 카메라 시퀀서 런타임 스냅샷 (직렬화 대상 아님)
	struct TimelineCameraSavedState
	{
		entt::entity cameraEntity = entt::null;
		vec3 savedPosition = vec3(0.0f);
		quat savedRotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
		vec3 savedEulerRotation = vec3(0.0f);
		f32 savedFOV = 60.0f;
		bool savedIsMainCamera = false;
		bool hasTransform = false;
	};

	struct TimelineCameraBlendState
	{
		int32 activeSectionIndex = -1;
		vec3 startPosition = vec3(0.0f);
		quat startRotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
		f32 startFOV = 60.0f;
		bool hasStartPose = false;
	};
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
		bool notifyStartPending = true;

		// 런타임 캐시
		std::string loadedTimelinePath;

		// 타임라인을 작성한 클립 (런타임 전용). 재생 세션마다 에셋의 ClipName으로 다시 찾아 Animator에 건다.
		ResourceHandle<Animation> boundClip;
		uint32 boundClipSerial = 0xFFFFFFFFu; // 아직 찾지 않음
		entt::entity boundClipAnimator = entt::null;

		// 타임라인이 넘겨받은 Animator 추적 (런타임 전용). 원래 상태는 AnimatorComponent가 소유하며, 세션 종료·Stop·교체·컴포넌트 제거 시 되돌린다.
		// animatorOverrideEntity: 넘겨받은 Animator 엔티티(자신 또는 자식 모델 루트). null이면 넘겨받지 않은 상태다.
		entt::entity animatorOverrideEntity = entt::null;
		uint32 animatorOverrideSerial = 0;

		// 카메라 시퀀서 오버라이드 추적 (런타임 전용). 종료·Stop·에셋 교체·비활성·컴포넌트 제거 시 원래 상태로 무손실 복원한다.
		bool cameraOverrideActive = false;
		uint32 cameraOverrideSerial = 0;
		std::vector<TimelineCameraSavedState> savedCameraStates;
		TimelineCameraBlendState activeCameraCutBlend;
		TimelineCameraBlendState activeCameraSplineBlend;

		// ── 게임플레이 이벤트 콜백 (사용 중단 예정) ──
		// 해제 수단이 없어 핫리로드되는 모듈에서 쓰면 안전하지 않다. 이벤트 노티파이는 ActionTimelineEvents로 구독한다.
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
			notifyStartPending = true;
			++playSerial;
			isPlaying = true;
		}

		void Pause() { isPlaying = false; }
		void Resume() { isPlaying = true; }
		void Stop() { isPlaying = false; currentTime = 0.0f; prevTime = 0.0f; playElapsed = 0.0f; notifyStartPending = true; ++playSerial; }
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
	// ownerEntity에 리플렉션을 붙이면 Scene::DestroyEntity가 비울 수 없는 외부 참조로 오판하므로 런타임 비리플렉션 필드로 둔다.
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ACTION_VFX_HOLD"), CATEGORY("CATEGORY_RENDER"))
	struct ActionVFXHoldComponent
	{
		REFLECT_BODY()
		entt::entity ownerEntity = entt::null;
		uint32 playSerial = 0;
		f32 releaseElapsed = 0.0f;
	};
}