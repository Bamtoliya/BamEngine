#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "BoundingBox.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

BEGIN(Engine)

class Scene;
struct ColliderComponent;

class ENGINE_API NavGrid
{
public:
	NavGrid() = default;
	NavGrid(int32 width, int32 height, f32 cellSize, const vec3& origin = vec3(0.0f));
	~NavGrid() = default;

	bool Initialize(int32 width, int32 height, f32 cellSize, const vec3& origin = vec3(0.0f));

	void SetWalkable(int32 x, int32 z, bool walkable);
	[[nodiscard]] bool IsWalkable(int32 x, int32 z) const;
	[[nodiscard]] bool IsDynamicBlocked(int32 gx, int32 gz) const;
	int32 SetDynamicObstacleBox(const vec3& minBounds, const vec3& maxBounds, bool addObstacle);
	int32 SetDynamicObstacleCircle(const vec3& center, f32 radius, bool addObstacle);
	void ClearDynamicObstacles();
	void SetCellHeight(int32 x, int32 z, f32 height);
	[[nodiscard]] f32 GetCellHeight(int32 x, int32 z) const;
	void SetCellCost(int32 x, int32 z, f32 cost);
	[[nodiscard]] f32 GetCellCost(int32 x, int32 z) const;
	void SetMaxStepHeight(f32 maxStepHeight) { m_MaxStepHeight = maxStepHeight; }
	[[nodiscard]] f32 GetMaxStepHeight() const { return m_MaxStepHeight; }
	[[nodiscard]] bool IsValidCell(int32 x, int32 z) const;
	void ResetWalkable(bool walkable = true);

	[[nodiscard]] bool WorldToCell(const vec3& worldPos, int32& outX, int32& outZ) const;
	[[nodiscard]] vec3 CellToWorld(int32 x, int32 z) const;

	[[nodiscard]] int32 GetWidth() const { return m_Width; }
	[[nodiscard]] int32 GetHeight() const { return m_Height; }
	[[nodiscard]] f32 GetCellSize() const { return m_CellSize; }
	[[nodiscard]] const vec3& GetOrigin() const { return m_Origin; }

	void BakeObstaclesFromScene(const Scene& scene, f32 agentRadius = 0.4f, f32 maxSlopeDegrees = 45.0f, f32 maxStepHeight = 0.5f);
	[[nodiscard]] static bool CalculateSceneBounds(const Scene& scene, BoundingBox& outBounds);

	[[nodiscard]] bool SaveToFile(const std::filesystem::path& path) const;
	[[nodiscard]] bool LoadFromFile(const std::filesystem::path& path);
	[[nodiscard]] static std::filesystem::path MakeDefaultPath(const std::filesystem::path& scenePath);
	[[nodiscard]] static std::filesystem::path ResolveDataPath(const std::string& navGridUri);

	[[nodiscard]] bool FindPath(
		const vec3& startWorld,
		const vec3& goalWorld,
		std::vector<vec3>& outWaypoints,
		bool allowDiagonal = true) const;

	[[nodiscard]] bool IsLineWalkable(const vec3& a, const vec3& b) const;

private:
	static bool ComputeColliderAABB(const ColliderComponent& col, const mat4& worldMatrix, BoundingBox& outAABB);
	static f32 CalculateHeuristic(int32 x1, int32 z1, int32 x2, int32 z2, f32 cellSize);

private:
	int32 m_Width = 0;
	int32 m_Height = 0;
	f32 m_CellSize = 1.0f;
	vec3 m_Origin = vec3(0.0f);
	f32 m_MaxStepHeight = 0.5f;
	std::vector<bool> m_Walkable;
	std::vector<uint16_t> m_DynamicObstacleCounts;
	std::vector<f32> m_CellHeights;
	std::vector<f32> m_CellCosts;
};

END
