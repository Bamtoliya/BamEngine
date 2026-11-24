#pragma once

#include "Base.h"
#include "RHIAccelerationStructure.h"
#include "RaytracingBuildPolicy.h"
#include "RaytracingInstanceData.h"
#include "RaytracingSkinnedPolicy.h"
#include "RenderTargetTypes.h"

struct RaytracingSceneManagerDesc
{
	Engine::RHI* RHI = { nullptr };
};

BEGIN(Engine)
class Scene;
class Mesh;
class MaterialInterface;
class RHIBuffer;
class RHIPipeline;
struct MaterialConstants;

// 렌더되는 씬과 뷰(Geometry 패스 ID)의 조합마다 레이트레이싱 그림자용 TLAS를 매 프레임 다시 빌드하고, 메시별 BLAS를 캐시한다.
// - 뷰포트 제출 단계에서 RequestScene으로 씬을 등록하고, Renderer::Render 시작에 BuildRequestedScenes가 빌드한다.
// - 지원 대상: 활성·보이는 castShadow 정적 메시 중 IsShadowCasterSupported를 만족하는 것(LOD는 그 뷰가 고른 LOD)과
//   스킨드 메시(프레임당 MaxSkinnedInstances개까지). 스킨드 메시는 Compute 스키닝 결과로 엔티티별 BLAS를 매 프레임 갱신(refit)한다.
// - 미지원 캐스터(동적·스플라인·불투명이 아닌 재질·그림자 전용 정점 셰이더, 한도를 넘은 스킨드 메시)는 기존 섀도맵이 담당한다.
// - 새 BLAS는 프레임 예산 안에서만 빌드한다. 대기 중이거나 실패한 캐스터와 TLAS 빌드에 실패한 씬도 섀도맵이 담당한다.
class ENGINE_API RaytracingSceneManager : public Base
{
	DECLARE_SINGLETON(RaytracingSceneManager)
	using DESC = RaytracingSceneManagerDesc;
#pragma region Constructor&Destructor
private:
	RaytracingSceneManager() {}
	virtual ~RaytracingSceneManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Frame
public:
	// 현재 장치가 인라인 RayQuery(DXR Tier 1.1 + SM 6.5)를 지원하는지
	bool IsSupported() const;
	// 이번 프레임에 TLAS가 필요한 씬과 뷰를 등록한다. 같은 조합은 한 번만 빌드한다.
	// reflections면 반사에 필요한 메시와 인스턴스 표도 함께 준비한다.
	// cameraPosition이 있으면 스킨드 메시 한도를 초과할 때 카메라에 가까운 순서대로 우선 선택한다.
	void RequestScene(Scene* scene, uint32 viewID, bool reflections = false, const vec3* cameraPosition = nullptr);
	// 이번 프레임에 TLAS가 요청된 씬인지. 제출 단계에서 섀도맵 제외 후보를 표시할 때 확인한다.
	bool IsSceneRequested(const Scene* scene, uint32 viewID) const;
	// 빌드 전에 프레임이 중단되면 해제된 씬 포인터가 다음 프레임에 남지 않도록 프레임 큐와 함께 비운다.
	void ClearRequests() { m_RequestedScenes.clear(); }
	bool HasRequests() const { return !m_RequestedScenes.empty(); }
	// 요청된 씬의 BLAS를 준비하고 TLAS를 다시 빌드한다. 요청되지 않은 씬의 TLAS는 해제하고 요청 목록을 비운다.
	EResult BuildRequestedScenes();
	// 이번 프레임에 빌드된 씬의 TLAS. 요청되지 않았거나 빌드에 실패했으면 nullptr.
	RHIAccelerationStructure* GetTopLevelAS(const Scene* scene, uint32 viewID) const;
	// 이번 프레임의 TLAS에 실제로 들어간 엔티티. TLAS가 없으면 nullptr이며, 그 씬의 캐스터는 모두 섀도맵에 그려야 한다.
	const RaytracedCasterSet* GetTopLevelCasters(const Scene* scene, uint32 viewID) const;
	// 이번 프레임의 반사용 인스턴스 표(RaytracingInstanceData)의 바인드리스 번호. 준비되지 않았으면 RHI::INVALID_BINDLESS_INDEX.
	uint32 GetInstanceTableIndex(const Scene* scene, uint32 viewID) const;
#pragma endregion

#pragma region Caster Policy
public:
	// 레이트레이싱 그림자가 형상을 그대로 표현할 수 있는 캐스터인지. 섀도맵 제외 여부는 이 판정이 아니라 GetTopLevelCasters로 정한다.
	// allowMasked가 true면 Masked 재질도 허용한다(바인드리스 인스턴스 표가 필요하다).
	static bool IsShadowCasterSupported(const Mesh* mesh, const MaterialInterface* material, bool allowMasked = false);
	// 반사에 나타낼 수 있는 메시인지(스킨드 포함). Masked는 광선마다 알파를 확인한다.
	static bool IsReflectionSupported(const Mesh* mesh, const MaterialInterface* material);
	// Compute 스키닝으로 레이트레이싱할 수 있는 스킨드 메시인지(정점 형식과 스트림 개수).
	static bool IsSkinningSupported(const Mesh* mesh);
	// 한 프레임에 레이트레이싱에 넣는 스킨드 메시 수(측정 전 시작값)
	static constexpr uint32 MaxSkinnedInstances = 32;
	// 뼈 행렬은 그리기 경로(Renderer::RenderSkinnedMeshes)와 같이 메시마다 256개를 쓴다.
	static constexpr uint32 SkinnedBoneCount = 256;
	// refit을 이만큼 이어 가면 빌드 예산이 있을 때 BLAS를 새로 만든다(측정 전 시작값)
	static constexpr uint32 SkinnedRebuildInterval = 300;
#pragma endregion

#pragma region Statistics
public:
	struct Stats
	{
		// 아래 네 값과 TLAS 크기는 유지 중인 TLAS(씬·뷰 조합)의 마지막 빌드를 합한 값이다.
		uint32 TopLevelCount = 0;
		uint32 InstanceCount = 0;
		uint32 UnsupportedCasterCount = 0;   // 섀도맵으로 넘긴 캐스터 수(스킨드 포함)
		uint32 PendingCasterCount = 0;       // BLAS 빌드 예산을 기다리며 섀도맵을 쓰는 캐스터 수
		uint32 FailedCasterCount = 0;        // BLAS 생성 실패로 섀도맵을 쓰는 캐스터 수
		uint32 BottomLevelCount = 0;         // 스킨드 메시의 엔티티별 BLAS 포함
		uint32 BottomLevelBuiltThisFrame = 0;
		uint64 BottomLevelBytes = 0;
		uint64 TopLevelBytes = 0;
		// 이번 프레임 값. 여러 뷰가 같은 스킨드 메시를 써도 한 번만 센다.
		uint32 SkinnedCount = 0;             // 스키닝하고 BLAS를 맞춘 스킨드 메시 수
		uint32 SkinnedOverLimitCount = 0;    // MaxSkinnedInstances를 넘어 레이트레이싱에서 빠진 스킨드 메시 수
		uint64 SkinnedBufferBytes = 0;       // 스키닝 결과 버퍼(위치·재질 정점) 합계
	};
	const Stats& GetLastStats() const { return m_LastStats; }
#pragma endregion

private:
	struct BottomLevelKey
	{
		RHIBuffer* PositionBuffer = nullptr;
		RHIBuffer* IndexBuffer = nullptr;
		uint32 VertexCount = 0;
		uint32 IndexCount = 0;
		uint64 GeometryVersion = 0;
		bool operator==(const BottomLevelKey&) const = default;
	};
	struct BottomLevelKeyHash
	{
		size_t operator()(const BottomLevelKey& key) const;
	};
	struct BottomLevelEntry
	{
		RHIAccelerationStructure* AccelerationStructure = nullptr;
		uint64 LastUsedFrame = 0;
	};
	struct TopLevelKey
	{
		const Scene* Source = nullptr;
		uint32 ViewID = 0;
		bool operator==(const TopLevelKey&) const = default;
	};
	struct TopLevelKeyHash
	{
		size_t operator()(const TopLevelKey& key) const
		{
			const size_t seed = std::hash<const void*>()(key.Source);
			return seed ^ (std::hash<uint32>()(key.ViewID) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
		}
	};
	struct TopLevelRequest
	{
		Scene* Source = nullptr;
		uint32 ViewID = 0;
		bool Reflections = false;
		bool HasCameraPosition = false;
		vec3 CameraPosition{ 0.0f };
	};
	struct TopLevelEntry
	{
		RHIAccelerationStructure* AccelerationStructure = nullptr;
		RaytracedCasterSet Casters;
		uint32 InstanceCount = 0;
		uint32 UnsupportedCasterCount = 0;
		uint32 PendingCasterCount = 0;
		uint32 FailedCasterCount = 0;
		uint64 LastRequestedFrame = 0;
		bool BuiltThisFrame = false;
		// GPU가 이전 프레임의 표를 읽는 동안 덮어쓰지 않도록 프레임 슬롯별로 둔다.
		RHIBuffer* InstanceTables[MAX_SWAPCHAIN_BUFFERS] = {};
		uint32 InstanceTableCapacity[MAX_SWAPCHAIN_BUFFERS] = {};
		uint32 InstanceTableIndex = 0xFFFFFFFFu;
	};
	enum class EBottomLevelState { Ready, Pending, Failed };

	// 스킨드 메시는 같은 메시를 여러 엔티티가 다른 자세로 쓰므로 BLAS를 (씬, 엔티티)마다 둔다.
	struct SkinnedKey
	{
		const Scene* Source = nullptr;
		uint32 Entity = 0;
		bool operator==(const SkinnedKey&) const = default;
	};
	struct SkinnedKeyHash
	{
		size_t operator()(const SkinnedKey& key) const
		{
			const size_t seed = std::hash<const void*>()(key.Source);
			return seed ^ (std::hash<uint32>()(key.Entity) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
		}
	};
	// Unavailable: 이번 프레임에 스키닝하지 않았다(반사·바인드리스 미지원, 스키닝 셰이더 없음 등).
	enum class ESkinnedState { Ready, Pending, Failed, OverLimit, Unavailable };
	struct SkinnedEntry
	{
		// 원본 기하의 식별. 위치 버퍼는 참조를 보유해 해제 뒤 같은 주소가 다른 메시를 가리키는 일을 막는다.
		RHIBuffer* SourcePositions = nullptr;
		uint64 GeometryVersion = 0;
		uint32 VertexCount = 0;
		uint32 IndexCount = 0;
		// 스키닝 결과(Storage). 위치 12바이트, 재질 정점은 원본 간격
		RHIBuffer* Positions = nullptr;
		RHIBuffer* Materials = nullptr;
		uint32 MaterialStride = 0;
		RHIAccelerationStructure* BottomLevel = nullptr;
		uint32 RefitCount = 0;
		bool BuildFailed = false;          // 같은 기하로는 다시 빌드하지 않는다(기하가 바뀌면 다시 시도)
		mat4 WorldMatrix = mat4(1.0f);     // 애니메이터(루트)의 월드 행렬. 그리기 경로와 같다
		ESkinnedState State = ESkinnedState::Pending;
		uint64 StateFrame = 0;             // State가 이번 프레임 값인지
		uint64 LastUsedFrame = 0;
	};
	struct SkinnedJob
	{
		SkinnedEntry* Entry = nullptr;
		Mesh* SourceMesh = nullptr;
		uint32 BoneOffset = 0;
	};

	// Ready일 때만 bottomLevel이 유효하다. 이번 프레임 예산이 없으면 Pending을 반환하고 캐시에 남기지 않는다.
	EBottomLevelState GetOrBuildBottomLevel(Mesh& mesh, Stats& stats, RHIAccelerationStructure*& bottomLevel);
	EResult BuildScene(Scene& scene, uint32 viewID, bool reflections, TopLevelEntry& entry, Stats& stats);
	// constants가 nullptr면 재질에서 만든다(스킨드 메시는 일시 효과를 합친 상수를 넘긴다).
	bool BuildInstanceData(const Mesh& mesh, const MaterialInterface& material, const MaterialConstants* constants,
		RaytracingInstanceData& data) const;
	bool UploadInstanceTable(TopLevelEntry& entry);
	void ReleaseTopLevel(TopLevelEntry& entry);
	void ReleaseBottomLevel(const BottomLevelKey& key, BottomLevelEntry& entry);
	void PruneBottomLevels();

	// 요청된 씬의 스킨드 메시를 프레임마다 한 번씩 스키닝하고 BLAS를 빌드·갱신한다. BuildScene보다 먼저 부른다.
	void PrepareSkinnedMeshes(Stats& stats);
	bool CollectSkinnedJobs(Stats& stats);
	bool EnsureSkinnedOutputs(SkinnedEntry& entry, const Mesh& mesh);
	void UpdateSkinnedBottomLevel(SkinnedEntry& entry, const Mesh& mesh, Stats& stats);
	const SkinnedEntry* FindReadySkinned(const Scene* scene, uint32 entity, ESkinnedState& state) const;
	RHIPipeline* GetSkinningPipeline();
	void ReleaseSkinned(SkinnedEntry& entry);
	void PruneSkinned();

private:
	RHI* m_RHI = nullptr;
	uint64 m_FrameCounter = 0;
	uint64 m_LastBuildFrame = 0;
	std::vector<TopLevelRequest> m_RequestedScenes;
	std::unordered_map<TopLevelKey, TopLevelEntry, TopLevelKeyHash> m_TopLevels;
	// 키의 정점·인덱스 버퍼는 참조를 보유한다. 메시가 해제된 뒤 같은 주소가 재사용되어 다른 기하를 가리키는 일을 막는다.
	std::unordered_map<BottomLevelKey, BottomLevelEntry, BottomLevelKeyHash> m_BottomLevels;
	// 측정 전 시작값이다. 프레임당 새 BLAS 16개 또는 삼각형 100만 개.
	RaytracingBuildBudget m_BuildBudget{ 16, 1'000'000 };
	std::vector<RHIRaytracingInstanceDesc> m_InstanceScratch;
	std::vector<RaytracingInstanceData> m_InstanceDataScratch;
	Stats m_LastStats;

	std::unordered_map<SkinnedKey, SkinnedEntry, SkinnedKeyHash> m_Skinned;
	RaytracingSkinnedLimit m_SkinnedLimit{ MaxSkinnedInstances };
	std::vector<SkinnedJob> m_SkinnedJobs;
	std::vector<mat4> m_BoneScratch;
	// 뼈 행렬 표(업로드 힙). GPU가 이전 프레임의 표를 읽는 동안 덮어쓰지 않도록 프레임 슬롯별로 둔다.
	RHIBuffer* m_BoneTables[MAX_SWAPCHAIN_BUFFERS] = {};
	uint32 m_BoneTableCapacity[MAX_SWAPCHAIN_BUFFERS] = {};
	bool m_SkinningShaderMissing = false;
};
END
