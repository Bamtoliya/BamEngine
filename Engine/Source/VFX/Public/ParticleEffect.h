#pragma once

#include "Resource.h"
#include "ResourceHandle.h"
#include "ParticleEmitterComponent.h"
#include <string>
#include <vector>

BEGIN(Engine)

class Texture;
class MaterialInterface;
class Mesh;

// 이미터 발화 조건 (트리거)
ENUM()
enum class EParticleEmitterTrigger : uint8
{
	Continuous = 0,     // 시간 경과에 따른 지속 방출 (emissionRate 또는 burstInterval)
	OnParentCollision,  // 부모 이미터의 입자가 충돌했을 때 (hitPosition, hitNormal에서 스폰)
	OnParentDeath       // 부모 이미터의 입자가 수명 만료로 사망했을 때 (deathPosition에서 스폰)
};

// 개별 이미터 모듈 데이터 정의
STRUCT()
struct ENGINE_API ParticleEmitterData
{
	REFLECT_BODY()

	// ── 식별 및 트리거 체이닝 ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NAME"))
	std::string name = "Emitter";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
	bool enabled = true;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRIGGER_MODE"))
	EParticleEmitterTrigger triggerMode = EParticleEmitterTrigger::Continuous;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PARENT_EMITTER_INDEX"))
	int32 parentEmitterIndex = -1; // -1: 없음 (독립 이미터)

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRIGGER_BURST_COUNT"))
	uint32 triggerBurstCount = 5;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INHERIT_PARENT_VELOCITY"))
	bool inheritParentVelocity = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INHERIT_VELOCITY_RATIO"))
	f32 inheritVelocityRatio = 0.3f;

	// ── 방출 및 수량 ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MAX_PARTICLES"))
	uint32 maxParticles = 500;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_EMISSION_RATE"))
	f32 emissionRate = 20.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BURST_COUNT"))
	uint32 burstCount = 0;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BURST_INTERVAL"))
	f32 burstInterval = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SIM_SPACE"))
	EParticleSimulationSpace simulationSpace = EParticleSimulationSpace::World;

	// ── 수명 및 초기 운동 ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIFETIME_MIN"))
	f32 lifetimeMin = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIFETIME_MAX"))
	f32 lifetimeMax = 1.5f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPEED_MIN"))
	f32 speedMin = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPEED_MAX"))
	f32 speedMax = 3.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_EMIT_DIRECTION"))
	vec3 direction = vec3(0.0f, 1.0f, 0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPREAD_ANGLE"))
	f32 spreadAngleDegrees = 30.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ALIGN_TO_SURFACE_NORMAL"))
	bool alignToSurfaceNormal = true; // 이벤트 스폰 시 법선 방향을 기준축으로 사용

	// ── 환경 역학 (중력, 부력, 저항, 바람, 컬 노이즈) ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_GRAVITY_MODIFIER"))
	vec3 gravity = vec3(0.0f, -9.81f, 0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BUOYANCY"))
	f32 buoyancy = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DRAG"))
	f32 drag = 0.1f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_WIND_FORCE"))
	vec3 wind = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLE_CURL_NOISE"))
	bool enableCurlNoise = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CURL_NOISE_STRENGTH"))
	f32 curlNoiseStrength = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CURL_NOISE_FREQUENCY"))
	f32 curlNoiseFrequency = 0.5f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CURL_NOISE_SCROLL_SPEED"))
	f32 curlNoiseScrollSpeed = 0.5f;

	// ── 물리 및 충돌 ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_MODE"))
	EParticleCollisionMode collisionMode = EParticleCollisionMode::None;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_RESPONSE"))
	EParticleCollisionResponse collisionResponse = EParticleCollisionResponse::Bounce;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RESTITUTION"))
	f32 restitution = 0.5f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FRICTION"))
	f32 friction = 0.2f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLOOR_HEIGHT"))
	f32 floorHeight = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_RADIUS"))
	f32 collisionRadius = 0.05f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MIN_BOUNCE_SPEED"))
	f32 minBounceSpeed = 0.15f;

	// ── 크기 및 회전 ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SIZE_START"))
	vec2 sizeStart = vec2(0.5f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SIZE_END"))
	vec2 sizeEnd = vec2(0.1f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ROTATION_SPEED_MIN"))
	f32 rotationSpeedMin = -1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ROTATION_SPEED_MAX"))
	f32 rotationSpeedMax = 1.0f;

	// ── 색상 ──
	PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR_START"))
	vec4 colorStart = vec4(1.0f, 0.8f, 0.2f, 1.0f);

	PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR_END"))
	vec4 colorEnd = vec4(1.0f, 0.1f, 0.0f, 0.0f);

	// ── 렌더링 리소스 ──
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TEXTURE_HANDLE"))
	ResourceHandle<Texture> textureHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
	ResourceHandle<MaterialInterface> materialHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
	ResourceHandle<Mesh> meshHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BILLBOARD_MODE"))
	ESpriteBillboardMode billboardMode = ESpriteBillboardMode::CameraFacing;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ALIGN_TO_SURFACE_DISK"))
	bool alignToSurfaceDisk = false; // 충격파 링처럼 표면 법선에 밀착된 디스크로 렌더링
};

struct ParticleEffectCreateDesc : public ResourceCreateDesc
{
	std::vector<ParticleEmitterData> emitters;
};

CLASS()
class ENGINE_API ParticleEffect final : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(ParticleEffect)
	using DESC = ParticleEffectCreateDesc;

private:
	ParticleEffect() : Resource(EResourceType::ParticleEffect) {}
	virtual ~ParticleEffect() = default;
	EResult Initialize(void* arg = nullptr) override;

public:
	static ParticleEffect* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	const std::vector<ParticleEmitterData>& GetEmitters() const { return m_Emitters; }
	std::vector<ParticleEmitterData>& GetEmitters() { return m_Emitters; }
	int32 GetEmitterCount() const { return static_cast<int32>(m_Emitters.size()); }
	const ParticleEmitterData* GetEmitter(int32 index) const;
	ParticleEmitterData* GetEmitter(int32 index);

	void AddEmitter(const ParticleEmitterData& emitterData);
	void InsertEmitter(int32 index, const ParticleEmitterData& emitterData);
	void RemoveEmitter(int32 index);
	void ClearEmitters();

	bool SaveToFile(const std::wstring& filePath) const;
	bool LoadFromFile(const std::wstring& filePath);

private:
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_EMITTERS"))
	std::vector<ParticleEmitterData> m_Emitters;
};

END
