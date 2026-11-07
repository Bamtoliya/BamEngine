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

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TIMELINE_ASSET"))
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

		// 런타임 캐시
		std::string loadedTimelinePath;

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
			isPlaying = true;
		}

		void Pause() { isPlaying = false; }
		void Resume() { isPlaying = true; }
		void Stop() { isPlaying = false; currentTime = 0.0f; prevTime = 0.0f; }
	};
}