#include "SpriteAnimationSystem.h"
#include "Components/Render/SpriteAnimationComponent.h"
#include "Components/Render/SpriteRendererComponent.h"
#include "Scene.h"
#include "Components/Core/FlagComponent.h"

SpriteAnimationSystem* SpriteAnimationSystem::Create(void* arg)
{
	auto* instance = new SpriteAnimationSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void SpriteAnimationSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	if (!std::isfinite(dt) || dt <= 0.0f) return;
	for (Scene* scene : activeScenes)
	{
		if (!scene || !scene->IsActive() || scene->IsPaused()) continue;
		auto& world = scene->GetRegistry();
		auto view = world.view<SpriteAnimationComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			const auto* flag = world.try_get<FlagComponent>(entity);
			if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
				HasFlag(flag->flags, EEntityFlag::Dead)))
			{
				continue;
			}
			auto& anim = view.get<SpriteAnimationComponent>(entity);
			auto& renderer = view.get<SpriteRendererComponent>(entity);
			if (!anim.isPlaying || anim.frames.empty()) continue;
			const int32 frameCount = static_cast<int32>(anim.frames.size());
			if (frameCount <= 0) continue;
			if (frameCount == 1)
			{
				anim.currentFrameIndex = 0;
				renderer.spriteHandle = anim.frames[0];
				continue;
			}
			const f32 fps = anim.framesPerSecond > 0.0f ? anim.framesPerSecond : 12.0f;
			const f32 frameDuration = std::max(1.0f / fps, 0.001f);
			anim.elapsedTime += dt * std::abs(anim.playRate);
			// 프레임 드롭이나 극단적인 delta 스파이크 시 무한 루프 방지 (최대 2바퀴 분량으로 컷오프)
			const int32 maxSteps = frameCount * 2;
			int32 stepCount = 0;
			while (anim.elapsedTime >= frameDuration && stepCount < maxSteps)
			{
				anim.elapsedTime -= frameDuration;
				++stepCount;
				const bool forward = (anim.playRate >= 0.0f) ^ anim.isReversed;
				if (anim.playMode == EFlipbookPlayMode::Loop)
				{
					if (forward)
						anim.currentFrameIndex = (anim.currentFrameIndex + 1) % frameCount;
					else
						anim.currentFrameIndex = (anim.currentFrameIndex - 1 + frameCount) % frameCount;
				}
				else if (anim.playMode == EFlipbookPlayMode::Once)
				{
					if (forward)
					{
						if (anim.currentFrameIndex + 1 < frameCount)
						{
							anim.currentFrameIndex++;
						}
						else
						{
							anim.isPlaying = false;
							anim.isFinished = true;
							break;
						}
					}
					else
					{
						if (anim.currentFrameIndex > 0)
						{
							anim.currentFrameIndex--;
						}
						else
						{
							anim.isPlaying = false;
							anim.isFinished = true;
							break;
						}
					}
				}
				else if (anim.playMode == EFlipbookPlayMode::PingPong)
				{
					if (forward)
					{
						if (anim.currentFrameIndex + 1 < frameCount)
						{
							anim.currentFrameIndex++;
						}
						else
						{
							anim.isReversed = !anim.isReversed;
							anim.currentFrameIndex = std::max(0, frameCount - 2);
						}
					}
					else
					{
						if (anim.currentFrameIndex > 0)
						{
							anim.currentFrameIndex--;
						}
						else
						{
							anim.isReversed = !anim.isReversed;
							anim.currentFrameIndex = std::min(frameCount - 1, 1);
						}
					}
				}
			}
			// 안전 상한을 초과한 누적 잔여 시간은 리셋하여 다음 프레임 폭주 방지
			if (stepCount >= maxSteps) anim.elapsedTime = 0.0f;
			if (anim.currentFrameIndex >= 0 && anim.currentFrameIndex < frameCount)
				renderer.spriteHandle = anim.frames[anim.currentFrameIndex];
		}
	}
}