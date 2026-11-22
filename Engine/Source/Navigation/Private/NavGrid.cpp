#include "NavGrid.h"
#include "Scene.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "ComponentEnable.h"

#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>

BEGIN(Engine)

NavGrid::NavGrid(int32 width, int32 height, f32 cellSize, const vec3& origin)
{
	Initialize(width, height, cellSize, origin);
}

bool NavGrid::Initialize(int32 width, int32 height, f32 cellSize, const vec3& origin)
{
	if (width <= 0 || height <= 0 || cellSize <= 0.0f)
	{
		return false;
	}

	m_Width = width;
	m_Height = height;
	m_CellSize = cellSize;
	m_Origin = origin;
	m_Walkable.assign(static_cast<size_t>(m_Width * m_Height), true);
	return true;
}

void NavGrid::SetWalkable(int32 x, int32 z, bool walkable)
{
	if (IsValidCell(x, z))
	{
		m_Walkable[static_cast<size_t>(z * m_Width + x)] = walkable;
	}
}

bool NavGrid::IsWalkable(int32 x, int32 z) const
{
	if (!IsValidCell(x, z))
	{
		return false;
	}
	return m_Walkable[static_cast<size_t>(z * m_Width + x)];
}

bool NavGrid::IsValidCell(int32 x, int32 z) const
{
	return x >= 0 && x < m_Width && z >= 0 && z < m_Height;
}

void NavGrid::ResetWalkable(bool walkable)
{
	std::fill(m_Walkable.begin(), m_Walkable.end(), walkable);
}

bool NavGrid::WorldToCell(const vec3& worldPos, int32& outX, int32& outZ) const
{
	if (m_CellSize <= 0.0f)
	{
		outX = 0;
		outZ = 0;
		return false;
	}

	const f32 relX = (worldPos.x - m_Origin.x) / m_CellSize;
	const f32 relZ = (worldPos.z - m_Origin.z) / m_CellSize;
	outX = static_cast<int32>(std::floor(relX));
	outZ = static_cast<int32>(std::floor(relZ));
	return IsValidCell(outX, outZ);
}

vec3 NavGrid::CellToWorld(int32 x, int32 z) const
{
	const f32 wx = m_Origin.x + (static_cast<f32>(x) + 0.5f) * m_CellSize;
	const f32 wz = m_Origin.z + (static_cast<f32>(z) + 0.5f) * m_CellSize;
	return vec3(wx, m_Origin.y, wz);
}

bool NavGrid::ComputeColliderAABB(const ColliderComponent& col, const mat4& worldMatrix, BoundingBox& outAABB)
{
	const vec3 worldCenter = vec3(worldMatrix * vec4(col.GetOffset(), 1.0f));
	const vec3 col0 = vec3(worldMatrix[0]);
	const vec3 col1 = vec3(worldMatrix[1]);
	const vec3 col2 = vec3(worldMatrix[2]);

	if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
	{
		if (!box->HasValidParameters()) return false;
		const vec3 halfSize = box->Size * 0.5f;
		const vec3 extent = glm::abs(col0) * halfSize.x + glm::abs(col1) * halfSize.y + glm::abs(col2) * halfSize.z;
		outAABB = BoundingBox(worldCenter - extent, worldCenter + extent);
		return true;
	}
	if (const auto* sphere = std::get_if<SphereShape>(&col.GetShape()))
	{
		if (!sphere->HasValidParameters()) return false;
		const f32 maxScale = glm::max(glm::length(col0), glm::max(glm::length(col1), glm::length(col2)));
		const vec3 extent(sphere->Radius * maxScale);
		outAABB = BoundingBox(worldCenter - extent, worldCenter + extent);
		return true;
	}
	if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
	{
		if (!cap->HasValidParameters()) return false;
		const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
		const f32 scaleXZ = glm::max(glm::length(col0), glm::length(col2));
		const vec3 extent = glm::abs(col1) * halfSegment + vec3(cap->Radius * scaleXZ);
		outAABB = BoundingBox(worldCenter - extent, worldCenter + extent);
		return true;
	}
	return false;
}

void NavGrid::BakeObstaclesFromScene(const Scene& scene, f32 agentRadius)
{
	const auto& world = scene.GetRegistry();
	auto colliderView = world.view<ColliderComponent>();

	for (auto entity : colliderView)
	{
		if (!IsComponentEnabled<ColliderComponent>(world, entity))
		{
			continue;
		}

		const auto& col = colliderView.get<ColliderComponent>(entity);
		const auto& settings = col.GetSettings();
		if (!settings.Enabled || settings.IsTrigger)
		{
			continue;
		}

		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
			{
				continue;
			}
		}

		mat4 worldMatrix = glm::identity<mat4>();
		if (const auto* wt = world.try_get<WorldTransformComponent>(entity))
		{
			worldMatrix = wt->worldMatrix;
		}
		else if (const auto* t = world.try_get<TransformComponent>(entity))
		{
			worldMatrix = t->GetLocalMatrix();
		}

		BoundingBox aabb;
		if (ComputeColliderAABB(col, worldMatrix, aabb))
		{
			aabb.min.x -= agentRadius;
			aabb.min.z -= agentRadius;
			aabb.max.x += agentRadius;
			aabb.max.z += agentRadius;

			int32 minCellX = static_cast<int32>(std::floor((aabb.min.x - m_Origin.x) / m_CellSize));
			int32 minCellZ = static_cast<int32>(std::floor((aabb.min.z - m_Origin.z) / m_CellSize));
			int32 maxCellX = static_cast<int32>(std::floor((aabb.max.x - m_Origin.x) / m_CellSize));
			int32 maxCellZ = static_cast<int32>(std::floor((aabb.max.z - m_Origin.z) / m_CellSize));

			minCellX = std::clamp(minCellX, 0, m_Width - 1);
			maxCellX = std::clamp(maxCellX, 0, m_Width - 1);
			minCellZ = std::clamp(minCellZ, 0, m_Height - 1);
			maxCellZ = std::clamp(maxCellZ, 0, m_Height - 1);

			for (int32 z = minCellZ; z <= maxCellZ; ++z)
			{
				for (int32 x = minCellX; x <= maxCellX; ++x)
				{
					SetWalkable(x, z, false);
				}
			}
		}
	}
}

f32 NavGrid::CalculateHeuristic(int32 x1, int32 z1, int32 x2, int32 z2, f32 cellSize)
{
	const f32 dx = static_cast<f32>(std::abs(x1 - x2));
	const f32 dz = static_cast<f32>(std::abs(z1 - z2));
	const f32 minD = std::min(dx, dz);
	const f32 maxD = std::max(dx, dz);
	return (1.41421356f * minD + (maxD - minD)) * cellSize;
}

bool NavGrid::IsLineWalkable(const vec3& a, const vec3& b) const
{
	const vec2 p0(a.x, a.z);
	const vec2 p1(b.x, b.z);
	const vec2 delta = p1 - p0;
	const f32 dist = glm::length(delta);
	if (dist <= 1e-4f)
	{
		int32 cx = 0, cz = 0;
		return WorldToCell(a, cx, cz) && IsWalkable(cx, cz);
	}

	const f32 step = glm::max(m_CellSize * 0.25f, 0.05f);
	const int32 numSamples = static_cast<int32>(std::ceil(dist / step));
	for (int32 i = 0; i <= numSamples; ++i)
	{
		const f32 t = static_cast<f32>(i) / static_cast<f32>(numSamples);
		const vec2 pt = glm::mix(p0, p1, t);
		int32 cx = 0, cz = 0;
		if (!WorldToCell(vec3(pt.x, m_Origin.y, pt.y), cx, cz) || !IsWalkable(cx, cz))
		{
			return false;
		}
	}
	return true;
}

bool NavGrid::FindPath(
	const vec3& startWorld,
	const vec3& goalWorld,
	std::vector<vec3>& outWaypoints,
	bool allowDiagonal) const
{
	outWaypoints.clear();

	if (m_Width <= 0 || m_Height <= 0 || m_Walkable.empty())
	{
		return false;
	}

	int32 startX = 0, startZ = 0;
	if (!WorldToCell(startWorld, startX, startZ))
	{
		return false;
	}

	int32 goalX = 0, goalZ = 0;
	if (!WorldToCell(goalWorld, goalX, goalZ))
	{
		return false;
	}

	if (!IsWalkable(startX, startZ) || !IsWalkable(goalX, goalZ))
	{
		return false;
	}

	if (startX == goalX && startZ == goalZ)
	{
		outWaypoints.push_back(startWorld);
		outWaypoints.push_back(goalWorld);
		return true;
	}

	struct Node
	{
		int32 x;
		int32 z;
		f32 gCost;
		f32 fCost;
	};

	struct CompareNode
	{
		bool operator()(const Node& a, const Node& b) const
		{
			return a.fCost > b.fCost;
		}
	};

	const size_t totalCells = static_cast<size_t>(m_Width * m_Height);
	std::vector<f32> gCosts(totalCells, std::numeric_limits<f32>::infinity());
	std::vector<int32> parentIndex(totalCells, -1);
	std::vector<bool> closed(totalCells, false);

	const int32 startIndex = startZ * m_Width + startX;
	const int32 goalIndex = goalZ * m_Width + goalX;

	gCosts[startIndex] = 0.0f;
	std::priority_queue<Node, std::vector<Node>, CompareNode> openSet;
	openSet.push({ startX, startZ, 0.0f, CalculateHeuristic(startX, startZ, goalX, goalZ, m_CellSize) });

	static constexpr int32 dxOrthogonal[4] = { 1, -1, 0, 0 };
	static constexpr int32 dzOrthogonal[4] = { 0, 0, 1, -1 };

	static constexpr int32 dxDiagonal[4] = { 1, 1, -1, -1 };
	static constexpr int32 dzDiagonal[4] = { 1, -1, 1, -1 };

	bool found = false;

	while (!openSet.empty())
	{
		const Node current = openSet.top();
		openSet.pop();

		if (current.x == goalX && current.z == goalZ)
		{
			found = true;
			break;
		}

		const int32 currentIdx = current.z * m_Width + current.x;
		if (closed[currentIdx])
		{
			continue;
		}
		if (current.gCost > gCosts[currentIdx])
		{
			continue;
		}
		closed[currentIdx] = true;

		// 1. Orthogonal neighbors
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 nx = current.x + dxOrthogonal[i];
			const int32 nz = current.z + dzOrthogonal[i];

			if (!IsValidCell(nx, nz) || !IsWalkable(nx, nz))
			{
				continue;
			}

			const int32 nIdx = nz * m_Width + nx;
			if (closed[nIdx])
			{
				continue;
			}

			const f32 tentativeG = current.gCost + m_CellSize;
			if (tentativeG < gCosts[nIdx])
			{
				gCosts[nIdx] = tentativeG;
				parentIndex[nIdx] = currentIdx;
				const f32 fCost = tentativeG + CalculateHeuristic(nx, nz, goalX, goalZ, m_CellSize);
				openSet.push({ nx, nz, tentativeG, fCost });
			}
		}

		// 2. Diagonal neighbors (corner-cut safe)
		if (allowDiagonal)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const int32 nx = current.x + dxDiagonal[i];
				const int32 nz = current.z + dzDiagonal[i];

				if (!IsValidCell(nx, nz) || !IsWalkable(nx, nz))
				{
					continue;
				}

				// Corner cutting prevention: both orthogonal adjacent cells must be walkable
				if (!IsWalkable(current.x + dxDiagonal[i], current.z) ||
					!IsWalkable(current.x, current.z + dzDiagonal[i]))
				{
					continue;
				}

				const int32 nIdx = nz * m_Width + nx;
				if (closed[nIdx])
				{
					continue;
				}

				const f32 tentativeG = current.gCost + 1.41421356f * m_CellSize;
				if (tentativeG < gCosts[nIdx])
				{
					gCosts[nIdx] = tentativeG;
					parentIndex[nIdx] = currentIdx;
					const f32 fCost = tentativeG + CalculateHeuristic(nx, nz, goalX, goalZ, m_CellSize);
					openSet.push({ nx, nz, tentativeG, fCost });
				}
			}
		}
	}

	if (!found)
	{
		return false;
	}

	// Reconstruct raw cell path
	std::vector<vec3> rawWaypoints;
	int32 currIdx = goalIndex;
	while (currIdx != -1)
	{
		const int32 cx = currIdx % m_Width;
		const int32 cz = currIdx / m_Width;
		rawWaypoints.push_back(CellToWorld(cx, cz));
		if (currIdx == startIndex)
		{
			break;
		}
		currIdx = parentIndex[currIdx];
	}

	std::reverse(rawWaypoints.begin(), rawWaypoints.end());

	if (!rawWaypoints.empty())
	{
		rawWaypoints.front() = startWorld;
		rawWaypoints.back() = goalWorld;
	}

	// Path smoothing (string pulling)
	if (rawWaypoints.size() <= 2)
	{
		outWaypoints = std::move(rawWaypoints);
		return true;
	}

	outWaypoints.push_back(rawWaypoints.front());
	size_t current = 0;
	while (current + 1 < rawWaypoints.size())
	{
		size_t furthest = current + 1;
		for (size_t next = rawWaypoints.size() - 1; next > current + 1; --next)
		{
			if (IsLineWalkable(rawWaypoints[current], rawWaypoints[next]))
			{
				furthest = next;
				break;
			}
		}
		outWaypoints.push_back(rawWaypoints[furthest]);
		current = furthest;
	}

	return true;
}

END
