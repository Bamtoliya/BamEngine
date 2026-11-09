#pragma once

#include "Resource.h"
#include "ResourceHandle.h"
#include "ResourceBinaryHeader.h"
#include "SpriteAnimation.h"
#include "SpriteRendererComponent.h"
#include "ParticleEffect.h"
#include <string>
#include <vector>

BEGIN(Engine)

class Mesh;
class MaterialInterface;
class AudioClip;

// ── 트랙 유형 열거형 ──
ENUM()
enum class EVFXTrackType : uint8
{
	SpriteFlipbook = 0, // 2D 스프라이트 플립북 애니메이션
	ParticleEmitter,    // 파티클 이미터 시스템 (.bameffect)
	LightFlash,         // 동적 포인트 조명 섬광
	MeshEffect,         // 마법진, 충격파 링 등 3D 메쉬
	Sound               // 동기화 효과음
};

// 1. 2D 스프라이트 플립북 트랙
STRUCT()
struct ENGINE_API VFXSpriteTrack
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRACK_NAME"))
	std::string trackName = "Sprite Flipbook";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
	bool enabled = true;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_MUTED"))
	bool isMuted = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_START_TIME"))
	f32 startTime = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DURATION"))
	f32 duration = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_RATE"))
	f32 playbackSpeed = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
	bool loop = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANIMATION_HANDLE"))
	ResourceHandle<SpriteAnimation> animationHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
	ResourceHandle<MaterialInterface> materialHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BILLBOARD_MODE"))
	ESpriteBillboardMode billboardMode = ESpriteBillboardMode::CameraFacing;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_POSITION"))
	vec3 localPosition = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_ROTATION"))
	vec3 localRotation = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_SCALE"))
	vec3 localScale = vec3(1.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLOR_TINT"))
	vec4 colorTint = vec4(1.0f, 1.0f, 1.0f, 1.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SORTING_ORDER"))
	int32 sortingOrder = 0;
};

// 2. 파티클 이미터 트랙
STRUCT()
struct ENGINE_API VFXParticleTrack
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRACK_NAME"))
	std::string trackName = "Particles";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
	bool enabled = true;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_MUTED"))
	bool isMuted = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_START_TIME"))
	f32 startTime = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DURATION"))
	f32 duration = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PLAY_RATE"))
	f32 playbackSpeed = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
	bool loop = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_EFFECT_HANDLE"))
	ResourceHandle<ParticleEffect> effectHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_POSITION"))
	vec3 localPosition = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_ROTATION"))
	vec3 localRotation = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_SCALE"))
	vec3 localScale = vec3(1.0f);
};

// 3. 조명 섬광 트랙 (Light Flash)
STRUCT()
struct ENGINE_API VFXLightTrack
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRACK_NAME"))
	std::string trackName = "Light Flash";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
	bool enabled = true;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_MUTED"))
	bool isMuted = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_START_TIME"))
	f32 startTime = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DURATION"))
	f32 duration = 0.5f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_POSITION"))
	vec3 localPosition = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHT_COLOR"))
	vec3 lightColor = vec3(1.0f, 0.6f, 0.2f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PEAK_INTENSITY"))
	f32 peakIntensity = 5.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHT_RADIUS"))
	f32 radius = 5.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ATTACK_TIME"))
	f32 attackTime = 0.05f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DECAY_TIME"))
	f32 decayTime = 0.45f;
};

// 4. 메쉬 이펙트 트랙 (충격파 링, 회전 마법진 등)
STRUCT()
struct ENGINE_API VFXMeshTrack
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRACK_NAME"))
	std::string trackName = "Mesh FX";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
	bool enabled = true;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_MUTED"))
	bool isMuted = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_START_TIME"))
	f32 startTime = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DURATION"))
	f32 duration = 0.8f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
	bool loop = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
	ResourceHandle<Mesh> meshHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
	ResourceHandle<MaterialInterface> materialHandle;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_POSITION"))
	vec3 localPosition = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INITIAL_ROTATION"))
	vec3 initialRotation = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ROTATION_RATE"))
	vec3 rotationRate = vec3(0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SCALE_START"))
	vec3 scaleStart = vec3(0.1f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SCALE_END"))
	vec3 scaleEnd = vec3(2.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLOR_START"))
	vec4 colorStart = vec4(1.0f, 1.0f, 1.0f, 1.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLOR_END"))
	vec4 colorEnd = vec4(1.0f, 1.0f, 1.0f, 0.0f);

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UV_SCROLL_SPEED"))
	vec2 uvScrollSpeed = vec2(0.0f);
};

// 5. 사운드 트랙
STRUCT()
struct ENGINE_API VFXSoundTrack
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TRACK_NAME"))
	std::string trackName = "SFX";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
	bool enabled = true;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_MUTED"))
	bool isMuted = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_START_TIME"))
	f32 startTime = 0.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_AUDIO_HANDLE"))
	ResourceHandle<AudioClip> audioClip;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_VOLUME"))
	f32 volume = 1.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PITCH"))
	f32 pitch = 1.0f;
};

// ── 에셋 생성 디스크립터 ──
struct CompositeEffectCreateDesc : public ResourceCreateDesc
{
	f32 totalDuration = 2.0f;
	bool isLooping = false;
};

// ── 복합 비주얼 이펙트 에셋 클래스 ──
CLASS()
class ENGINE_API CompositeEffect final : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(CompositeEffect)
	using DESC = CompositeEffectCreateDesc;

private:
	CompositeEffect() : Resource(EResourceType::CompositeEffect) {}
	virtual ~CompositeEffect() = default;
	EResult Initialize(void* arg = nullptr) override;

public:
	static CompositeEffect* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	// ── 전체 연출 속성 ──
	f32 GetTotalDuration() const { return m_TotalDuration; }
	void SetTotalDuration(f32 duration) { m_TotalDuration = (duration > 0.05f) ? duration : 0.05f; }

	bool IsLooping() const { return m_IsLooping; }
	void SetLooping(bool loop) { m_IsLooping = loop; }

	// ── 트랙 접근자 ──
	std::vector<VFXSpriteTrack>& GetSpriteTracks() { return m_SpriteTracks; }
	const std::vector<VFXSpriteTrack>& GetSpriteTracks() const { return m_SpriteTracks; }

	std::vector<VFXParticleTrack>& GetParticleTracks() { return m_ParticleTracks; }
	const std::vector<VFXParticleTrack>& GetParticleTracks() const { return m_ParticleTracks; }

	std::vector<VFXLightTrack>& GetLightTracks() { return m_LightTracks; }
	const std::vector<VFXLightTrack>& GetLightTracks() const { return m_LightTracks; }

	std::vector<VFXMeshTrack>& GetMeshTracks() { return m_MeshTracks; }
	const std::vector<VFXMeshTrack>& GetMeshTracks() const { return m_MeshTracks; }

	std::vector<VFXSoundTrack>& GetSoundTracks() { return m_SoundTracks; }
	const std::vector<VFXSoundTrack>& GetSoundTracks() const { return m_SoundTracks; }

	// ── 유틸리티 메서드 ──
	uint32 GetTotalTrackCount() const;
	void RecalculateDurationFromTracks();
	void ClearAllTracks();

	// ── 간편 파일 I/O ──
	bool SaveToFile(const std::wstring& filePath) const;
	bool LoadFromFile(const std::wstring& filePath);

private:
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TOTAL_DURATION"))
	f32 m_TotalDuration = 2.0f;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
	bool m_IsLooping = false;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPRITE_TRACKS"))
	std::vector<VFXSpriteTrack> m_SpriteTracks;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PARTICLE_TRACKS"))
	std::vector<VFXParticleTrack> m_ParticleTracks;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHT_TRACKS"))
	std::vector<VFXLightTrack> m_LightTracks;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_TRACKS"))
	std::vector<VFXMeshTrack> m_MeshTracks;

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SOUND_TRACKS"))
	std::vector<VFXSoundTrack> m_SoundTracks;
};

END
