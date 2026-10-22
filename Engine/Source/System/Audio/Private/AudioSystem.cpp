#include "AudioSystem.h"
#include "AudioManager.h"
#include "AudioComponents.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include <fmod.hpp>
#include <fmod_errors.h>

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
		if (scene != nullptr)
		{
			UpdateListeners(scene->GetRegistry());
			UpdateSources(scene->GetRegistry());
		}
	}
}

namespace
{
	vec3 g_LastListenerPos{ 0.f };
	bool g_HasActiveListener = false;
}

void AudioSystem::UpdateListeners(entt::registry& registry)
{
	FMOD::System* fmodSystem = AudioManager::Get().GetFMODSystem();
	if (!fmodSystem) return;

	auto view = registry.view<AudioListenerComponent>();
	for (auto entity : view)
	{
		const auto& listener = view.get<AudioListenerComponent>(entity);
		if (!listener.isActive) continue;

		vec3 pos(0.f);
		vec3 forward(0.f, 0.f, 1.f);
		vec3 up(0.f, 1.f, 0.f);

		if (const auto* worldTransform = registry.try_get<WorldTransformComponent>(entity))
		{
			const mat4& m = worldTransform->worldMatrix;
			pos = vec3(m[3]);
			if (glm::length(vec3(m[2])) > 0.001f) forward = glm::normalize(vec3(m[2]));
			if (glm::length(vec3(m[1])) > 0.001f) up = glm::normalize(vec3(m[1]));
		}
		else if (const auto* localTransform = registry.try_get<TransformComponent>(entity))
		{
			pos = localTransform->position;
		}

		FMOD_VECTOR fPos = { pos.x, pos.y, pos.z };
		FMOD_VECTOR fVel = { 0.f, 0.f, 0.f };
		FMOD_VECTOR fFwd = { forward.x, forward.y, forward.z };
		FMOD_VECTOR fUp = { up.x, up.y, up.z };

		fmodSystem->set3DListenerAttributes(0, &fPos, &fVel, &fFwd, &fUp);
		g_LastListenerPos = pos;
		g_HasActiveListener = true;
		break;
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

		vec3 pos(0.f);
		if (const auto* worldTransform = registry.try_get<WorldTransformComponent>(entity))
		{
			pos = vec3(worldTransform->worldMatrix[3]);
		}
		else if (const auto* localTransform = registry.try_get<TransformComponent>(entity))
		{
			pos = localTransform->position;
		}

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
			if (!source.clip.IsValid())
			{
				BAM_LOG(Warn, "Engine","AudioSystem", "Cannot play AudioSource: AudioClip is not assigned");
			}
			else
			{
				AudioClip* clipRes = source.clip.Get();
				if (!clipRes || !clipRes->GetFMODSound())
				{
					BAM_LOG(Warn, "Engine","AudioSystem", "Cannot play AudioSource: FMOD Sound is null");
				}
				else
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
							source.m_Channel->setMode(FMOD_3D | FMOD_3D_LINEARROLLOFF);
							source.m_Channel->set3DAttributes(&fPos, &fVel);
							source.m_Channel->set3DMinMaxDistance(source.minDistance, source.maxDistance);
							source.m_Channel->set3DSpread(0.f); // 스테레오 음원도 3D 포인트 점음원으로 강제 수렴
						}
						else
						{
							source.m_Channel->setMode(FMOD_2D);
						}

						source.m_Channel->setPaused(false);
						source.m_IsPlaying = true;

						f32 dist = glm::distance(pos, g_LastListenerPos);
						BAM_LOG(Info, "Engine","AudioSystem", "Playing 3D Audio! Listener: ({:.1f}, {:.1f}, {:.1f}), Source: ({:.1f}, {:.1f}, {:.1f}), Distance: {:.1f} (Max: {:.1f})",
							g_LastListenerPos.x, g_LastListenerPos.y, g_LastListenerPos.z, pos.x, pos.y, pos.z, dist, source.maxDistance);
					}
					else
					{
						BAM_LOG(Error, "Engine","AudioSystem", "Failed to play sound: {}", FMOD_ErrorString(res));
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