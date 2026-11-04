#pragma once

#include "Resource.h"
#include "ResourceHandle.h"
#include <string>
#include <vector>

BEGIN(Engine)

class Model;
class Animation;

// ── 트랙 및 노티파이 타입 ──
ENUM()
enum class ETimelineTrackType : uint8
{
	Notify,        // 파티클, 사운드, 커스텀 이벤트 마커 트랙
	CameraShot,    // 카메라 줌인, 타깃 본 추적, 셰이크 트랙
	TimeDilation   // 역경직 / 슬로우 모션 트랙 (확장용)
};

ENUM()
enum class EAnimNotifyType : uint8
{
	Particle,
	Sound,
	CustomEvent
};

// 개별 노티파이 마커 데이터
STRUCT()
struct ENGINE_API AnimNotifyData
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string Name = "New_Notify";

	PROPERTY(EDITABLE)
		float TriggerTime = 0.0f;

	PROPERTY(EDITABLE)
		EAnimNotifyType Type = EAnimNotifyType::Particle;

	PROPERTY(EDITABLE)
		std::string SocketBoneName = "";

	PROPERTY(EDITABLE)
		vec3 OffsetPosition = vec3(0.0f);

	PROPERTY(EDITABLE)
		vec4 ParticleColor = vec4(1.0f, 0.65f, 0.15f, 1.0f);

	PROPERTY(EDITABLE)
		float ParticleSize = 0.12f;

	PROPERTY(EDITABLE)
		float ParticleSpeed = 1.6f;
	PROPERTY(EDITABLE)
		float ParticleLifetime = 0.6f;
	PROPERTY(EDITABLE)
		uint32 BurstCount = 25;
	PROPERTY(EDITABLE)
		std::string SoundPath = "";
	PROPERTY(EDITABLE)
		float SoundVolume = 1.0f;
	PROPERTY(EDITABLE)
		std::string EventTag = "OnAttackHit";
	bool operator==(const AnimNotifyData& other) const = default;
};

// 1. 노티파이 트랙 (파티클/사운드/이벤트 목록을 가지는 한 줄의 트랙)
STRUCT()
struct ENGINE_API TimelineNotifyTrack
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string TrackName = "FX Track";

	PROPERTY(EDITABLE)
		bool IsMuted = false;

	PROPERTY(EDITABLE)
		std::vector<AnimNotifyData> Notifies;

	bool operator==(const TimelineNotifyTrack& other) const = default;
};

// 2. 카메라 샷 연출 트랙 (스킬/액션 시전 시 FOV 줌인 및 셰이크 연출)
STRUCT()
struct ENGINE_API TimelineCameraTrack
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string TrackName = "Camera Shot";

	PROPERTY(EDITABLE)
		bool IsActive = true;

	PROPERTY(EDITABLE)
		float StartTime = 0.0f;

	PROPERTY(EDITABLE)
		float Duration = 1.0f;

	PROPERTY(EDITABLE)
		float TargetFOV = 35.0f;           // 줌인 목표 화각 (기본 45도 기준)

	PROPERTY(EDITABLE)
		std::string FocusBoneName = "";   // 카메라가 시선을 둘 본 (비어있으면 기본 타깃)

	PROPERTY(EDITABLE)
		float ShakeIntensity = 0.0f;      // 화면 흔들림 강도 (0.0 = 없음, 1.0 = 강한 셰이크)

	bool operator==(const TimelineCameraTrack& other) const = default;
};

struct ActionTimelineCreateDesc : public ResourceCreateDesc
{
	std::string ModelPath;
	std::string ClipName;
	float Duration = 1.0f;
	std::vector<TimelineNotifyTrack> NotifyTracks;
	std::vector<TimelineCameraTrack> CameraTracks;
};

// 최상위 복합 액션 타임라인 에셋
CLASS()
class ENGINE_API ActionTimelineAsset : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(ActionTimelineAsset)

private:
	using DESC = ActionTimelineCreateDesc;
	ActionTimelineAsset() : Resource(EResourceType::ActionTimeline) {}
	virtual ~ActionTimelineAsset() = default;

public:
	virtual EResult Initialize(void* arg = nullptr) override;
	virtual void Free() override;

	static ActionTimelineAsset* Create(void* arg = nullptr);

	// JsonArchive 기반 파일 저장 / 불러오기
	bool SaveToFile(const std::string& filePath) const;
	bool LoadFromFile(const std::string& filePath);

	// Getters & Setters
	const std::string& GetModelPath() const { return m_ModelPath; }
	void SetModelPath(const std::string& path) { m_ModelPath = path; }

	const std::string& GetClipName() const { return m_ClipName; }
	void SetClipName(const std::string& clipName) { m_ClipName = clipName; }

	float GetDuration() const { return m_Duration; }
	void SetDuration(float duration) { m_Duration = duration; }

	const std::vector<TimelineNotifyTrack>& GetNotifyTracks() const { return m_NotifyTracks; }
	std::vector<TimelineNotifyTrack>& GetNotifyTracks() { return m_NotifyTracks; }

	const std::vector<TimelineCameraTrack>& GetCameraTracks() const { return m_CameraTracks; }
	std::vector<TimelineCameraTrack>& GetCameraTracks() { return m_CameraTracks; }

private:
	PROPERTY(EDITABLE)
		std::string m_ModelPath;

	PROPERTY(EDITABLE)
		std::string m_ClipName;

	PROPERTY(EDITABLE)
		float m_Duration = 1.0f;

	PROPERTY(EDITABLE)
		std::vector<TimelineNotifyTrack> m_NotifyTracks;

	PROPERTY(EDITABLE)
		std::vector<TimelineCameraTrack> m_CameraTracks;
};

END