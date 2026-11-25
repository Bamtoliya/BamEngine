#pragma once

#include "System.h"
#include "TerrainComponent.h"

BEGIN(Engine)
class ENGINE_API TerrainSystem : public ISystem
{
private:
	TerrainSystem() = default;
	virtual ~TerrainSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static TerrainSystem* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

	// 지형 단위 및 프레임 전체 청크 LOD·컬링·스트리밍 통계 조회 API
	static TerrainChunkStats GetTerrainChunkStats(const TerrainComponent& terrain) { return terrain.ChunkStats; }
	TerrainChunkStats GetLastFrameTotalChunkStats() const { return m_LastFrameStats; }

private:
	bool EnsureTerrainResources(TerrainComponent& terrain);
	void EnsureTerrainChunks(TerrainComponent& terrain);
	void UpdateTerrainChunkBounds(TerrainComponent& terrain);
	std::shared_ptr<Mesh> EnsureChunkLodMesh(const TerrainComponent& terrain, TerrainChunkRuntime& chunk, uint32 lodLevel);
	void UpdateTerrainHeightmap(TerrainComponent& terrain);
	void UpdateTerrainSplatmap(TerrainComponent& terrain);
	void UpdateTerrainHoleMask(TerrainComponent& terrain);

	TerrainChunkStats m_LastFrameStats{};
};
END