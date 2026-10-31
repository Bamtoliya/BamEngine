#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include <vector>
#include <algorithm>

namespace Engine
{
	ENUM()
		enum class EFlipbookPlayMode : uint8
	{
		Loop,       // 무한 반복 재생
		Once,       // 1회 재생 후 정지
		PingPong    // 끝에 도달하면 역방향으로 왕복 재생
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPRITE_ANIMATION"), CATEGORY("CATEGORY_RENDER"))
		struct SpriteAnimationComponent
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FRAMES"))
			std::vector<ResourceHandle<Sprite>> frames;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FPS"))
			f32 framesPerSecond = 12.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_RATE"))
			f32 playRate = 1.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_MODE"))
			EFlipbookPlayMode playMode = EFlipbookPlayMode::Loop;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_PLAYING"))
			bool isPlaying = true;

		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_CURRENT_FRAME"))
			int32 currentFrameIndex = 0;

		PROPERTY(READONLY, NOSERIALIZE)
			f32 elapsedTime = 0.0f;

		PROPERTY(READONLY, NOSERIALIZE)
			bool isReversed = false;

		PROPERTY(READONLY, NOSERIALIZE)
			bool isFinished = false;

		void Play() { isPlaying = true; isFinished = false; }
		void Pause() { isPlaying = false; }
		void Stop() { isPlaying = false; currentFrameIndex = 0; elapsedTime = 0.0f; isReversed = false; isFinished = false; }
		void SetFrame(int32 frameIndex)
		{
			if (!frames.empty())
				currentFrameIndex = std::clamp(frameIndex, 0, static_cast<int32>(frames.size() - 1));
			elapsedTime = 0.0f;
		}
	};
}