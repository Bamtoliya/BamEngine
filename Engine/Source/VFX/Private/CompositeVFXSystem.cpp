#include "CompositeVFXSystem.h"
#include "CompositeVFXComponent.h"
#include "LightTypes.h"
#include "SpriteRendererComponent.h"
#include "CompositeEffect.h"
#include "ParticleEffect.h"
#include "ParticleSystem.h"
#include "WorldComponents.h"
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
#include "AudioManager.h"
#include "AudioClip.h"
#include "Scene.h"
#include "PickingManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>

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
	const f32 playbackDelta = dt * vfxComp.playRate;
	vfxComp.currentTime += playbackDelta;

	// 1. 전체 지속 시간 초과 처리 (루프 / 정지)
	if (vfxComp.currentTime >= totalDuration)
	{
		if (!vfxComp.previewSeeking && (vfxComp.isLooping || effect->IsLooping()))
		{
			vfxComp.currentTime = std::fmod(vfxComp.currentTime, totalDuration);
			vfxComp.prevTime = 0.0f;
			vfxComp.isFinished = false;
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
		tempComp.previewSeed = vfxComp.previewSeed;

		ParticleSystem::UpdateEffectComponent(scene, tempComp, trackTransform, dt);

		rt.emitterInstances = std::move(tempComp.instances);
		rt.isInitialized = tempComp.isInitialized;
	}

	RefreshFlashLight(vfxComp);

	// 5. 사운드 트랙 (SFX) 타이밍 1회 트리거
	const auto& soundTracks = effect->GetSoundTracks();
	if (vfxComp.soundPlayed.size() != soundTracks.size())
	{
		vfxComp.soundPlayed.assign(soundTracks.size(), false);
	}

	for (size_t i = 0; i < soundTracks.size(); ++i)
	{
		const auto& track = soundTracks[i];
		if (!track.enabled || track.isMuted) continue;

		if (vfxComp.currentTime < track.startTime)
		{
			vfxComp.soundPlayed[i] = false;
		}
		else if (!vfxComp.soundPlayed[i] && vfxComp.currentTime >= track.startTime)
		{
			AudioClip* clip = track.audioClip.Get();
			if (!vfxComp.previewSeeking && clip && clip->GetFMODSound())
			{
				AudioManager::Get().PlayOneShot(clip->GetFMODSound(), track.volume);
			}
			vfxComp.soundPlayed[i] = true;
		}
	}

	// 6. 트레일 / 리본 트랙 런타임 갱신
	const auto& trailTracks = effect->GetTrailTracks();
	if (vfxComp.trailRuntimes.size() != trailTracks.size())
	{
		vfxComp.trailRuntimes.resize(trailTracks.size());
	}

	for (size_t i = 0; i < trailTracks.size(); ++i)
	{
		const auto& track = trailTracks[i];
		auto& rt = vfxComp.trailRuntimes[i];
		if (!track.enabled || track.isMuted)
		{
			rt.Reset();
			continue;
		}

		// 기존 포인트 나이 증가 및 만료된 포인트 제거
		for (auto& pt : rt.points)
		{
			pt.age += std::max(0.0f, playbackDelta);
		}
		std::erase_if(rt.points, [&track](const VFXTrailSamplePoint& pt) {
			return pt.age >= track.lifetime;
		});

		const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
		const bool isActive = (trackLocalTime >= 0.0f) && (track.loop || trackLocalTime < track.duration);

		if (isActive)
		{
			const auto* worldTrans = scene->GetRegistry().try_get<WorldTransformComponent>(entity);
			const mat4 worldMat = worldTrans ? worldTrans->worldMatrix : mat4(1.0f);
			const vec3 currentPosA = vec3(worldMat * vec4(track.localOffsetA, 1.0f));
			const vec3 currentPosB = vec3(worldMat * vec4(track.localOffsetB, 1.0f));

			if (!rt.hasLastSpawn || glm::distance(currentPosA, rt.lastSpawnPosA) >= track.minVertexDistance)
			{
				rt.points.push_back({ currentPosA, currentPosB, 0.0f });
				rt.lastSpawnPosA = currentPosA;
				rt.lastSpawnPosB = currentPosB;
				rt.hasLastSpawn = true;
			}
		}
		else
		{
			rt.hasLastSpawn = false;
		}
	}

	// 7. 카메라 셰이크 트랙 진동 계산
	const auto& shakeTracks = effect->GetCameraShakeTracks();
	vec3 totalShake = vec3(0.0f);
	for (const auto& track : shakeTracks)
	{
		if (!track.enabled || track.isMuted) continue;
		const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
		if (trackLocalTime >= 0.0f && trackLocalTime < track.duration)
		{
			const f32 duration = std::max(track.duration, 0.001f);
			const f32 decay = 1.0f - (trackLocalTime / duration);
			const f32 phase = trackLocalTime * track.frequency * 6.2831853f;
			const vec3 shake = vec3(std::sin(phase), std::cos(phase * 1.33f), std::sin(phase * 0.77f)) * (track.amplitude * decay);
			totalShake += shake;
		}
	}
	vfxComp.currentCameraShakeOffset = totalShake;
}

void CompositeVFXSystem::RefreshFlashLight(CompositeVFXComponent& vfxComp)
{
	vfxComp.hasActiveLight = false;
	vfxComp.activeLightIntensity = 0.0f;

	const auto* effect = vfxComp.effectHandle.Get();
	if (!effect || vfxComp.isFinished) return;

	for (const auto& track : effect->GetLightTracks())
	{
		if (!track.enabled || track.isMuted || !std::isfinite(track.radius) || track.radius <= 0.0f)
			continue;

		const f32 time = vfxComp.currentTime - track.startTime;
		if (time < 0.0f || time > track.duration) continue;

		const f32 attack = std::max(track.attackTime, 0.001f);
		const f32 decay = std::max(track.decayTime, 0.001f);
		const f32 factor = time < attack ? time / attack : std::max(0.0f, 1.0f - (time - attack) / decay);
		const f32 intensity = track.peakIntensity * factor;
		if (!std::isfinite(intensity) || intensity <= vfxComp.activeLightIntensity) continue;

		vfxComp.activeLightIntensity = intensity;
		vfxComp.activeLightColor = track.lightColor;
		vfxComp.activeLightRadius = track.radius;
		vfxComp.activeLightOffset = track.localPosition;
		vfxComp.hasActiveLight = true;
	}
}

Engine::uint32 CompositeVFXSystem::CollectFlashLights(const Scene& scene, std::span<GPULight> output)
{
	if (output.empty()) return 0;

	const auto flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular;
	const uint32 packedFlags = PackLightFlags(static_cast<uint32>(flags), static_cast<uint32>(ELightType::Point),
		static_cast<uint32>(EAttenuationMode::Coefficients));
	const vec3 flashAttenuationCoefficients{ 1.0f, 0.09f, 0.032f };

	uint32 count = 0;
	auto view = scene.GetRegistry().view<const CompositeVFXComponent, const WorldTransformComponent, const FlagComponent>();
	for (auto [entity, vfxComp, transform, flag] : view.each())
	{
		if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		if (!vfxComp.hasActiveLight || !vfxComp.effectHandle.IsValid()) continue;

		const vec3 position(transform.worldMatrix * vec4(vfxComp.activeLightOffset, 1.0f));
		const vec3 color = vfxComp.activeLightColor;
		if (glm::any(glm::isnan(position)) || glm::any(glm::isinf(position))) continue;
		if (glm::any(glm::isnan(color)) || glm::any(glm::isinf(color))) continue;

		GPULight light{};
		light.Position = position;
		light.Intensity = vfxComp.activeLightIntensity;
		light.Range = vfxComp.activeLightRadius;
		light.Color = glm::max(color, vec3(0.0f));
		light.PackedFlags = packedFlags;
		light.AttenuationCoeff = flashAttenuationCoefficients;
		output[count++] = light;
		if (count == output.size()) break;
	}
	return count;
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

	RenderPass* renderPass = RenderPassManager::Get().GetRenderPassByID(targetPassId);
	if (!renderPass) return;

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

			// 렌더 패스가 특정 블렌드 모드를 명시적으로 제한하고 있는 경우 검사
			if (renderPass->GetAcceptedBlendModes() != EBlendMode::None &&
				!renderPass->IsAcceptsBlendMode(material->GetBlendMode()))
				continue;

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

		// ── 3D 메쉬 이펙트 트랙 렌더링 ──
		const auto& meshTracks = effect->GetMeshTracks();
		for (const auto& track : meshTracks)
		{
			if (!track.enabled || track.isMuted) continue;

			const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
			if (trackLocalTime < 0.0f) continue;
			if (!track.loop && trackLocalTime >= track.duration) continue;

			Mesh* mesh = track.meshHandle.Get() ? track.meshHandle.Get() : defaultQuadMesh;
			MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultMaterial;
			if (!mesh || !material) continue;

			// 렌더 패스가 특정 블렌드 모드를 명시적으로 제한하고 있는 경우 검사
			if (renderPass->GetAcceptedBlendModes() != EBlendMode::None &&
				!renderPass->IsAcceptsBlendMode(material->GetBlendMode()))
				continue;

			const f32 duration = std::max(track.duration, 0.001f);
			const f32 progress = track.loop ? std::fmod(trackLocalTime, duration) / duration : std::clamp(trackLocalTime / duration, 0.0f, 1.0f);

			// 스케일, 회전, 컬러 보간
			const vec3 currentScale = glm::mix(track.scaleStart, track.scaleEnd, progress);
			const vec3 currentRotEuler = track.initialRotation + track.rotationRate * trackLocalTime;
			const vec4 currentColor = glm::mix(track.colorStart, track.colorEnd, progress);
			const vec2 currentUVOffset = track.uvScrollSpeed * trackLocalTime;

			mat4 localMat = glm::translate(mat4(1.0f), track.localPosition) *
				glm::mat4_cast(glm::quat(glm::radians(currentRotEuler))) *
				glm::scale(mat4(1.0f), currentScale);
			mat4 finalWorld = transform.worldMatrix * localMat;

			Texture* texture = nullptr;
			const auto& texBindings = material->GetTextureBindings();
			if (!texBindings.empty() && texBindings[0].texture.Get())
			{
				texture = texBindings[0].texture.Get();
			}
			if (!texture)
			{
				texture = resourceManager.GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png").Get();
				if (!texture)
					texture = resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png").Get();
			}

			if (texture && texture->GetRHITexture() && mesh->GetIndexBuffer())
			{
				SpriteDrawCommand cmd;
				cmd.mesh = mesh;
				cmd.texture = texture;
				cmd.material = material;
				cmd.color = currentColor;
				cmd.uvTransform = vec4(currentUVOffset.x, currentUVOffset.y, 1.0f, 1.0f);
				cmd.pickingId = pickingId;
				cmd.worldMatrix = finalWorld;
				renderer.SubmitSprite(cmd, targetPassId);
			}
			else
			{
				StaticDrawCommand cmd;
				cmd.mesh = mesh;
				cmd.material = material;
				cmd.worldMatrix = finalWorld;
				cmd.pickingId = pickingId;
				renderer.SubmitStatic(cmd, targetPassId);
			}
		}

		// ── 4. 지면 투영 데칼 트랙 렌더링 ──
		const auto& decalTracks = effect->GetDecalTracks();
		for (const auto& track : decalTracks)
		{
			if (!track.enabled || track.isMuted) continue;

			const f32 trackLocalTime = vfxComp.currentTime - track.startTime;
			if (trackLocalTime < 0.0f || trackLocalTime >= track.duration) continue;

			Mesh* mesh = defaultQuadMesh;
			MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultMaterial;
			if (!mesh || !material) continue;

			if (renderPass->GetAcceptedBlendModes() != EBlendMode::None &&
				!renderPass->IsAcceptsBlendMode(material->GetBlendMode()))
				continue;

			// 페이드아웃 알파 계산
			f32 alpha = 1.0f;
			if (trackLocalTime > track.fadeStartTime)
			{
				const f32 fadeSpan = std::max(track.duration - track.fadeStartTime, 0.001f);
				alpha = 1.0f - ((trackLocalTime - track.fadeStartTime) / fadeSpan);
			}
			alpha = std::clamp(alpha, 0.0f, 1.0f);

			vec4 finalColor = track.colorTint;
			finalColor.a *= alpha;

			mat4 localMat = glm::translate(mat4(1.0f), track.localPosition) *
				glm::mat4_cast(glm::quat(glm::radians(track.localRotation))) *
				glm::scale(mat4(1.0f), vec3(track.size.x, track.size.y, 1.0f));
			mat4 finalWorld = transform.worldMatrix * localMat;

			Texture* texture = track.textureHandle.Get();
			if (!texture)
			{
				const auto& texBindings = material->GetTextureBindings();
				if (!texBindings.empty() && texBindings[0].texture.Get())
					texture = texBindings[0].texture.Get();
			}
			if (!texture)
			{
				texture = resourceManager.GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png").Get();
				if (!texture)
					texture = resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png").Get();
			}

			if (texture && texture->GetRHITexture() && mesh->GetIndexBuffer())
			{
				SpriteDrawCommand cmd;
				cmd.mesh = mesh;
				cmd.texture = texture;
				cmd.material = material;
				cmd.color = finalColor;
				cmd.uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f);
				cmd.pickingId = pickingId;
				cmd.worldMatrix = finalWorld;
				renderer.SubmitSprite(cmd, targetPassId);
			}
		}

		// ── 5. 트레일 / 리본 트랙 렌더링 (동적 세그먼트 스트립) ──
		const auto& trailTracks = effect->GetTrailTracks();
		for (size_t trackIdx = 0; trackIdx < trailTracks.size(); ++trackIdx)
		{
			const auto& track = trailTracks[trackIdx];
			if (!track.enabled || track.isMuted) continue;
			if (trackIdx >= vfxComp.trailRuntimes.size()) continue;

			const auto& rt = vfxComp.trailRuntimes[trackIdx];
			if (rt.points.size() < 2) continue;

			Mesh* mesh = defaultQuadMesh;
			MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultMaterial;
			if (!mesh || !material) continue;

			if (renderPass->GetAcceptedBlendModes() != EBlendMode::None &&
				!renderPass->IsAcceptsBlendMode(material->GetBlendMode()))
				continue;

			Texture* texture = track.textureHandle.Get();
			if (!texture)
			{
				const auto& texBindings = material->GetTextureBindings();
				if (!texBindings.empty() && texBindings[0].texture.Get())
					texture = texBindings[0].texture.Get();
			}
			if (!texture)
			{
				texture = resourceManager.GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png").Get();
				if (!texture)
					texture = resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png").Get();
			}

			if (!texture || !texture->GetRHITexture() || !mesh->GetIndexBuffer()) continue;

			// 연속된 포인트 쌍을 순회하며 쿼드 세그먼트 빌드 및 제출
			const size_t ptCount = rt.points.size();
			for (size_t p = 1; p < ptCount; ++p)
			{
				const auto& p0 = rt.points[p - 1];
				const auto& p1 = rt.points[p];

				const vec3 mid0 = (p0.posA + p0.posB) * 0.5f;
				const vec3 mid1 = (p1.posA + p1.posB) * 0.5f;
				const vec3 segDir = mid1 - mid0;
				const f32 segLen = glm::length(segDir);
				if (segLen < 0.0001f) continue;

				const vec3 center = (mid0 + mid1) * 0.5f;
				const vec3 dirNorm = segDir / segLen;
				const vec3 upNorm = glm::normalize((p0.posA - p0.posB) + (p1.posA - p1.posB));
				const vec3 rightNorm = glm::cross(upNorm, dirNorm);

				const f32 avgAge = (p0.age + p1.age) * 0.5f;
				const f32 normAge = std::clamp(avgAge / std::max(track.lifetime, 0.001f), 0.0f, 1.0f);
				const f32 widthFactor = glm::mix(1.0f, track.taperRatio, normAge);
				const f32 width = track.trailWidth * widthFactor;

				// 세그먼트 방향으로 정렬된 변환 행렬
				mat4 orientMat(1.0f);
				orientMat[0] = vec4(rightNorm * width, 0.0f);
				orientMat[1] = vec4(dirNorm * segLen, 0.0f);
				orientMat[2] = vec4(upNorm, 0.0f);
				orientMat[3] = vec4(center, 1.0f);

				const vec4 segColor = glm::mix(track.colorStart, track.colorEnd, normAge);

				// UV: 진행 방향을 따라 비율 매핑
				const f32 u0 = static_cast<f32>(p - 1) / static_cast<f32>(ptCount - 1);
				const f32 uSpan = 1.0f / static_cast<f32>(ptCount - 1);

				SpriteDrawCommand cmd;
				cmd.mesh = mesh;
				cmd.texture = texture;
				cmd.material = material;
				cmd.color = segColor;
				cmd.uvTransform = vec4(u0, 0.0f, uSpan, 1.0f);
				cmd.pickingId = pickingId;
				cmd.worldMatrix = orientMat;
				renderer.SubmitSprite(cmd, targetPassId);
			}
		}
	}
}