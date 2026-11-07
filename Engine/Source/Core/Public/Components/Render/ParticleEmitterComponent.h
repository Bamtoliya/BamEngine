#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Components/Render/SpriteRendererComponent.h"
#include <random>
#include <vector>

namespace Engine
{
	class Texture;
	class MaterialInterface;
	class Mesh;

	ENUM()
	enum class EParticlePlayState : uint8
	{
		Playing,
		Paused,
		Stopped
	};

	ENUM()
	enum class EParticleSimulationSpace : uint8
	{
		World,
		Local
	};

	ENUM()
	enum class EParticleCollisionMode : uint8
	{
		None,
		FloorPlane,
		PhysicsRaycast
	};

	ENUM()
	enum class EParticleCollisionResponse : uint8
	{
		Bounce,
		Slide,
		DieOnContact,
		Stop
	};

	// 개별 파티클 인스턴스 데이터 (경량 POD)
	struct Particle
	{
		vec3 position = vec3(0.0f);
		vec3 velocity = vec3(0.0f);
		vec4 color = vec4(1.0f);
		vec2 size = vec2(1.0f);
		f32 rotation = 0.0f;
		f32 angularVelocity = 0.0f;
		f32 age = 0.0f;
		f32 lifetime = 1.0f;
		uint8 bounceCount = 0;

		bool IsAlive() const { return age < lifetime; }
		f32 GetNormalizedAge() const { return lifetime > 0.0f ? glm::clamp(age / lifetime, 0.0f, 1.0f) : 1.0f; }
	};

	// 파티클 충돌 이벤트 데이터 (외부 서브 이미터 연동 및 에셋 모듈용)
	struct ParticleCollisionEvent
	{
		vec3 hitPosition = vec3(0.0f);
		vec3 hitNormal = vec3(0.0f, 1.0f, 0.0f);
		vec3 velocity = vec3(0.0f);
		f32 time = 0.0f;
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_PARTICLE_EMITTER"), CATEGORY("CATEGORY_RENDER"))
	struct ParticleEmitterComponent
	{
		REFLECT_BODY()

		// ── 렌더링 리소스 (Rendering) ──
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_TEXTURE_HANDLE"))
		ResourceHandle<Texture> textureHandle;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_MESH_HANDLE"))
		ResourceHandle<Mesh> meshHandle;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_BILLBOARD_MODE"))
		ESpriteBillboardMode billboardMode = ESpriteBillboardMode::CameraFacing;

		// ── 방출 및 시뮬레이션 제어 (Emission) ──
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_PLAY_STATE"))
		EParticlePlayState playState = EParticlePlayState::Playing;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_SIM_SPACE"))
		EParticleSimulationSpace simulationSpace = EParticleSimulationSpace::World;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_MAX_PARTICLES"))
		uint32 maxParticles = 1000;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_EMISSION_RATE"))
		f32 emissionRate = 30.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_BURST_COUNT"))
		uint32 burstCount = 0;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_BURST_INTERVAL"))
		f32 burstInterval = 1.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_WARMUP_TIME"))
		f32 warmupTime = 0.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_USE_FIXED_SEED"))
		bool useFixedSeed = false;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_EMISSION"), DISPLAY_NAME("PROP_SEED"))
		uint32 seed = 1337;

		// ── 수명 및 역학 (Lifetime & Dynamics) ──
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_LIFETIME_MIN"))
		f32 lifetimeMin = 1.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_LIFETIME_MAX"))
		f32 lifetimeMax = 2.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_SPEED_MIN"))
		f32 initialSpeedMin = 1.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_SPEED_MAX"))
		f32 initialSpeedMax = 3.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_EMIT_DIRECTION"))
		vec3 emissionDirection = vec3(0.0f, 1.0f, 0.0f);

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_SPREAD_ANGLE"))
		f32 spreadAngleDegrees = 30.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_GRAVITY_MODIFIER"))
		vec3 gravity = vec3(0.0f, -9.81f, 0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_BUOYANCY"))
		f32 buoyancy = 0.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_DRAG"))
		f32 drag = 0.1f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_WIND_FORCE"))
		vec3 wind = vec3(0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_ENABLE_CURL_NOISE"))
		bool enableCurlNoise = false;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_CURL_NOISE_STRENGTH"))
		f32 curlNoiseStrength = 1.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_CURL_NOISE_FREQUENCY"))
		f32 curlNoiseFrequency = 0.5f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_MOTION"), DISPLAY_NAME("PROP_CURL_NOISE_SCROLL_SPEED"))
		f32 curlNoiseScrollSpeed = 0.5f;

		// ── 물리 및 충돌 (Collision & Interaction) ──
		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_COLLISION_MODE"))
		EParticleCollisionMode collisionMode = EParticleCollisionMode::None;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_COLLISION_RESPONSE"))
		EParticleCollisionResponse collisionResponse = EParticleCollisionResponse::Bounce;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_RESTITUTION"))
		f32 restitution = 0.5f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_FRICTION"))
		f32 friction = 0.2f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_FLOOR_HEIGHT"))
		f32 floorHeight = 0.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_COLLISION_RADIUS"))
		f32 collisionRadius = 0.05f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_MIN_BOUNCE_SPEED"))
		f32 minBounceSpeed = 0.15f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PARTICLE_COLLISION"), DISPLAY_NAME("PROP_MAX_COLLISION_CHECKS"))
		uint32 maxCollisionChecksPerFrame = 100;

		// ── 크기 및 회전 (Size & Rotation) ──
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_SIZE"), DISPLAY_NAME("PROP_SIZE_START"))
		vec2 sizeStart = vec2(0.5f, 0.5f);

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_SIZE"), DISPLAY_NAME("PROP_SIZE_END"))
		vec2 sizeEnd = vec2(0.1f, 0.1f);

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_SIZE"), DISPLAY_NAME("PROP_ROTATION_SPEED_MIN"))
		f32 rotationSpeedMin = -1.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_PARTICLE_SIZE"), DISPLAY_NAME("PROP_ROTATION_SPEED_MAX"))
		f32 rotationSpeedMax = 1.0f;

		// ── 색상 및 알파 (Color & Alpha) ──
		PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_PARTICLE_COLOR"), DISPLAY_NAME("PROP_COLOR_START"))
		vec4 colorStart = vec4(1.0f, 0.8f, 0.2f, 1.0f);

		PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_PARTICLE_COLOR"), DISPLAY_NAME("PROP_COLOR_END"))
		vec4 colorEnd = vec4(1.0f, 0.1f, 0.0f, 0.0f);

		// ── 런타임 시뮬레이션 상태 ──
		std::vector<Particle> particles;
		std::vector<ParticleCollisionEvent> collisionEvents; // 프레임당 발생한 충돌 이벤트 버퍼 (외부 모듈/서브이미터 소비용)
		f32 emissionAccumulator = 0.0f;
		f32 burstTimer = 0.0f;
		std::mt19937 rng;
		bool rngInitialized = false;
		bool warmupDone = false;

		// ── 런타임 제어 API ──
		void Play() { playState = EParticlePlayState::Playing; }
		void Pause() { playState = EParticlePlayState::Paused; }
		void Stop() { playState = EParticlePlayState::Stopped; }
		void Reset() { particles.clear(); collisionEvents.clear(); emissionAccumulator = 0.0f; burstTimer = 0.0f; warmupDone = false; }
		void ClearEvents() { collisionEvents.clear(); }
	};
}
