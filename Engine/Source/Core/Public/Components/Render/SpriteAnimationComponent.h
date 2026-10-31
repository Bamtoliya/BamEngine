#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include "SpriteAnimation.h"
#include <vector>
#include <algorithm>

namespace Engine
{
	using EFlipbookPlayMode = ESpriteAnimationPlayMode;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPRITE_ANIMATION"), CATEGORY("CATEGORY_RENDER"))
		struct SpriteAnimationComponent
	{
		REFLECT_BODY()

			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPRITE_ANIMATION"), CATEGORY("CATEGORY_ANIMATION"))
			ResourceHandle<SpriteAnimation> animation;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FRAMES"))
			std::vector<ResourceHandle<Sprite>> frames;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FPS"))
			f32 framesPerSecond = 12.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_RATE"))
			f32 playRate = 1.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_MODE"))
			ESpriteAnimationPlayMode playMode = ESpriteAnimationPlayMode::Loop;

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

		// ── 상태 제어 ──
		void Play() { isPlaying = true; isFinished = false; }
		void Pause() { isPlaying = false; }
		void Stop() { isPlaying = false; currentFrameIndex = 0; elapsedTime = 0.0f; isReversed = false; isFinished = false; }
		void SetFrame(int32 frameIndex)
		{
			const int32 count = GetEffectiveFrameCount();
			if (count > 0)
				currentFrameIndex = std::clamp(frameIndex, 0, count - 1);
			elapsedTime = 0.0f;
		}

		// ── 에셋 우선 및 로컬 폴백 헬퍼 ──
		int32 GetEffectiveFrameCount() const
		{
			if (animation.IsValid() && animation.Get()) return animation.Get()->GetFrameCount();
			return static_cast<int32>(frames.size());
		}

		ResourceHandle<Sprite> GetSpriteAtFrame(int32 index) const
		{
			if (animation.IsValid() && animation.Get())
			{
				const auto* f = animation.Get()->GetFrame(index);
				return f ? f->sprite : ResourceHandle<Sprite>();
			}
			if (index >= 0 && index < static_cast<int32>(frames.size())) return frames[index];
			return ResourceHandle<Sprite>();
		}

		f32 GetEffectiveFPS() const
		{
			if (animation.IsValid() && animation.Get()) return animation.Get()->GetFPS();
			return (framesPerSecond > 0.0f) ? framesPerSecond : 12.0f;
		}

		f32 GetEffectiveFrameDuration(int32 index) const
		{
			if (animation.IsValid() && animation.Get())
			{
				const auto* f = animation.Get()->GetFrame(index);
				return f ? f->frameDuration : 1.0f;
			}
			return 1.0f;
		}

		ESpriteAnimationPlayMode GetEffectivePlayMode() const
		{
			if (animation.IsValid() && animation.Get()) return animation.Get()->GetPlayMode();
			return playMode;
		}
	};
}