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
		std::vector<entt::entity> toDestroy;
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
			const int32 frameCount = anim.GetEffectiveFrameCount();
			if (frameCount <= 0) continue;

			if (!anim.isPlaying)
			{
				if (anim.currentFrameIndex >= 0 && anim.currentFrameIndex < frameCount)
				{
					auto curSprite = anim.GetSpriteAtFrame(anim.currentFrameIndex);
					if (curSprite.IsValid()) renderer.spriteHandle = curSprite;
				}
				continue;
			}

			if (frameCount == 1)
			{
				anim.currentFrameIndex = 0;
				renderer.spriteHandle = anim.GetSpriteAtFrame(0);
				continue;
			}

			const f32 fps = anim.GetEffectiveFPS();
			const f32 baseFrameDuration = std::max(1.0f / fps, 0.001f);
			anim.elapsedTime += dt * std::abs(anim.playRate);

			const int32 maxSteps = frameCount * 2;
			int32 stepCount = 0;
			const auto playMode = anim.GetEffectivePlayMode();
			f32 currentRequiredDuration = baseFrameDuration * anim.GetEffectiveFrameDuration(anim.currentFrameIndex);

			while (anim.elapsedTime >= currentRequiredDuration && stepCount < maxSteps)
			{
				anim.elapsedTime -= currentRequiredDuration;
				++stepCount;
				const bool forward = (anim.playRate >= 0.0f) ^ anim.isReversed;
				if (playMode == ESpriteAnimationPlayMode::Loop)
				{
					if (forward)
						anim.currentFrameIndex = (anim.currentFrameIndex + 1) % frameCount;
					else
						anim.currentFrameIndex = (anim.currentFrameIndex - 1 + frameCount) % frameCount;
				}
				else if (playMode == ESpriteAnimationPlayMode::Once)
				{
					if (forward)
					{
						if (anim.currentFrameIndex + 1 < frameCount)
							anim.currentFrameIndex++;
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
							anim.currentFrameIndex--;
						else
						{
							anim.isPlaying = false;
							anim.isFinished = true;
							break;
						}
					}
				}
				else if (playMode == ESpriteAnimationPlayMode::PingPong)
				{
					if (forward)
					{
						if (anim.currentFrameIndex + 1 < frameCount)
							anim.currentFrameIndex++;
						else
						{
							anim.isReversed = !anim.isReversed;
							anim.currentFrameIndex = std::max(0, frameCount - 2);
						}
					}
					else
					{
						if (anim.currentFrameIndex > 0)
							anim.currentFrameIndex--;
						else
						{
							anim.isReversed = !anim.isReversed;
							anim.currentFrameIndex = std::min(frameCount - 1, 1);
						}
					}
				}
				currentRequiredDuration = baseFrameDuration * anim.GetEffectiveFrameDuration(anim.currentFrameIndex);
			}

			if (stepCount >= maxSteps) anim.elapsedTime = 0.0f;
			if (anim.currentFrameIndex >= 0 && anim.currentFrameIndex < frameCount)
				renderer.spriteHandle = anim.GetSpriteAtFrame(anim.currentFrameIndex);

			if (anim.isFinished)
			{
				if (anim.autoDeactivateOnFinish)
				{
					if (auto* flagComp = world.try_get<FlagComponent>(entity))
					{
						flagComp->SetActive(false);
					}
				}
				if (anim.autoDestroyOnFinish)
				{
					toDestroy.push_back(entity);
				}
			}
		}

		for (auto destroyEnt : toDestroy)
		{
			if (auto* entObj = scene->FindEntity(destroyEnt))
			{
				(void)scene->DestroyEntity(*entObj);
			}
		}
	}
}