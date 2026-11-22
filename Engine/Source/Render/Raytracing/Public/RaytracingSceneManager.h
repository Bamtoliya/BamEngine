#pragma once

#include "Base.h"
#include "RHIAccelerationStructure.h"

struct RaytracingSceneManagerDesc
{
	Engine::RHI* RHI = { nullptr };
};

BEGIN(Engine)
class Scene;
class Mesh;
class MaterialInterface;
class RHIBuffer;

// 렌더되는 씬마다 레이트레이싱 그림자용 TLAS를 매 프레임 다시 빌드하고, 메시별 BLAS를 캐시한다.
// - 뷰포트 제출 단계에서 RequestScene으로 씬을 등록하고, Renderer::Render 시작에 BuildRequestedScenes가 빌드한다.
// - 지원 대상: 활성·보이는 castShadow 정적 메시 중 IsShadowCasterSupported를 만족하는 것(LOD는 주 뷰포트의 currentLODIndex).
// - 미지원 캐스터(스킨드·동적·스플라인·불투명이 아닌 재질·그림자 전용 정점 셰이더)는 기존 섀도맵이 담당한다.
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
	// 이번 프레임에 TLAS가 필요한 씬을 등록한다. 같은 씬을 여러 뷰포트가 등록해도 한 번만 빌드한다.
	void RequestScene(Scene* scene);
	// 이번 프레임에 TLAS가 요청된 씬인지. TLAS에 없는 씬의 캐스터를 섀도맵에서 빼지 않도록 제출 단계에서 확인한다.
	bool IsSceneRequested(const Scene* scene) const;
	// 빌드 전에 프레임이 중단되면 해제된 씬 포인터가 다음 프레임에 남지 않도록 프레임 큐와 함께 비운다.
	void ClearRequests() { m_RequestedScenes.clear(); }
	// 요청된 씬의 BLAS를 준비하고 TLAS를 다시 빌드한다. 요청되지 않은 씬의 TLAS는 해제하고 요청 목록을 비운다.
	EResult BuildRequestedScenes();
	// 이번 프레임에 빌드된 씬의 TLAS. 요청되지 않았거나 빌드에 실패했으면 nullptr.
	RHIAccelerationStructure* GetTopLevelAS(const Scene* scene) const;
#pragma endregion

#pragma region Caster Policy
public:
	// 레이트레이싱 그림자가 형상을 그대로 표현할 수 있는 캐스터인지. 섀도맵 패스 필터도 같은 판정을 사용해야 한다.
	static bool IsShadowCasterSupported(const Mesh* mesh, const MaterialInterface* material);
#pragma endregion

#pragma region Statistics
public:
	struct Stats
	{
		uint32 SceneCount = 0;
		uint32 InstanceCount = 0;
		uint32 UnsupportedCasterCount = 0;   // 섀도맵으로 넘긴 캐스터 수(스킨드 포함)
		uint32 FailedCasterCount = 0;        // BLAS 생성 실패로 어느 경로에도 들어가지 못한 캐스터 수
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
	struct TopLevelEntry
	{
		RHIAccelerationStructure* AccelerationStructure = nullptr;
		uint64 LastRequestedFrame = 0;
		bool BuiltThisFrame = false;
	};

	RHIAccelerationStructure* GetOrBuildBottomLevel(Mesh& mesh, Stats& stats);
	EResult BuildScene(Scene& scene, TopLevelEntry& entry, Stats& stats);
	void ReleaseBottomLevel(const BottomLevelKey& key, BottomLevelEntry& entry);
	void PruneBottomLevels();

private:
	RHI* m_RHI = nullptr;
	uint64 m_FrameCounter = 0;
	std::vector<Scene*> m_RequestedScenes;
	std::unordered_map<const Scene*, TopLevelEntry> m_TopLevels;
	// 키의 정점·인덱스 버퍼는 참조를 보유한다. 메시가 해제된 뒤 같은 주소가 재사용되어 다른 기하를 가리키는 일을 막는다.
	std::unordered_map<BottomLevelKey, BottomLevelEntry, BottomLevelKeyHash> m_BottomLevels;
	std::vector<RHIRaytracingInstanceDesc> m_InstanceScratch;
	Stats m_LastStats;
};
END
