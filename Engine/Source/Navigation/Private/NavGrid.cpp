#include "NavGrid.h"
#include "Scene.h"
#include "ColliderComponent.h"
#include "TerrainComponent.h"
#include "TerrainUtility.h"
#include "TerrainLayerAsset.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "ComponentEnable.h"
#include "PlatformPath.h"
#include "Functions.h"

#include <queue>
#include <cmath>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <system_error>

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
	const size_t total = static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height);
	m_Walkable.assign(total, true);
	m_CellHeights.assign(total, m_Origin.y);
	m_CellCosts.assign(total, 1.0f);
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

void NavGrid::SetCellHeight(int32 x, int32 z, f32 height)
{
	if (IsValidCell(x, z) && !m_CellHeights.empty())
	{
		m_CellHeights[static_cast<size_t>(z * m_Width + x)] = height;
	}
}

f32 NavGrid::GetCellHeight(int32 x, int32 z) const
{
	if (!IsValidCell(x, z) || m_CellHeights.empty())
	{
		return m_Origin.y;
	}
	return m_CellHeights[static_cast<size_t>(z * m_Width + x)];
}

void NavGrid::SetCellCost(int32 x, int32 z, f32 cost)
{
	if (IsValidCell(x, z) && !m_CellCosts.empty())
	{
		m_CellCosts[static_cast<size_t>(z * m_Width + x)] = std::max(1.0f, cost);
	}
}

f32 NavGrid::GetCellCost(int32 x, int32 z) const
{
	if (!IsValidCell(x, z) || m_CellCosts.empty())
	{
		return 1.0f;
	}
	return m_CellCosts[static_cast<size_t>(z * m_Width + x)];
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
	return vec3(wx, GetCellHeight(x, z), wz);
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

bool NavGrid::CalculateSceneBounds(const Scene& scene, BoundingBox& outBounds)
{
	const auto& world = scene.GetRegistry();
	auto colliderView = world.view<ColliderComponent>();

	bool found = false;
	vec3 minPt(std::numeric_limits<f32>::max());
	vec3 maxPt(std::numeric_limits<f32>::lowest());

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
			minPt = glm::min(minPt, aabb.min);
			maxPt = glm::max(maxPt, aabb.max);
			found = true;
		}
	}

	auto terrainView = world.view<TerrainComponent>();
	for (auto entity : terrainView)
	{
		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
			{
				continue;
			}
		}

		const auto& terrain = terrainView.get<TerrainComponent>(entity);
		if (!(terrain.Width > 0.0f) || !(terrain.Length > 0.0f))
		{
			continue;
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

		const f32 hw = terrain.Width * 0.5f;
		const f32 hl = terrain.Length * 0.5f;
		const f32 maxY = std::max(0.0f, terrain.MaxHeight);
		const vec3 corners[8] = {
			{ -hw, 0.0f, -hl }, { hw, 0.0f, -hl }, { -hw, 0.0f, hl }, { hw, 0.0f, hl },
			{ -hw, maxY, -hl }, { hw, maxY, -hl }, { -hw, maxY, hl }, { hw, maxY, hl }
		};
		for (const vec3& c : corners)
		{
			const vec3 wp = vec3(worldMatrix * vec4(c, 1.0f));
			minPt = glm::min(minPt, wp);
			maxPt = glm::max(maxPt, wp);
		}
		found = true;
	}

	if (found)
	{
		outBounds = BoundingBox(minPt, maxPt);
		return true;
	}
	return false;
}

void NavGrid::BakeObstaclesFromScene(const Scene& scene, f32 agentRadius, f32 maxSlopeDegrees, f32 maxStepHeight)
{
	m_MaxStepHeight = std::max(0.0f, maxStepHeight);
	const size_t total = static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height);
	if (m_CellHeights.size() != total) m_CellHeights.assign(total, m_Origin.y);
	if (m_CellCosts.size() != total) m_CellCosts.assign(total, 1.0f);

	const auto& world = scene.GetRegistry();
	const f32 cosMaxSlope = std::cos(glm::radians(std::clamp(maxSlopeDegrees, 0.0f, 89.9f)));
	std::vector<bool> hasTerrainHeight(total, false);

	// 1. 지형 높이·경사·Blocked 속성·레이어 Walkable/NavCost 베이크
	auto terrainView = world.view<TerrainComponent>();
	for (auto entity : terrainView)
	{
		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
			{
				continue;
			}
		}

		const auto& terrain = terrainView.get<TerrainComponent>(entity);
		uint32 axisCount = 0;
		std::size_t sampleCount = 0;
		if (!(terrain.Width > 0.0f) || !(terrain.Length > 0.0f) ||
			!TerrainUtility::TryGetGridLayout(terrain.Resolution, axisCount, sampleCount) ||
			terrain.HeightData.size() != sampleCount)
		{
			continue;
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

		const mat4 invWorld = glm::inverse(worldMatrix);
		const mat3 normalMat = glm::mat3(glm::transpose(invWorld));
		const f32 halfW = terrain.Width * 0.5f;
		const f32 halfL = terrain.Length * 0.5f;
		const f32 stepX = terrain.Width / static_cast<f32>(terrain.Resolution);
		const f32 stepZ = terrain.Length / static_cast<f32>(terrain.Resolution);
		const bool hasAttr = terrain.AttributeData.size() == sampleCount;
		const bool hasSplat0 = terrain.SplatData.size() == sampleCount * 4;
		const bool hasSplat1 = terrain.SplatDataExtra.size() == sampleCount * 4;

		const auto sampleHeightAt = [&](uint32 vx, uint32 vz) -> f32
		{
			vx = std::min(vx, terrain.Resolution);
			vz = std::min(vz, terrain.Resolution);
			return terrain.HeightData[static_cast<std::size_t>(vz) * axisCount + vx] * terrain.MaxHeight;
		};

		for (int32 cz = 0; cz < m_Height; ++cz)
		{
			const f32 wz = m_Origin.z + (static_cast<f32>(cz) + 0.5f) * m_CellSize;
			for (int32 cx = 0; cx < m_Width; ++cx)
			{
				const f32 wx = m_Origin.x + (static_cast<f32>(cx) + 0.5f) * m_CellSize;
				const vec3 local = vec3(invWorld * vec4(wx, m_Origin.y, wz, 1.0f));
				if (local.x < -halfW || local.x > halfW || local.z < -halfL || local.z > halfL)
				{
					continue;
				}

				const f32 gx = std::clamp((local.x + halfW) / stepX, 0.0f, static_cast<f32>(terrain.Resolution));
				const f32 gz = std::clamp((local.z + halfL) / stepZ, 0.0f, static_cast<f32>(terrain.Resolution));
				const uint32 x0 = static_cast<uint32>(gx);
				const uint32 z0 = static_cast<uint32>(gz);
				const uint32 x1 = std::min(x0 + 1, terrain.Resolution);
				const uint32 z1 = std::min(z0 + 1, terrain.Resolution);
				const f32 tx = gx - static_cast<f32>(x0);
				const f32 tz = gz - static_cast<f32>(z0);

				const f32 h00 = sampleHeightAt(x0, z0);
				const f32 h10 = sampleHeightAt(x1, z0);
				const f32 h01 = sampleHeightAt(x0, z1);
				const f32 h11 = sampleHeightAt(x1, z1);
				const f32 localY = glm::mix(glm::mix(h00, h10, tx), glm::mix(h01, h11, tx), tz);
				const f32 worldY = (worldMatrix * vec4(local.x, localY, local.z, 1.0f)).y;

				const size_t cellIdx = static_cast<size_t>(cz * m_Width + cx);
				if (hasTerrainHeight[cellIdx] && worldY < m_CellHeights[cellIdx])
				{
					continue;
				}
				hasTerrainHeight[cellIdx] = true;
				m_CellHeights[cellIdx] = worldY;

				// 속성·레이어는 가장 가까운 정점에서 읽는다.
				const uint32 nx = static_cast<uint32>(std::lround(gx));
				const uint32 nz = static_cast<uint32>(std::lround(gz));
				// 경사는 셀 중심이 놓인 칸(정점 네 개)의 쌍선형 기울기로 구한다. 가까운 정점의 좌우 차분은 정점 한 줄짜리
				// 능선처럼 그 정점이 꼭대기이면 양옆 높이가 같아 기울기가 0이 되어 급경사 칸을 평지로 오판한다.
				const f32 dHdx = stepX > 1e-5f ? glm::mix(h10 - h00, h11 - h01, tz) / stepX : 0.0f;
				const f32 dHdz = stepZ > 1e-5f ? glm::mix(h01 - h00, h11 - h10, tx) / stepZ : 0.0f;
				const vec3 localNormal = glm::normalize(vec3(-dHdx, 1.0f, -dHdz));
				const vec3 worldNormal = glm::normalize(normalMat * localNormal);
				if (worldNormal.y < cosMaxSlope)
				{
					SetWalkable(cx, cz, false);
				}

				const std::size_t vIdx = static_cast<std::size_t>(nz) * axisCount + nx;
				if (hasAttr && (terrain.AttributeData[vIdx] == static_cast<uint8>(ETerrainAttribute::Blocked) ||
					terrain.AttributeData[vIdx] == static_cast<uint8>(ETerrainAttribute::Hole)))
				{
					SetWalkable(cx, cz, false);
				}

				// 지배 레이어의 Walkable / NavCost 반영
				if (hasSplat0 && !terrain.Layers.empty())
				{
					uint32 dominantLayer = 0;
					uint8 maxWeight = 0;
					for (uint32 l = 0; l < TerrainComponent::MaxLayers; ++l)
					{
						uint8 w = 0;
						if (l < 4) w = terrain.SplatData[vIdx * 4 + l];
						else if (hasSplat1) w = terrain.SplatDataExtra[vIdx * 4 + (l - 4)];
						if (w > maxWeight)
						{
							maxWeight = w;
							dominantLayer = l;
						}
					}
					if (maxWeight > 0 && dominantLayer < terrain.Layers.size())
					{
						if (const auto* layerAsset = terrain.Layers[dominantLayer].Layer.Get())
						{
							const auto& s = layerAsset->GetSettings();
							if (!s.Walkable) SetWalkable(cx, cz, false);
							else SetCellCost(cx, cz, std::max(1.0f, s.NavCost));
						}
					}
				}
			}
		}
	}

	// 2. 콜라이더 장애물 베이크
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
	int32 prevX = -1, prevZ = -1;
	for (int32 i = 0; i <= numSamples; ++i)
	{
		const f32 t = static_cast<f32>(i) / static_cast<f32>(numSamples);
		const vec2 pt = glm::mix(p0, p1, t);
		int32 cx = 0, cz = 0;
		if (!WorldToCell(vec3(pt.x, m_Origin.y, pt.y), cx, cz) || !IsWalkable(cx, cz))
		{
			return false;
		}
		if (GetCellCost(cx, cz) > 1.0f + 1e-4f)
		{
			return false;
		}
		if (prevX != -1 && (cx != prevX || cz != prevZ) &&
			std::abs(GetCellHeight(cx, cz) - GetCellHeight(prevX, prevZ)) > m_MaxStepHeight)
		{
			return false;
		}
		prevX = cx;
		prevZ = cz;
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

			if (std::abs(GetCellHeight(nx, nz) - GetCellHeight(current.x, current.z)) > m_MaxStepHeight)
			{
				continue;
			}

			const int32 nIdx = nz * m_Width + nx;
			if (closed[nIdx])
			{
				continue;
			}

			const f32 tentativeG = current.gCost + m_CellSize * GetCellCost(nx, nz);
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

				if (std::abs(GetCellHeight(nx, nz) - GetCellHeight(current.x, current.z)) > m_MaxStepHeight)
				{
					continue;
				}

				const int32 nIdx = nz * m_Width + nx;
				if (closed[nIdx])
				{
					continue;
				}

				const f32 tentativeG = current.gCost + 1.41421356f * m_CellSize * GetCellCost(nx, nz);
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

namespace
{
	constexpr std::array<char, 8> NavGridMagic{ 'B', 'A', 'M', 'N', 'A', 'V', 'G', '\0' };
	constexpr uint32 NavGridFileVersion = 1;

	struct NavGridFileHeader
	{
		std::array<char, 8> Magic{};
		uint32 Version = 0;
		int32 Width = 0;
		int32 Height = 0;
		f32 CellSize = 0.0f;
		f32 Origin[3]{};
		f32 MaxStepHeight = 0.5f;
	};
	static_assert(sizeof(NavGridFileHeader) == 40, "NavGrid header layout must not change.");
}

bool NavGrid::SaveToFile(const std::filesystem::path& path) const
{
	if (path.empty() || m_Width <= 0 || m_Height <= 0 || !(m_CellSize > 0.0f))
	{
		return false;
	}

	const size_t count = static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height);
	if (m_Walkable.size() != count)
	{
		return false;
	}

	std::vector<uint8> walkableBytes(count);
	for (size_t i = 0; i < count; ++i)
	{
		walkableBytes[i] = m_Walkable[i] ? 1 : 0;
	}
	const std::vector<f32> defaultHeights = m_CellHeights.size() == count ? std::vector<f32>{} : std::vector<f32>(count, m_Origin.y);
	const std::vector<f32> defaultCosts = m_CellCosts.size() == count ? std::vector<f32>{} : std::vector<f32>(count, 1.0f);
	const auto& heights = m_CellHeights.size() == count ? m_CellHeights : defaultHeights;
	const auto& costs = m_CellCosts.size() == count ? m_CellCosts : defaultCosts;

	std::error_code ec;
	if (path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path(), ec);
	}

	auto tempPath = path;
	tempPath += L".tmp";
	{
		std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}
		NavGridFileHeader header{
			NavGridMagic,
			NavGridFileVersion,
			m_Width,
			m_Height,
			m_CellSize,
			{ m_Origin.x, m_Origin.y, m_Origin.z },
			m_MaxStepHeight
		};
		file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		file.write(reinterpret_cast<const char*>(walkableBytes.data()), static_cast<std::streamsize>(walkableBytes.size()));
		file.write(reinterpret_cast<const char*>(heights.data()), static_cast<std::streamsize>(heights.size() * sizeof(f32)));
		file.write(reinterpret_cast<const char*>(costs.data()), static_cast<std::streamsize>(costs.size() * sizeof(f32)));
		if (!file.flush())
		{
			std::filesystem::remove(tempPath, ec);
			return false;
		}
	}

	std::filesystem::rename(tempPath, path, ec);
	if (ec)
	{
		std::filesystem::remove(path, ec);
		ec.clear();
		std::filesystem::rename(tempPath, path, ec);
		if (ec)
		{
			std::filesystem::remove(tempPath, ec);
			return false;
		}
	}
	return true;
}

bool NavGrid::LoadFromFile(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return false;
	}

	std::error_code ec;
	const auto fileSize = std::filesystem::file_size(path, ec);
	if (ec || fileSize < sizeof(NavGridFileHeader))
	{
		return false;
	}

	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return false;
	}

	NavGridFileHeader header{};
	if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)))
	{
		return false;
	}
	if (header.Magic != NavGridMagic || header.Version != NavGridFileVersion ||
		header.Width <= 0 || header.Height <= 0 || !(header.CellSize > 0.0f) ||
		header.Width > 4096 || header.Height > 4096)
	{
		return false;
	}

	const size_t count = static_cast<size_t>(header.Width) * static_cast<size_t>(header.Height);
	const uint64 expectedSize = sizeof(NavGridFileHeader) + count * (sizeof(uint8) + sizeof(f32) + sizeof(f32));
	if (fileSize != expectedSize)
	{
		return false;
	}

	std::vector<uint8> walkableBytes(count);
	std::vector<f32> heights(count);
	std::vector<f32> costs(count);
	if (!file.read(reinterpret_cast<char*>(walkableBytes.data()), static_cast<std::streamsize>(walkableBytes.size())) ||
		!file.read(reinterpret_cast<char*>(heights.data()), static_cast<std::streamsize>(heights.size() * sizeof(f32))) ||
		!file.read(reinterpret_cast<char*>(costs.data()), static_cast<std::streamsize>(costs.size() * sizeof(f32))))
	{
		return false;
	}

	m_Width = header.Width;
	m_Height = header.Height;
	m_CellSize = header.CellSize;
	m_Origin = vec3(header.Origin[0], header.Origin[1], header.Origin[2]);
	m_MaxStepHeight = std::max(0.0f, header.MaxStepHeight);
	m_Walkable.resize(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_Walkable[i] = walkableBytes[i] != 0;
	}
	m_CellHeights = std::move(heights);
	m_CellCosts = std::move(costs);
	return true;
}

std::filesystem::path NavGrid::MakeDefaultPath(const std::filesystem::path& scenePath)
{
	auto name = scenePath.filename().wstring();
	for (const auto* suffix : { L".bamscene.json", L".json", L".bamscene" })
	{
		const std::wstring_view view(suffix);
		if (name.size() > view.size() && name.ends_with(view))
		{
			name.resize(name.size() - view.size());
			break;
		}
	}
	return scenePath.parent_path() / (name + L"_NavGrid.bamnavgrid");
}

std::filesystem::path NavGrid::ResolveDataPath(const std::string& navGridUri)
{
	const auto path = Utf8ToPath(navGridUri);
	return PlatformPath::IsVirtualUri(navGridUri) ? PlatformPath::ResolveUri(path) : PlatformPath::ResolveProjectRelative(path);
}

END
