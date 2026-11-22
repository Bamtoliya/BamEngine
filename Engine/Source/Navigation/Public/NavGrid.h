#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "BoundingBox.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

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
	[[nodiscard]] bool IsValidCell(int32 x, int32 z) const;
	void ResetWalkable(bool walkable = true);

	[[nodiscard]] bool WorldToCell(const vec3& worldPos, int32& outX, int32& outZ) const;
	[[nodiscard]] vec3 CellToWorld(int32 x, int32 z) const;

	[[nodiscard]] int32 GetWidth() const { return m_Width; }
	[[nodiscard]] int32 GetHeight() const { return m_Height; }
	[[nodiscard]] f32 GetCellSize() const { return m_CellSize; }
	[[nodiscard]] const vec3& GetOrigin() const { return m_Origin; }

	void BakeObstaclesFromScene(const Scene& scene, f32 agentRadius = 0.4f);

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
	std::vector<bool> m_Walkable;
};

END
