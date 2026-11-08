#include "CompositeVFXSystem.h"
#include "Components/Render/CompositeVFXComponent.h"
#include "Components/Render/LightComponent.h"
#include "Components/Render/SpriteRendererComponent.h"
#include "CompositeEffect.h"
#include "ParticleEffect.h"
#include "ParticleSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "RenderPassManager.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Texture.h"
#include "Sprite.h"
#include "SpriteAnimation.h"
#include "Scene.h"
#include "PickingManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace Engine
{
	EResult CompositeVFXSystem::Initialize(void* arg)
	{
		return EResult::Success;
	}

	void CompositeVFXSystem::Free()
	{
	}

	CompositeVFXSystem* CompositeVFXSystem::Create(void* arg)
	{
		auto* instance = new CompositeVFXSystem();
		if (instance->Initialize(arg) != EResult::Success)
		{
			delete instance;
			return nullptr;
		}
		return instance;
	}

	void CompositeVFXSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
	{
		for (Scene* scene : activeScenes)
		{
			if (!scene) continue;
			SimulateSceneVFX(scene, dt);
		}
	}

	void CompositeVFXSystem::SimulateSceneVFX(Scene* scene, f32 dt)
	{
		if (!scene) return;

		auto view = scene->GetRegistry().view<CompositeVFXComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, vfxComp, transform, flag] : view.each())
		{
			if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
			UpdateVFXEntity(scene, entity, vfxComp, transform, dt);
		}
	}

	void CompositeVFXSystem::UpdateVFXEntity(Scene* scene, entt::entity entity, CompositeVFXComponent& vfxComp, const WorldTransformComponent& transform, f32 dt)
	{
		if (!vfxComp.isPlaying) return;

		CompositeEffect* effect = vfxComp.effectHandle.Get();
		if (!effect) return;

		const f32 totalDuration = effect->GetTotalDuration();
		vfxComp.prevTime = vfxComp.currentTime;
		vfxComp.currentTime += dt * vfxComp.playRate;

		// 1. 전체 지속 시간 초과 처리 (루프 / 정지)
		if (vfxComp.currentTime >= totalDuration)
		{
			if (vfxComp.isLooping || effect->IsLooping())
			{
				vfxComp.currentTime = std::fmod(vfxComp.currentTime, totalDuration);
				vfxComp.prevTime = 0.0f;
				vfxComp.ResetRuntimes();
			}
			else
			{
				vfxComp.currentTime = totalDuration;
				vfxComp.isFinished = true;
				vfxComp.isPlaying = false;
			}
		}

		// 2. 파티클 트랙 시뮬레이션
		const auto& particleTracks = effect->GetParticleTracks();
		if (vfxComp.particleRuntimes.size() != particleTracks.size())
		{
			vfxComp.particleRuntimes.resize(particleTracks.size());
		}

		for (size_t i = 0; i < particleTracks.size(); ++i)
		{
			const auto& track = particleTracks[i];
			auto& rt = vfxComp.particleRuntimes[i];
			if (!track.enabled || track.isMuted) continue;

			ParticleEffect* subEffect = track.effectHandle.Get();
			if (!subEffect) continue;

			const auto& emitters = subEffect->GetEmitters();
			if (rt.emitterInstances.size() != emitters.size())
			{
				rt.emitterInstances.resize(emitters.size());
			}

			const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
			const bool isTrackActive = (trackLocalTime >= 0.0f && (trackLocalTime <= track.duration || track.loop));

			WorldTransformComponent trackTransform = transform;
			mat4 localMat = glm::translate(mat4(1.0f), track.localPosition) *
				glm::mat4_cast(glm::quat(glm::radians(track.localRotation))) *
				glm::scale(mat4(1.0f), track.localScale);
			trackTransform.worldMatrix = transform.worldMatrix * localMat;

			ParticleEffectComponent tempComp;
			tempComp.effectHandle = track.effectHandle;
			tempComp.playState = isTrackActive ? EParticlePlayState::Playing : EParticlePlayState::Paused;
			tempComp.playRate = track.playbackSpeed * vfxComp.playRate;
			tempComp.instances = std::move(rt.emitterInstances);
			tempComp.isInitialized = rt.isInitialized;

			ParticleSystem::UpdateEffectComponent(scene, tempComp, trackTransform, dt);

			rt.emitterInstances = std::move(tempComp.instances);
			rt.isInitialized = tempComp.isInitialized;
		}

		// 3. 조명 섬광 트랙 합성 (Light Flash)
		const auto& lightTracks = effect->GetLightTracks();
		vfxComp.hasActiveLight = false;
		vfxComp.activeLightIntensity = 0.0f;

		for (const auto& track : lightTracks)
		{
			if (!track.enabled || track.isMuted) continue;
			const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
			if (trackLocalTime >= 0.0f && trackLocalTime <= track.duration)
			{
				const f32 attack = std::max(track.attackTime, 0.001f);
				const f32 decay = std::max(track.decayTime, 0.001f);
				f32 factor = 0.0f;
				if (trackLocalTime < attack)
				{
					factor = trackLocalTime / attack;
				}
				else
				{
					const f32 decayProgress = (trackLocalTime - attack) / decay;
					factor = std::max(0.0f, 1.0f - decayProgress);
				}

				const f32 intensity = track.peakIntensity * factor;
				if (intensity > vfxComp.activeLightIntensity)
				{
					vfxComp.activeLightIntensity = intensity;
					vfxComp.activeLightColor = track.lightColor;
					vfxComp.activeLightRadius = track.radius;
					vfxComp.activeLightOffset = track.localPosition;
					vfxComp.hasActiveLight = true;
				}
			}
		}

		// 4. 엔티티 상의 LightComponent 동기화
		auto* lightComp = scene->GetRegistry().try_get<LightComponent>(entity);
		if (vfxComp.hasActiveLight)
		{
			if (!lightComp)
			{
				lightComp = &scene->GetRegistry().emplace<LightComponent>(entity);
				lightComp->LightType = ELightType::Point;
			}
			lightComp->Color = vfxComp.activeLightColor;
			lightComp->Intensity = vfxComp.activeLightIntensity;
			lightComp->Range = vfxComp.activeLightRadius;
		}
		else if (lightComp && lightComp->LightType == ELightType::Point)
		{
			lightComp->Intensity = 0.0f;
		}
	}

	void CompositeVFXSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
	{
		auto& renderer = Renderer::Get();
		const auto geometryTargets = renderer.CollectGeometryViewportCameras();
		if (geometryTargets.empty()) return;

		for (Scene* scene : activeScenes)
		{
			if (!scene) continue;

			for (const auto& target : geometryTargets)
			{
				SubmitSceneVFX(scene, target.cameraBuffer, target.renderPass->GetID());
			}
		}
	}

	void CompositeVFXSystem::SubmitSceneVFX(Scene* scene, const CameraBuffer& cameraBuffer, uint32 targetPassId)
	{
		if (!scene) return;

		auto& renderer = Renderer::Get();
		auto& resourceManager = ResourceManager::Get();
		Mesh* defaultQuadMesh = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh").Get();
		MaterialInterface* defaultMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Engine://Material/SpriteMaterial.bammat.json").Get();
		if (!defaultMaterial)
			defaultMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Resources/Material/SpriteMaterial.bammat.json").Get();

		const vec3 camRight = glm::normalize(vec3(cameraBuffer.invViewMatrix[0]));
		const vec3 camUp = glm::normalize(vec3(cameraBuffer.invViewMatrix[1]));
		const vec3 camForward = glm::normalize(vec3(cameraBuffer.invViewMatrix[2]));

		auto view = scene->GetRegistry().view<CompositeVFXComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, vfxComp, transform, flag] : view.each())
		{
			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
			CompositeEffect* effect = vfxComp.effectHandle.Get();
			if (!effect) continue;

			const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

			// ── 스프라이트 플립북 트랙 렌더링 ──
			const auto& spriteTracks = effect->GetSpriteTracks();
			for (const auto& track : spriteTracks)
			{
				if (!track.enabled || track.isMuted) continue;

				const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
				if (trackLocalTime < 0.0f) continue;

				SpriteAnimation* anim = track.animationHandle.Get();
				if (!anim) continue;

				const f32 animDuration = anim->GetTotalDuration();
				const f32 effectiveDuration = (track.duration > 0.0f) ? track.duration : animDuration;
				if (!track.loop && trackLocalTime >= effectiveDuration) continue;

				const f32 playbackSpeed = (track.playbackSpeed > 0.0f) ? track.playbackSpeed : 1.0f;
				const f32 animEvalTime = trackLocalTime * playbackSpeed;
				if (!track.loop && animDuration > 0.0f && animEvalTime >= animDuration) continue;

				ResourceHandle<Sprite> spriteHandle = anim->GetSpriteAtTime(animEvalTime, track.loop);
				Sprite* sprite = spriteHandle.Get();
				if (!sprite) continue;

				Texture* texture = sprite->GetTexture();
				if (!texture || !texture->GetRHITexture()) continue;

				Mesh* mesh = defaultQuadMesh;
				MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultMaterial;
				if (!mesh || !material) continue;

				mat4 localMat = glm::translate(mat4(1.0f), track.localPosition) *
					glm::mat4_cast(glm::quat(glm::radians(track.localRotation))) *
					glm::scale(mat4(1.0f), track.localScale);
				mat4 finalWorld = transform.worldMatrix * localMat;

				if (track.billboardMode == ESpriteBillboardMode::CameraFacing)
				{
					const vec3 worldPos = vec3(finalWorld[3]);
					const vec3 scale = vec3(
						std::max(glm::length(vec3(finalWorld[0])), 0.0001f),
						std::max(glm::length(vec3(finalWorld[1])), 0.0001f),
						std::max(glm::length(vec3(finalWorld[2])), 0.0001f));

					const mat4 rotMatrix(
						vec4(camRight, 0.0f),
						vec4(camUp, 0.0f),
						vec4(camForward, 0.0f),
						vec4(0.0f, 0.0f, 0.0f, 1.0f));

					finalWorld = glm::translate(mat4(1.0f), worldPos) * rotMatrix * glm::scale(mat4(1.0f), scale);
				}

				mat4 quadMatrix;
				if (!sprite->TryGetQuadMatrix(quadMatrix)) continue;

				SpriteDrawCommand cmd;
				cmd.mesh = mesh;
				cmd.texture = texture;
				cmd.material = material;
				cmd.color = track.colorTint;
				cmd.uvTransform = sprite->GetUVTransform();
				cmd.pickingId = pickingId;
				cmd.worldMatrix = finalWorld * quadMatrix;
				renderer.SubmitSprite(cmd, targetPassId);
			}
		}
	}
}
