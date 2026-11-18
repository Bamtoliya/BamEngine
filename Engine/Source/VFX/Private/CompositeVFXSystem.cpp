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
#include <fmod.hpp>

void Engine::VFXSoundChannelStop::operator()(FMOD::Channel* channel) const noexcept
{
	if (AudioManager::IsCreated() && AudioManager::Get().GetFMODSystem())
		(void)channel->stop();
}

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
		if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
		{
			vfxComp.StopSoundTracks();
			continue;
		}
		UpdateVFXEntity(scene, entity, vfxComp, transform, dt);
	}
}

void CompositeVFXSystem::UpdateVFXEntity(Scene* scene, entt::entity entity, CompositeVFXComponent& vfxComp, const WorldTransformComponent& transform, f32 dt)
{
	CompositeEffect* effect = vfxComp.isPlaying ? vfxComp.effectHandle.Get() : nullptr;
	const f32 totalDuration = effect ? effect->GetTotalDuration() : 0.0f;
	if (!std::isfinite(totalDuration) || totalDuration <= 0.0f)
	{
		vfxComp.StopSoundTracks();
		return;
	}
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
		// 환경 외력(바람 및 중력 배율)이 설정되어 있을 경우 각 이미터 인스턴스에 실시간 합성
		if (vfxComp.gravityMultiplier != 1.0f || glm::length2(vfxComp.externalWind) > 0.0001f)
		{
			for (size_t eIdx = 0; eIdx < emitters.size() && eIdx < tempComp.instances.size(); ++eIdx)
			{
				auto& inst = tempComp.instances[eIdx];
				const auto& emitterData = emitters[eIdx];
				const vec3 baseGravity = emitterData.gravity * vfxComp.gravityMultiplier;
				const vec3 totalGravity = baseGravity + vec3(0.0f, emitterData.buoyancy, 0.0f);
				const vec3 totalWind = emitterData.wind + vfxComp.externalWind;
				const bool isWorld = (emitterData.simulationSpace == EParticleSimulationSpace::World);
				const mat4 invWorldMat = isWorld ? mat4(1.0f) : glm::inverse(trackTransform.worldMatrix);
				const vec3 localWind = isWorld ? totalWind : vec3(invWorldMat * vec4(totalWind, 0.0f));
				for (auto& p : inst.particles)
				{
					p.velocity += (totalGravity - emitterData.gravity) * dt;
					if (emitterData.drag > 0.0f)
						p.velocity += (localWind - emitterData.wind) * (emitterData.drag * dt);
				}
			}
		}
		ParticleSystem::UpdateEffectComponent(scene, tempComp, trackTransform, dt);
		rt.emitterInstances = std::move(tempComp.instances);
		rt.isInitialized = tempComp.isInitialized;
	}

	RefreshFlashLight(vfxComp);

	// 5. 사운드 트랙 (SFX) 타이밍 1회 트리거
	const auto& soundTracks = effect->GetSoundTracks();
	if (vfxComp.soundRuntimes.size() != soundTracks.size())
	{
		vfxComp.soundRuntimes.clear();
		vfxComp.soundRuntimes.resize(soundTracks.size());
	}

	for (std::size_t i = 0; i < soundTracks.size(); ++i)
	{
		const auto& track = soundTracks[i];
		auto& runtime = vfxComp.soundRuntimes[i];
		const float time = vfxComp.currentTime;

		if (!track.enabled || track.isMuted || !std::isfinite(track.startTime))
		{
			runtime.channel.reset();
			continue;
		}

		if (vfxComp.previewSeeking)
		{
			runtime.channel.reset();
			if (time >= track.startTime)
				runtime.triggered = true;
			continue;
		}

		const bool hasDuration = std::isfinite(track.playDuration) && track.playDuration > 0.0f;
		const float end = hasDuration ? std::min(totalDuration, track.startTime + track.playDuration) : totalDuration;
		if (time < track.startTime || time >= end)
		{
			runtime.channel.reset();
			continue;
		}

		const float fade = std::isfinite(track.fadeOutDuration)
			? std::clamp(track.fadeOutDuration, 0.0f, end - track.startTime)
			: 0.0f;
		const float gain = fade > 0.0f ? std::clamp((end - time) / fade, 0.0f, 1.0f) : 1.0f;
		const float volume = std::clamp(track.volume, 0.0f, 1.0f) * gain;

		if (runtime.triggered)
		{
			if (runtime.channel && runtime.channel->setVolume(volume) != FMOD_OK)
				runtime.channel.reset();
			continue;
		}

		runtime.triggered = true;
		auto* clip = track.audioClip.Get();
		if (!clip || !clip->GetFMODSound() || !AudioManager::IsCreated())
			continue;

		auto& audio = AudioManager::Get();
		auto* system = audio.GetFMODSystem();
		FMOD::Channel* channel = nullptr;
		if (!system || system->playSound(clip->GetFMODSound(), audio.GetSFXGroup(), true, &channel) != FMOD_OK || !channel)
			continue;

		runtime.channel.reset(channel);
		if (channel->setVolume(volume) != FMOD_OK ||
			channel->setVolumeRamp(true) != FMOD_OK ||
			channel->setPaused(false) != FMOD_OK)
			runtime.channel.reset();
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

			rt.headA = currentPosA;
			rt.headB = currentPosB;
			rt.hasHead = true;

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
			rt.hasHead = false;
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
		const f32 duration = std::max(track.duration, 0.001f);
		f32 factor = 0.0f;
		if (track.useIntensityCurve && !track.intensityCurve.IsEmpty())
		{
			factor = track.intensityCurve.Evaluate(std::clamp(time / duration, 0.0f, 1.0f), 0.0f);
		}
		else
		{
			const f32 attack = std::max(track.attackTime, 0.001f);
			const f32 decay = std::max(track.decayTime, 0.001f);
			factor = time < attack ? time / attack : std::max(0.0f, 1.0f - (time - attack) / decay);
		}
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
		const auto* effect = vfxComp.effectHandle.Get();
		if (!effect || vfxComp.isFinished) continue;

		bool emittedFromTracks = false;
		for (const auto& track : effect->GetLightTracks())
		{
			if (!track.enabled || track.isMuted || !std::isfinite(track.radius) || track.radius <= 0.0f)
				continue;

			const f32 time = vfxComp.currentTime - track.startTime;
			if (time < 0.0f || time > track.duration) continue;
			const f32 duration = std::max(track.duration, 0.001f);
			f32 factor = 0.0f;
			if (track.useIntensityCurve && !track.intensityCurve.IsEmpty())
			{
				factor = track.intensityCurve.Evaluate(std::clamp(time / duration, 0.0f, 1.0f), 0.0f);
			}
			else
			{
				const f32 attack = std::max(track.attackTime, 0.001f);
				const f32 decay = std::max(track.decayTime, 0.001f);
				factor = time < attack ? time / attack : std::max(0.0f, 1.0f - (time - attack) / decay);
			}
			const f32 intensity = track.peakIntensity * factor;
			if (!std::isfinite(intensity) || intensity <= 0.0f) continue;

			vec3 position(transform.worldMatrix * vec4(track.localPosition, 1.0f));
			// 지면(y = 0)과 동일한 높이에 놓여 N dot L = 0 으로 조명이 소실되는 현상 방지
			if (position.y >= -0.05f && position.y < 0.35f)
				position.y = 0.35f;

			const vec3 color = track.lightColor;
			if (glm::any(glm::isnan(position)) || glm::any(glm::isinf(position))) continue;
			if (glm::any(glm::isnan(color)) || glm::any(glm::isinf(color))) continue;

			GPULight light{};
			light.Position = position;
			light.Intensity = intensity;
			light.Range = track.radius;
			light.Color = glm::max(color, vec3(0.0f));
			light.PackedFlags = packedFlags;
			light.AttenuationCoeff = flashAttenuationCoefficients;
			output[count++] = light;
			emittedFromTracks = true;
			if (count == output.size()) return count;
		}

		if (!emittedFromTracks && vfxComp.hasActiveLight)
		{
			vec3 position(transform.worldMatrix * vec4(vfxComp.activeLightOffset, 1.0f));
			if (position.y >= -0.05f && position.y < 0.35f)
				position.y = 0.35f;
			const vec3 color = vfxComp.activeLightColor;
			if (!glm::any(glm::isnan(position)) && !glm::any(glm::isinf(position)) &&
				!glm::any(glm::isnan(color)) && !glm::any(glm::isinf(color)))
			{
				GPULight light{};
				light.Position = position;
				light.Intensity = vfxComp.activeLightIntensity;
				light.Range = vfxComp.activeLightRadius;
				light.Color = glm::max(color, vec3(0.0f));
				light.PackedFlags = packedFlags;
				light.AttenuationCoeff = flashAttenuationCoefficients;
				output[count++] = light;
				if (count == output.size()) return count;
			}
		}
	}
	return count;
}

void CompositeVFXSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	// 불투명·마스크드 트랙은 Geometry, 알파·가산·데칼 트랙은 ForwardTransparent로 제출한다.
	vector<ViewportCameraInfo> targets;
	for (const auto& viewport : Renderer::Get().GetActiveViewportCameras())
	{
		const auto* pass = viewport.renderPass;
		if (!viewport.hasCamera || !viewport.acceptsSceneSubmissions || !pass || !pass->IsEnabled()) continue;

		const auto type = pass->GetPassType();
		if (type != ERenderPassType::Geometry && type != ERenderPassType::ForwardTransparent) continue;
		if (pass->GetAcceptedBlendModes() == EBlendMode::None) continue;

		if (std::ranges::find(targets, viewport.renderPass, &ViewportCameraInfo::renderPass) == targets.end())
			targets.push_back(viewport);
	}
	if (targets.empty()) return;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;
		for (const auto& target : targets)
			SubmitSceneVFX(scene, target.cameraBuffer, target.renderPass->GetID());
	}
}

static bool BuildTrailStrip(const std::vector<VFXTrailSamplePoint>& samples, const VFXTrailTrack& track, const vec3& cameraPosition, DynamicGeometryDrawCommand& out)
{
	const size_t count = samples.size();
	if (count < 2) return false;

	std::vector<vec3> centers(count);
	for (size_t i = 0; i < count; ++i) centers[i] = (samples[i].posA + samples[i].posB) * 0.5f;

	out.topology = ETopology::TriangleStrip;
	out.positions.reserve(count * 2);
	out.attributes.reserve(count * 2);

	vec3 prevSide(0.0f);
	for (size_t i = 0; i < count; ++i)
	{
		// 중앙 차분 접선: 앞뒤 세그먼트 방향의 평균이라 이음새에서 양쪽 사각형의 모서리가 일치한다.
		const vec3 tangentRaw = centers[std::min(i + 1, count - 1)] - centers[i == 0 ? 0 : i - 1];
		const f32 tangentLen = glm::length(tangentRaw);
		// 폭 방향 = 기준 축 × 접선. OffsetAxis는 A-B 축, ViewFacing은 점→카메라 방향을 기준 축으로 삼아 띠 면이 카메라를 향한다.
		const vec3 axis = track.alignment == EVFXTrailAlignment::ViewFacing ? cameraPosition - centers[i] : samples[i].posA - samples[i].posB;
		vec3 side = glm::cross(axis, tangentRaw);
		const f32 sideLen = glm::length(side);
		if (tangentLen < 1e-6f || sideLen < 1e-6f)
		{
			if (glm::dot(prevSide, prevSide) == 0.0f) continue; // 방향을 정할 수 없는 첫 점은 건너뛴다.
			side = prevSide;
		}
		else
		{
			side /= sideLen;
			// 진행 방향이 갑자기 뒤집혀도 띠가 꼬이지(나비 모양) 않도록 이전 점과 같은 쪽을 유지한다.
			if (glm::dot(side, prevSide) < 0.0f) side = -side;
		}
		prevSide = side;

		const vec3 tangentDir = tangentLen < 1e-6f ? glm::normalize(glm::cross(prevSide, vec3(0.0f, 1.0f, 0.0f)) + vec3(1e-6f)) : tangentRaw / tangentLen;
		const vec3 normal = glm::normalize(glm::cross(side, tangentDir)); // 기존 쿼드와 같은 -up 방향
		const f32 normAge = std::clamp(samples[i].age / std::max(track.lifetime, 0.001f), 0.0f, 1.0f);
		const f32 halfWidth = track.trailWidth * glm::mix(1.0f, track.taperRatio, normAge) * 0.5f;
		const vec4 color = glm::mix(track.colorStart, track.colorEnd, normAge);
		const f32 u = static_cast<f32>(i) / static_cast<f32>(count - 1);

		// QuadMesh와 같은 UV 규칙: +폭 쪽 v=0, -폭 쪽 v=1, 진행 방향이 u
		out.positions.emplace_back(centers[i] + side * halfWidth);
		out.attributes.emplace_back(normal, vec2(u, 0.0f), tangentDir, side, color);
		out.positions.emplace_back(centers[i] - side * halfWidth);
		out.attributes.emplace_back(normal, vec2(u, 1.0f), tangentDir, side, color);
	}
	return out.positions.size() >= 4;
}

void CompositeVFXSystem::SubmitSceneVFX(Scene* scene, const CameraBuffer& cameraBuffer, uint32 targetPassId)
{
	if (!scene) return;

	RenderPass* renderPass = RenderPassManager::Get().GetRenderPassByID(targetPassId);
	if (!renderPass) return;

	EBlendMode acceptedBlendModes = renderPass->GetAcceptedBlendModes();
	// 프리뷰 Geometry 패스는 메인 씬 유입만 막으며, 자체 이펙트는 불투명·마스크드만 받는다.
	if (acceptedBlendModes == EBlendMode::None && renderPass->GetPassType() == ERenderPassType::Geometry)
		acceptedBlendModes = EBlendMode::Opaque | EBlendMode::Masked;

	auto& renderer = Renderer::Get();
	auto& resourceManager = ResourceManager::Get();
	Mesh* defaultQuadMesh = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh").Get();
	MaterialInterface* defaultMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Engine://Material/SpriteMaterial.bammat.json").Get();
	if (!defaultMaterial)
		defaultMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Resources/Material/SpriteMaterial.bammat.json").Get();
	MaterialInterface* defaultAlphaBlendMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Engine://Material/ParticleMaterial_AlphaBlend.bammat.json").Get();
	if (!defaultAlphaBlendMaterial)
		defaultAlphaBlendMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Resources/Material/ParticleMaterial_AlphaBlend.bammat.json").Get();
	if (!defaultAlphaBlendMaterial)
		defaultAlphaBlendMaterial = defaultMaterial;

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
			if ((acceptedBlendModes & material->GetBlendMode()) == EBlendMode::None)
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
			MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultAlphaBlendMaterial;
			if (!mesh || !material) continue;

			// 렌더 패스가 특정 블렌드 모드를 명시적으로 제한하고 있는 경우 검사
			if ((acceptedBlendModes & material->GetBlendMode()) == EBlendMode::None)
				continue;

			const f32 duration = std::max(track.duration, 0.001f);
			const f32 progress = track.loop ? std::fmod(trackLocalTime, duration) / duration : std::clamp(trackLocalTime / duration, 0.0f, 1.0f);

			// 스케일 보간 (커브 활성화 시 3차 베지어 곡선 가중치 적용)
			vec3 currentScale;
			if (track.useScaleCurve && !track.scaleCurve.IsEmpty())
			{
				const f32 scaleProgress = track.scaleCurve.Evaluate(progress, progress);
				currentScale = glm::mix(track.scaleStart, track.scaleEnd, scaleProgress);
			}
			else
			{
				currentScale = glm::mix(track.scaleStart, track.scaleEnd, progress);
			}

			const vec3 currentRotEuler = track.initialRotation + track.rotationRate * trackLocalTime;

			// 컬러 및 알파 보간 (알파 커브 활성화 시 3차 베지어 곡선 직접 평가 적용)
			vec4 currentColor = glm::mix(track.colorStart, track.colorEnd, progress);
			if (track.useAlphaCurve && !track.alphaCurve.IsEmpty())
			{
				currentColor.a = track.alphaCurve.Evaluate(progress, currentColor.a);
			}

			const vec2 currentUVOffset = track.uvScrollSpeed * trackLocalTime;

			mat4 localMat = glm::translate(mat4(1.0f), track.localPosition) *
				glm::mat4_cast(glm::quat(glm::radians(currentRotEuler))) *
				glm::scale(mat4(1.0f), currentScale);
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
				texture = resourceManager.GetResourceHandle<Texture>(L"Engine://Texture/Particle_Ring.png").Get();
				if (!texture)
					texture = resourceManager.GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png").Get();
			}

			if (texture && texture->GetRHITexture() && mesh->GetIndexBuffer())
			{
				SpriteDrawCommand cmd;
				cmd.mesh = mesh;
				cmd.texture = texture;
				cmd.material = material;
				cmd.color = vec4(vec3(currentColor) * track.emissiveIntensity, currentColor.a);
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
			MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultAlphaBlendMaterial;
			if (!mesh || !material) continue;

			if ((acceptedBlendModes & material->GetBlendMode()) == EBlendMode::None)
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
				texture = resourceManager.GetResourceHandle<Texture>(L"Engine://Texture/Particle_SoftCircle.png").Get();
				if (!texture)
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
		const bool useTrailStrip = renderer.GetRHI() && renderer.GetRHI()->SupportsDynamicVertexBuffers();
		for (size_t trackIdx = 0; trackIdx < trailTracks.size(); ++trackIdx)
		{
			const auto& track = trailTracks[trackIdx];
			if (!track.enabled || track.isMuted) continue;
			if (trackIdx >= vfxComp.trailRuntimes.size()) continue;

			const auto& rt = vfxComp.trailRuntimes[trackIdx];
			if (rt.points.empty()) continue;

			Mesh* mesh = defaultQuadMesh;
			MaterialInterface* material = track.materialHandle.Get() ? track.materialHandle.Get() : defaultAlphaBlendMaterial;
			if (!mesh || !material) continue;

			if ((acceptedBlendModes & material->GetBlendMode()) == EBlendMode::None)
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

			// 저장된 점 + 현재 머리 위치를 제어점으로 사용한다.
			std::vector<VFXTrailSamplePoint> controls(rt.points.begin(), rt.points.end());
			if (rt.hasHead && glm::distance(rt.headA, controls.back().posA) > 1e-4f)
				controls.push_back({ rt.headA, rt.headB, 0.0f });
			if (controls.size() < 2) continue;

			// Catmull-Rom으로 제어점 사이를 보간해 꺾인 직선 대신 곡선을 만들고, 세그먼트 사이 각도를 줄여 이음새 틈을 줄인다.
			constexpr int kSubdivisions = 4;
			auto catmullRom = [](const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, f32 t)
				{
					const f32 t2 = t * t;
					const f32 t3 = t2 * t;
					return 0.5f * (2.0f * p1 + (p2 - p0) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (3.0f * p1 - p0 - 3.0f * p2 + p3) * t3);
				};

			std::vector<VFXTrailSamplePoint> samples;
			samples.reserve((controls.size() - 1) * kSubdivisions + 1);
			for (size_t c = 0; c + 1 < controls.size(); ++c)
			{
				const auto& c0 = controls[c == 0 ? 0 : c - 1];
				const auto& c1 = controls[c];
				const auto& c2 = controls[c + 1];
				const auto& c3 = controls[std::min(c + 2, controls.size() - 1)];
				for (int s = 0; s < kSubdivisions; ++s)
				{
					const f32 t = static_cast<f32>(s) / static_cast<f32>(kSubdivisions);
					samples.push_back({ catmullRom(c0.posA, c1.posA, c2.posA, c3.posA, t), catmullRom(c0.posB, c1.posB, c2.posB, c3.posB, t), glm::mix(c1.age, c2.age, t) });
				}
			}
			samples.push_back(controls.back());

			if (useTrailStrip)
			{
				DynamicGeometryDrawCommand strip;
				strip.texture = texture;
				strip.material = material;
				strip.pickingId = pickingId;
				if (BuildTrailStrip(samples, track, cameraBuffer.cameraPosition, strip))
					renderer.SubmitDynamicGeometry(std::move(strip), targetPassId);
				continue;
			}

			// 동적 정점 버퍼를 지원하지 않는 백엔드(SDL GPU): 세그먼트별 쿼드로 대체한다. 곡선 구간 이음새가 남는다.

			// 연속된 보간 점 쌍을 순회하며 쿼드 세그먼트 빌드 및 제출
			const size_t ptCount = samples.size();
			for (size_t p = 1; p < ptCount; ++p)
			{
				const auto& p0 = samples[p - 1];
				const auto& p1 = samples[p];

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
				orientMat[0] = vec4(dirNorm * (segLen * 0.5f), 0.0f);
				orientMat[1] = vec4(rightNorm * (width * 0.5f), 0.0f);
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