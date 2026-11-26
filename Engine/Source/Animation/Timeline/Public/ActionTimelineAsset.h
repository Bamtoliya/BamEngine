#pragma once

#include "Engine_API.h"
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
	TimeDilation,  // 역경직 / 슬로우 모션 트랙 (확장용)
	CameraCut,     // 시네마틱 카메라 컷/블렌드 트랙
	CameraSpline   // 스플라인 카메라 경로 및 Look-At 추적 트랙
};

ENUM()
enum class EAnimNotifyType : uint8
{
	Particle,
	Sound,
	CustomEvent,
	SpawnCompositeVFX // [신설] 복합 VFX 스폰 노티파이
};

// 노티파이 트랙 종류: 트랙 아이콘과 새 노티파이의 기본 유형을 정한다.
// Unspecified는 종류 필드가 없던 이전 파일이며, 로드할 때 트랙 이름으로 한 번 판정한다. 저장값은 정수이므로 순서를 바꾸지 않는다.
ENUM()
enum class ETimelineNotifyTrackKind : uint8
{
	Unspecified,
	Effect,
	Audio,
	Event
};

// 재생이 끝나는 시점을 정하는 규칙. 저장값은 정수이므로 순서를 바꾸지 않는다.
ENUM()
enum class ETimelineEndPolicy : uint8
{
	Fixed,        // 저장한 길이(Duration)에서 끝난다. 값이 없던 이전 파일의 기본값이다.
	ClipLength,   // 작성 클립 길이에서 끝난다. 클립을 찾지 못하면 저장한 길이를 쓴다.
	LastTrackEnd  // 작성 클립과 모든 트랙 항목 중 가장 늦게 끝나는 시점에서 끝난다.
};

// 개별 노티파이 마커 데이터
STRUCT()
struct ENGINE_API AnimNotifyData
{
	REFLECT_BODY()
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NAME"))
		std::string Name = "New_Notify";
	// 상한(클립 길이)은 편집기에서 제한한다.
	PROPERTY(EDITABLE, RANGE(0.0, 600.0, 0.01), DISPLAY_NAME("PROP_NOTIFY_TRIGGER_TIME"))
		float TriggerTime = 0.0f;
	// 0이면 순간 노티파이(1회 재생). 0보다 크면 지속형: TriggerTime부터 이 시간 동안 유지한 뒤 해제한다.
	PROPERTY(EDITABLE, ALLOW_MISSING, EDITCONDITION(Type == SpawnCompositeVFX), RANGE(0.0, 600.0, 0.01), DISPLAY_NAME("PROP_NOTIFY_DURATION"))
		float Duration = 0.0f;
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NOTIFY_TYPE"))
		EAnimNotifyType Type = EAnimNotifyType::Particle;
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_SOCKET"), DISPLAY_NAME("PROP_SOCKET_BONE"))
		std::string SocketBoneName = "";
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_SOCKET"), RANGE(-10.0, 10.0, 0.02), DISPLAY_NAME("PROP_SOCKET_OFFSET"))
		vec3 OffsetPosition = vec3(0.0f);
	// 소켓 기준 오프셋 회전 (검기/충격파의 발사 각도 및 칼날 정렬용)
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_SOCKET"), EDITCONDITION(Type == SpawnCompositeVFX), RANGE(-180.0, 180.0, 1.0), DISPLAY_NAME("PROP_SOCKET_ROTATION"))
		vec3 OffsetRotation = vec3(0.0f);
	PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_NOTIFY_PARTICLE"), EDITCONDITION(Type == Particle), DISPLAY_NAME("PROP_COLOR"))
		vec4 ParticleColor = vec4(1.0f, 0.65f, 0.15f, 1.0f);
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_PARTICLE"), EDITCONDITION(Type == Particle), RANGE(0.02, 0.5, 0.01), DISPLAY_NAME("PROP_SIZE"))
		float ParticleSize = 0.12f;
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_PARTICLE"), EDITCONDITION(Type == Particle), RANGE(0.2, 5.0, 0.05), DISPLAY_NAME("PROP_PARTICLE_SPEED"))
		float ParticleSpeed = 1.6f;
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_PARTICLE"), EDITCONDITION(Type == Particle), RANGE(0.05, 5.0, 0.05), DISPLAY_NAME("PROP_LIFETIME"))
		float ParticleLifetime = 0.6f;
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_PARTICLE"), EDITCONDITION(Type == Particle), RANGE(5, 80), DISPLAY_NAME("PROP_BURST_COUNT"))
		uint32 BurstCount = 25;
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_SOUND"), EDITCONDITION(Type == Sound), DISPLAY_NAME("PROP_SOUND_PATH"))
		std::string SoundPath = "";
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_SOUND"), EDITCONDITION(Type == Sound), RANGE(0.0, 2.0, 0.01), DISPLAY_NAME("PROP_VOLUME"))
		float SoundVolume = 1.0f;
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_EVENT"), EDITCONDITION(Type == CustomEvent), DISPLAY_NAME("PROP_EVENT_TAG"))
		std::string EventTag = "OnAttackHit";
	// ── 복합 VFX (Composite VFX) 스폰 속성 ──
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_VFX"), EDITCONDITION(Type == SpawnCompositeVFX), DISPLAY_NAME("PROP_VFX_PATH"))
		std::string VFXPath = ""; // 스폰할 .bamvfx 에셋 경로
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_VFX"), EDITCONDITION(Type == SpawnCompositeVFX), RANGE(0.01, 10.0, 0.05), DISPLAY_NAME("PROP_SCALE"))
		vec3 VFXScale = vec3(1.0f); // 스폰 크기 배율
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_VFX"), EDITCONDITION(Type == SpawnCompositeVFX), DISPLAY_NAME("PROP_ATTACH_TO_SOCKET"))
		bool AttachToSocket = false; // 소켓에 지속 부착 여부 (true: 칼을 따라다님, false: 스폰 위치에 잔류)
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_VFX"), EDITCONDITION(Type == SpawnCompositeVFX), RANGE(0.1, 5.0, 0.05), DISPLAY_NAME("PROP_PLAY_RATE"))
		float VFXPlayRate = 1.0f; // 이펙트 재생 배속
	PROPERTY(EDITABLE, CATEGORY("CATEGORY_NOTIFY_VFX"), EDITCONDITION(Type == SpawnCompositeVFX), DISPLAY_NAME("PROP_AUTO_DESTROY"))
		bool AutoDestroy = true; // 재생 완료 시 엔티티 자동 소멸
	bool operator==(const AnimNotifyData& other) const = default;
};

// 1. 노티파이 트랙 (파티클/사운드/이벤트 목록을 가지는 한 줄의 트랙)
STRUCT()
struct ENGINE_API TimelineNotifyTrack
{
	REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string TrackName = "FX Track";

	PROPERTY(EDITABLE, ALLOW_MISSING)
		ETimelineNotifyTrackKind Kind = ETimelineNotifyTrackKind::Unspecified;

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

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NAME"))
		std::string TrackName = "Camera Shot";

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
		bool IsActive = true;

	// 상한(클립 길이)은 편집기에서 제한한다.
	PROPERTY(EDITABLE, RANGE(0.0, 600.0, 0.01), DISPLAY_NAME("PROP_START_TIME"))
		float StartTime = 0.0f;

	PROPERTY(EDITABLE, RANGE(0.05, 5.0, 0.01), DISPLAY_NAME("PROP_DURATION"))
		float Duration = 1.0f;

	PROPERTY(EDITABLE, RANGE(15.0, 90.0, 0.5), DISPLAY_NAME("PROP_TARGET_FOV"))
		float TargetFOV = 35.0f;           // 줌인 목표 화각 (기본 45도 기준)

	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FOCUS_BONE"))
		std::string FocusBoneName = "";   // 카메라가 시선을 둘 본 (비어있으면 기본 타깃)

	PROPERTY(EDITABLE, RANGE(0.0, 1.5, 0.01), DISPLAY_NAME("PROP_SHAKE_INTENSITY"))
		float ShakeIntensity = 0.0f;      // 화면 흔들림 강도 (0.0 = 없음, 1.0 = 강한 셰이크)

	bool operator==(const TimelineCameraTrack& other) const = default;
};

// 3. 시네마틱 카메라 컷/블렌드 구간 (Camera Cut Track)
STRUCT()
struct ENGINE_API TimelineCameraCutSection
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_NAME"))
	std::string TrackName = "Camera Cut";

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_ENABLED"))
	bool IsActive = true;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 600.0, 0.01), DISPLAY_NAME("PROP_START_TIME"))
	float StartTime = 0.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 600.0, 0.01))
	float EndTime = 1.0f;

	// 전환할 씬 카메라 엔티티 이름 (NameComponent 매칭). 비어있으면 현재 활성 메인 카메라에 오프셋을 적용한다.
	PROPERTY(EDITABLE, ALLOW_MISSING)
	std::string TargetCameraName = "";

	// 대상 카메라 기준 로컬 위치 오프셋
	PROPERTY(EDITABLE, ALLOW_MISSING)
	vec3 LocalPositionOffset = vec3(0.0f);

	// 대상 카메라 기준 로컬 오일러 회전 오프셋 (도)
	PROPERTY(EDITABLE, ALLOW_MISSING)
	vec3 LocalEulerRotationOffset = vec3(0.0f);

	// 0보다 크면 해당 FOV(도)로 블렌딩한다. 0 이하이면 대상 카메라의 원래 FOV를 유지한다.
	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 179.0, 0.5), DISPLAY_NAME("PROP_TARGET_FOV"))
	float TargetFOV = 0.0f;

	// 진입 블렌드 인 시간(초). 0이면 즉시 컷 전환한다.
	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 60.0, 0.01))
	float BlendInDuration = 0.0f;

	bool operator==(const TimelineCameraCutSection& other) const = default;
};

// 4. 스플라인 카메라 경로 및 Look-At 추적 구간 (Spline Camera Path Track)
STRUCT()
struct ENGINE_API TimelineCameraSplineSection
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_NAME"))
	std::string TrackName = "Camera Spline";

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_ENABLED"))
	bool IsActive = true;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 600.0, 0.01), DISPLAY_NAME("PROP_START_TIME"))
	float StartTime = 0.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 600.0, 0.01))
	float EndTime = 1.0f;

	// 경로로 사용할 SplineComponent 소유 엔티티 이름 (NameComponent 매칭)
	PROPERTY(EDITABLE, ALLOW_MISSING)
	std::string SplineEntityName = "";

	// 스플라인 시작/종료 정규화 진행도 (0.0 ~ 1.0)
	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 1.0, 0.01))
	float StartT = 0.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 1.0, 0.01))
	float EndT = 1.0f;

	// 선택적 주시 대상 엔티티 이름 (NameComponent 매칭). 비어있고 FollowTangent가 true면 스플라인 접선 방향을 바라본다.
	PROPERTY(EDITABLE, ALLOW_MISSING)
	std::string LookAtTargetName = "";

	// LookAtTargetName이 비어있을 때 스플라인 접선 방향 바라보기 여부
	PROPERTY(EDITABLE, ALLOW_MISSING)
	bool FollowTangent = true;

	// 0보다 크면 해당 FOV(도)로 블렌딩한다.
	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 179.0, 0.5), DISPLAY_NAME("PROP_TARGET_FOV"))
	float TargetFOV = 0.0f;

	// 진입 블렌드 인 시간(초). 0이면 즉시 경로 포즈를 적용한다.
	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 60.0, 0.01))
	float BlendInDuration = 0.0f;

	bool operator==(const TimelineCameraSplineSection& other) const = default;
};

struct ActionTimelineCreateDesc : public ResourceCreateDesc
{
	std::string ModelPath;
	std::string ClipName;
	float Duration = 1.0f;
	ETimelineEndPolicy EndPolicy = ETimelineEndPolicy::Fixed;
	std::vector<TimelineNotifyTrack> NotifyTracks;
	std::vector<TimelineCameraTrack> CameraTracks;
	std::vector<TimelineCameraCutSection> CameraCutTracks;
	std::vector<TimelineCameraSplineSection> CameraSplineTracks;
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
#if BAM_EDITOR
	bool PrepareFilePathRelocation(const std::vector<FilePathChange>& changes, std::vector<Utf8FilePathUpdate>& updates) override;
	static void PrepareNotifyPathRelocation(std::vector<TimelineNotifyTrack>& tracks,
		const std::vector<FilePathChange>& changes, std::vector<Utf8FilePathUpdate>& updates);
	static bool IsProjectJsonDocument(const std::filesystem::path& pathOrUri);
#endif

	// 종류가 정해진 트랙은 그대로 돌려주고, 이전 트랙은 이전 이름 규칙(Audio·Sound → 오디오, Event → 이벤트, 그 외 → 이펙트)으로 판정한다.
	static ETimelineNotifyTrackKind ResolveTrackKind(const TimelineNotifyTrack& track);

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
	// 타임라인이 끝난 뒤 원래 애니메이션으로 섞어 넘어가는 시간(초). 0이면 바로 바뀐다. 값이 없던 이전 파일은 기본값을 쓴다.
	static constexpr float DefaultBlendOutTime = 0.2f;
	float GetBlendOutTime() const { return m_BlendOutTime; }
	void SetBlendOutTime(float time) { m_BlendOutTime = std::max(0.0f, time); }

	ETimelineEndPolicy GetEndPolicy() const { return m_EndPolicy; }
	void SetEndPolicy(ETimelineEndPolicy policy) { m_EndPolicy = policy; }

	// 재생 길이의 하한(초). 길이 0으로 바로 끝나 노티파이를 놓치지 않게 한다.
	static constexpr float MinPlaybackDuration = 0.05f;
	// 모든 트랙 항목 중 가장 늦게 끝나는 시점(초). 음소거·비활성 항목도 작성된 내용이므로 포함한다. 항목이 없으면 0이다.
	static float ComputeLastTrackEnd(const std::vector<TimelineNotifyTrack>& notifyTracks, const std::vector<TimelineCameraTrack>& cameraTracks,
		const std::vector<TimelineCameraCutSection>& cutTracks, const std::vector<TimelineCameraSplineSection>& splineTracks);
	// 종료 정책으로 재생 길이를 정한다. clipLength가 0 이하이면 작성 클립을 찾지 못한 것으로 본다. 결과는 MinPlaybackDuration 이상이다.
	static float ResolvePlaybackDuration(ETimelineEndPolicy policy, float fixedDuration, float clipLength, float lastTrackEnd);
	float ResolvePlaybackDuration(float clipLength) const;

	const std::vector<TimelineNotifyTrack>& GetNotifyTracks() const { return m_NotifyTracks; }
	std::vector<TimelineNotifyTrack>& GetNotifyTracks() { return m_NotifyTracks; }

	const std::vector<TimelineCameraTrack>& GetCameraTracks() const { return m_CameraTracks; }
	std::vector<TimelineCameraTrack>& GetCameraTracks() { return m_CameraTracks; }

	const std::vector<TimelineCameraCutSection>& GetCameraCutTracks() const { return m_CameraCutTracks; }
	std::vector<TimelineCameraCutSection>& GetCameraCutTracks() { return m_CameraCutTracks; }

	const std::vector<TimelineCameraSplineSection>& GetCameraSplineTracks() const { return m_CameraSplineTracks; }
	std::vector<TimelineCameraSplineSection>& GetCameraSplineTracks() { return m_CameraSplineTracks; }

protected:
	// 공용 리소스 로드·핫 리로드·LoadFromFile이 모두 ReadArchive를 거치므로, 이전 파일의 트랙 종류 판정을 여기서 한 번 수행한다.
	EResult OnPostLoad() override;

private:
	PROPERTY(EDITABLE)
		std::string m_ModelPath;

	PROPERTY(EDITABLE)
		std::string m_ClipName;

	PROPERTY(EDITABLE)
		float m_Duration = 1.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		float m_BlendOutTime = DefaultBlendOutTime;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		ETimelineEndPolicy m_EndPolicy = ETimelineEndPolicy::Fixed;

	PROPERTY(EDITABLE)
		std::vector<TimelineNotifyTrack> m_NotifyTracks;

	PROPERTY(EDITABLE)
		std::vector<TimelineCameraTrack> m_CameraTracks;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		std::vector<TimelineCameraCutSection> m_CameraCutTracks;

	PROPERTY(EDITABLE, ALLOW_MISSING)
		std::vector<TimelineCameraSplineSection> m_CameraSplineTracks;
};

END