#pragma once

#include "Base.h"
#include "RHIAccelerationStructure.h"
#include "RaytracingBuildPolicy.h"

struct RaytracingSceneManagerDesc
{
	Engine::RHI* RHI = { nullptr };
};

BEGIN(Engine)
class Scene;
class Mesh;
class MaterialInterface;
class RHIBuffer;

// 렌더되는 씬과 뷰(Geometry 패스 ID)의 조합마다 레이트레이싱 그림자용 TLAS를 매 프레임 다시 빌드하고, 메시별 BLAS를 캐시한다.
// - 뷰포트 제출 단계에서 RequestScene으로 씬을 등록하고, Renderer::Render 시작에 BuildRequestedScenes가 빌드한다.
// - 지원 대상: 활성·보이는 castShadow 정적 메시 중 IsShadowCasterSupported를 만족하는 것(LOD는 그 뷰가 고른 LOD).
// - 미지원 캐스터(스킨드·동적·스플라인·불투명이 아닌 재질·그림자 전용 정점 셰이더)는 기존 섀도맵이 담당한다.
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
	void RequestScene(Scene* scene, uint32 viewID);
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
#pragma endregion

#pragma region Caster Policy
public:
	// 레이트레이싱 그림자가 형상을 그대로 표현할 수 있는 캐스터인지. 섀도맵 제외 여부는 이 판정이 아니라 GetTopLevelCasters로 정한다.
	static bool IsShadowCasterSupported(const Mesh* mesh, const MaterialInterface* material);
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
		uint32 BottomLevelCount = 0;
		uint32 BottomLevelBuiltThisFrame = 0;
		uint64 BottomLevelBytes = 0;
		uint64 TopLevelBytes = 0;
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
	};
	enum class EBottomLevelState { Ready, Pending, Failed };

	// Ready일 때만 bottomLevel이 유효하다. 이번 프레임 예산이 없으면 Pending을 반환하고 캐시에 남기지 않는다.
	EBottomLevelState GetOrBuildBottomLevel(Mesh& mesh, Stats& stats, RHIAccelerationStructure*& bottomLevel);
	EResult BuildScene(Scene& scene, uint32 viewID, TopLevelEntry& entry, Stats& stats);
	void ReleaseBottomLevel(const BottomLevelKey& key, BottomLevelEntry& entry);
	void PruneBottomLevels();

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
	Stats m_LastStats;
};
END
