#include "AudioSystem.h"
#include "AudioManager.h"
#include "AudioComponents.h"
#include "Components/Core/WorldTransformComponent.h"
#include <fmod.hpp>

EResult AudioSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

AudioSystem* AudioSystem::Create(void* arg)
{
	auto* instance = new AudioSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void AudioSystem::Free()
{
}

void AudioSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt)
{
	UpdateListeners(globalRegistry);
	UpdateSources(globalRegistry);

	for (Scene* scene : activeScenes)
	{
		if (scene != nullptr && !scene->IsPaused())
		{
			UpdateListeners(scene->GetRegistry());
			UpdateSources(scene->GetRegistry());
		}
	}
}

void AudioSystem::UpdateListeners(entt::registry& registry)
{
	FMOD::System* fmodSystem = AudioManager::Get().GetFMODSystem();
	if (!fmodSystem) return;

	auto view = registry.view<AudioListenerComponent, WorldTransformComponent>();
	for (auto entity : view)
	{
		const auto& [listener, worldTransform] = view.get<AudioListenerComponent, WorldTransformComponent>(entity);
		if (!listener.isActive) continue;

		const mat4& worldMatrix = worldTransform.worldMatrix;
		vec3 pos = vec3(worldMatrix[3]);
		vec3 forward = glm::length(vec3(worldMatrix[2])) > 0.f ? glm::normalize(vec3(worldMatrix[2])) : vec3(0.f, 0.f, 1.f);
		vec3 up = glm::length(vec3(worldMatrix[1])) > 0.f ? glm::normalize(vec3(worldMatrix[1])) : vec3(0.f, 1.f, 0.f);

		FMOD_VECTOR fPos = { pos.x, pos.y, pos.z };
		FMOD_VECTOR fVel = { 0.f, 0.f, 0.f };
		FMOD_VECTOR fFwd = { forward.x, forward.y, forward.z };
		FMOD_VECTOR fUp = { up.x, up.y, up.z };

		fmodSystem->set3DListenerAttributes(0, &fPos, &fVel, &fFwd, &fUp);
		break; // 주 청취자 1개만 적용
	}
}

void AudioSystem::UpdateSources(entt::registry& registry)
{
	FMOD::System* fmodSystem = AudioManager::Get().GetFMODSystem();
	if (!fmodSystem) return;

	auto view = registry.view<AudioSourceComponent>();
	for (auto entity : view)
	{
		auto& source = view.get<AudioSourceComponent>(entity);

		const auto* worldTransform = registry.try_get<WorldTransformComponent>(entity);
		vec3 pos = worldTransform ? vec3(worldTransform->worldMatrix[3]) : vec3(0.f);
		FMOD_VECTOR fPos = { pos.x, pos.y, pos.z };
		FMOD_VECTOR fVel = { 0.f, 0.f, 0.f };

		// 1. Play on Awake
		if (source.playOnAwake && !source.m_AwakeTriggered)
		{
			source.m_AwakeTriggered = true;
			source.m_PlayRequested = true;
		}

		// 2. Stop 요청
		if (source.m_StopRequested)
		{
			if (source.m_Channel) { source.m_Channel->stop(); source.m_Channel = nullptr; }
			source.m_IsPlaying = false;
			source.m_StopRequested = false;
		}

		// 3. Pause / Resume 요청
		if (source.m_PauseRequested && source.m_Channel)
		{
			source.m_Channel->setPaused(true);
			source.m_PauseRequested = false;
		}
		if (source.m_ResumeRequested && source.m_Channel)
		{
			source.m_Channel->setPaused(false);
			source.m_ResumeRequested = false;
		}

		// 4. Play 요청
		if (source.m_PlayRequested)
		{
			source.m_PlayRequested = false;
			if (source.clip.IsValid())
			{
				AudioClip* clipRes = source.clip.Get();
				if (clipRes && clipRes->GetFMODSound())
				{
					if (source.m_Channel) source.m_Channel->stop();

					FMOD::ChannelGroup* group = clipRes->IsStream() ? AudioManager::Get().GetBGMGroup() : AudioManager::Get().GetSFXGroup();
					FMOD_RESULT res = fmodSystem->playSound(clipRes->GetFMODSound(), group, true, &source.m_Channel);
					if (res == FMOD_OK && source.m_Channel)
					{
						source.m_Channel->setVolume(source.volume);
						source.m_Channel->setPitch(source.pitch);
						source.m_Channel->setMode(source.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

						if (source.is3D)
						{
							source.m_Channel->setMode(FMOD_3D);
							source.m_Channel->set3DAttributes(&fPos, &fVel);
							source.m_Channel->set3DMinMaxDistance(source.minDistance, source.maxDistance);
						}
						else
						{
							source.m_Channel->setMode(FMOD_2D);
						}

						source.m_Channel->setPaused(false);
						source.m_IsPlaying = true;
					}
				}
			}
		}

		// 5. 재생 중인 채널의 위치 및 속성 동기화
		if (source.m_Channel)
		{
			bool isPlaying = false;
			FMOD_RESULT res = source.m_Channel->isPlaying(&isPlaying);
			if (res == FMOD_OK && isPlaying)
			{
				source.m_Channel->setVolume(source.volume);
				source.m_Channel->setPitch(source.pitch);
				if (source.is3D)
				{
					source.m_Channel->set3DAttributes(&fPos, &fVel);
					source.m_Channel->set3DMinMaxDistance(source.minDistance, source.maxDistance);
				}
			}
			else
			{
				source.m_Channel = nullptr;
				source.m_IsPlaying = false;
			}
		}
	}
}