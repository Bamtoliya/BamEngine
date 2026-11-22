#pragma once

#include "Engine_Includes.h"
#include "Types.h"
#include "Collision.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

BEGIN(Engine)

// 3D 균일 공간 분할 그리드 브로드페이즈 (Uniform Spatial Hash Grid)
class SpatialBroadphase3D
{
public:
	using ID = entt::entity;

	struct Entry
	{
		ID id = entt::null;
		BoundingBox aabb;
	};

	explicit SpatialBroadphase3D(f32 cellSize = 2.0f)
		: m_CellSize(cellSize > 0.001f ? cellSize : 2.0f)
		, m_InvCellSize(1.0f / m_CellSize)
	{
	}

	void SetCellSize(f32 cellSize)
	{
		m_CellSize = cellSize > 0.001f ? cellSize : 2.0f;
		m_InvCellSize = 1.0f / m_CellSize;
		Clear();
	}

	f32 GetCellSize() const { return m_CellSize; }

	void Clear()
	{
		m_Grid.clear();
		m_Entries.clear();
	}

	void Insert(ID id, const BoundingBox& aabb)
	{
		const size_t entryIdx = m_Entries.size();
		m_Entries.push_back({ id, aabb });

		const int32 minX = static_cast<int32>(std::floor(aabb.min.x * m_InvCellSize));
		const int32 minY = static_cast<int32>(std::floor(aabb.min.y * m_InvCellSize));
		const int32 minZ = static_cast<int32>(std::floor(aabb.min.z * m_InvCellSize));

		const int32 maxX = static_cast<int32>(std::floor(aabb.max.x * m_InvCellSize));
		const int32 maxY = static_cast<int32>(std::floor(aabb.max.y * m_InvCellSize));
		const int32 maxZ = static_cast<int32>(std::floor(aabb.max.z * m_InvCellSize));

		const int32 spanX = std::clamp(maxX - minX, 0, 32);
		const int32 spanY = std::clamp(maxY - minY, 0, 32);
		const int32 spanZ = std::clamp(maxZ - minZ, 0, 32);

		for (int32 z = minZ; z <= minZ + spanZ; ++z)
		{
			for (int32 y = minY; y <= minY + spanY; ++y)
			{
				for (int32 x = minX; x <= minX + spanX; ++x)
				{
					const uint64_t key = HashCell(x, y, z);
					m_Grid[key].push_back(entryIdx);
				}
			}
		}
	}

	// 중복 제거된 후보 충돌 쌍 생성 (평균 O(N))
	void QueryPairs(std::vector<std::pair<ID, ID>>& outPairs) const
	{
		outPairs.clear();
		for (const auto& [cellKey, indices] : m_Grid)
		{
			const size_t count = indices.size();
			for (size_t i = 0; i < count; ++i)
			{
				const size_t idxA = indices[i];
				const auto& entryA = m_Entries[idxA];
				for (size_t j = i + 1; j < count; ++j)
				{
					const size_t idxB = indices[j];
					const auto& entryB = m_Entries[idxB];
					if (entryA.id == entryB.id) continue;

					// AABB 겹침 1차 필터링
					if (entryA.aabb.max.x >= entryB.aabb.min.x && entryA.aabb.min.x <= entryB.aabb.max.x &&
						entryA.aabb.max.y >= entryB.aabb.min.y && entryA.aabb.min.y <= entryB.aabb.max.y &&
						entryA.aabb.max.z >= entryB.aabb.min.z && entryA.aabb.min.z <= entryB.aabb.max.z)
					{
						ID a = entryA.id;
						ID b = entryB.id;
						if (b < a) std::swap(a, b);
						outPairs.emplace_back(a, b);
					}
				}
			}
		}

		std::sort(outPairs.begin(), outPairs.end());
		outPairs.erase(std::unique(outPairs.begin(), outPairs.end()), outPairs.end());
	}

	void QueryAABB(const BoundingBox& queryBox, std::vector<ID>& outResults) const
	{
		outResults.clear();
		const int32 minX = static_cast<int32>(std::floor(queryBox.min.x * m_InvCellSize));
		const int32 minY = static_cast<int32>(std::floor(queryBox.min.y * m_InvCellSize));
		const int32 minZ = static_cast<int32>(std::floor(queryBox.min.z * m_InvCellSize));

		const int32 maxX = static_cast<int32>(std::floor(queryBox.max.x * m_InvCellSize));
		const int32 maxY = static_cast<int32>(std::floor(queryBox.max.y * m_InvCellSize));
		const int32 maxZ = static_cast<int32>(std::floor(queryBox.max.z * m_InvCellSize));

		const int32 spanX = std::clamp(maxX - minX, 0, 32);
		const int32 spanY = std::clamp(maxY - minY, 0, 32);
		const int32 spanZ = std::clamp(maxZ - minZ, 0, 32);

		for (int32 z = minZ; z <= minZ + spanZ; ++z)
		{
			for (int32 y = minY; y <= minY + spanY; ++y)
			{
				for (int32 x = minX; x <= minX + spanX; ++x)
				{
					const uint64_t key = HashCell(x, y, z);
					auto it = m_Grid.find(key);
					if (it == m_Grid.end()) continue;

					for (size_t idx : it->second)
					{
						const auto& entry = m_Entries[idx];
						if (queryBox.max.x >= entry.aabb.min.x && queryBox.min.x <= entry.aabb.max.x &&
							queryBox.max.y >= entry.aabb.min.y && queryBox.min.y <= entry.aabb.max.y &&
							queryBox.max.z >= entry.aabb.min.z && queryBox.min.z <= entry.aabb.max.z)
						{
							outResults.push_back(entry.id);
						}
					}
				}
			}
		}

		std::sort(outResults.begin(), outResults.end());
		outResults.erase(std::unique(outResults.begin(), outResults.end()), outResults.end());
	}

	void QueryPoint(const vec3& point, std::vector<ID>& outResults) const
	{
		outResults.clear();
		const int32 cx = static_cast<int32>(std::floor(point.x * m_InvCellSize));
		const int32 cy = static_cast<int32>(std::floor(point.y * m_InvCellSize));
		const int32 cz = static_cast<int32>(std::floor(point.z * m_InvCellSize));

		const uint64_t key = HashCell(cx, cy, cz);
		auto it = m_Grid.find(key);
		if (it == m_Grid.end()) return;

		for (size_t idx : it->second)
		{
			const auto& entry = m_Entries[idx];
			if (point.x >= entry.aabb.min.x && point.x <= entry.aabb.max.x &&
				point.y >= entry.aabb.min.y && point.y <= entry.aabb.max.y &&
				point.z >= entry.aabb.min.z && point.z <= entry.aabb.max.z)
			{
				outResults.push_back(entry.id);
			}
		}

		std::sort(outResults.begin(), outResults.end());
		outResults.erase(std::unique(outResults.begin(), outResults.end()), outResults.end());
	}

	size_t GetEntryCount() const { return m_Entries.size(); }
	size_t GetActiveCellCount() const { return m_Grid.size(); }

private:
	static uint64_t HashCell(int32 x, int32 y, int32 z)
	{
		const uint64_t ux = static_cast<uint64_t>(static_cast<uint32>(x));
		const uint64_t uy = static_cast<uint64_t>(static_cast<uint32>(y));
		const uint64_t uz = static_cast<uint64_t>(static_cast<uint32>(z));
		return (ux * 73856093ULL) ^ (uy * 19349663ULL) ^ (uz * 83492791ULL);
	}

	f32 m_CellSize = 2.0f;
	f32 m_InvCellSize = 0.5f;
	std::unordered_map<uint64_t, std::vector<size_t>> m_Grid;
	std::vector<Entry> m_Entries;
};

// 2D 균일 공간 분할 그리드 브로드페이즈
class SpatialBroadphase2D
{
public:
	using ID = entt::entity;

	struct AABB2D
	{
		vec2 min = vec2(0.0f);
		vec2 max = vec2(0.0f);
	};

	struct Entry
	{
		ID id = entt::null;
		AABB2D aabb;
	};

	explicit SpatialBroadphase2D(f32 cellSize = 2.0f)
		: m_CellSize(cellSize > 0.001f ? cellSize : 2.0f)
		, m_InvCellSize(1.0f / m_CellSize)
	{
	}

	void SetCellSize(f32 cellSize)
	{
		m_CellSize = cellSize > 0.001f ? cellSize : 2.0f;
		m_InvCellSize = 1.0f / m_CellSize;
		Clear();
	}

	f32 GetCellSize() const { return m_CellSize; }

	void Clear()
	{
		m_Grid.clear();
		m_Entries.clear();
	}

	void Insert(ID id, const vec2& min, const vec2& max)
	{
		const size_t entryIdx = m_Entries.size();
		m_Entries.push_back({ id, { min, max } });

		const int32 minX = static_cast<int32>(std::floor(min.x * m_InvCellSize));
		const int32 minY = static_cast<int32>(std::floor(min.y * m_InvCellSize));
		const int32 maxX = static_cast<int32>(std::floor(max.x * m_InvCellSize));
		const int32 maxY = static_cast<int32>(std::floor(max.y * m_InvCellSize));

		const int32 spanX = std::clamp(maxX - minX, 0, 32);
		const int32 spanY = std::clamp(maxY - minY, 0, 32);

		for (int32 y = minY; y <= minY + spanY; ++y)
		{
			for (int32 x = minX; x <= minX + spanX; ++x)
			{
				const uint64_t key = HashCell(x, y);
				m_Grid[key].push_back(entryIdx);
			}
		}
	}

	void Insert(ID id, const Rect& rect)
	{
		Insert(id, vec2(rect.left, rect.top), vec2(rect.Right(), rect.Bottom()));
	}

	void QueryPairs(std::vector<std::pair<ID, ID>>& outPairs) const
	{
		outPairs.clear();
		for (const auto& [cellKey, indices] : m_Grid)
		{
			const size_t count = indices.size();
			for (size_t i = 0; i < count; ++i)
			{
				const size_t idxA = indices[i];
				const auto& entryA = m_Entries[idxA];
				for (size_t j = i + 1; j < count; ++j)
				{
					const size_t idxB = indices[j];
					const auto& entryB = m_Entries[idxB];
					if (entryA.id == entryB.id) continue;

					if (entryA.aabb.max.x >= entryB.aabb.min.x && entryA.aabb.min.x <= entryB.aabb.max.x &&
						entryA.aabb.max.y >= entryB.aabb.min.y && entryA.aabb.min.y <= entryB.aabb.max.y)
					{
						ID a = entryA.id;
						ID b = entryB.id;
						if (b < a) std::swap(a, b);
						outPairs.emplace_back(a, b);
					}
				}
			}
		}

		std::sort(outPairs.begin(), outPairs.end());
		outPairs.erase(std::unique(outPairs.begin(), outPairs.end()), outPairs.end());
	}

	void QueryAABB(const vec2& min, const vec2& max, std::vector<ID>& outResults) const
	{
		outResults.clear();
		const int32 minX = static_cast<int32>(std::floor(min.x * m_InvCellSize));
		const int32 minY = static_cast<int32>(std::floor(min.y * m_InvCellSize));
		const int32 maxX = static_cast<int32>(std::floor(max.x * m_InvCellSize));
		const int32 maxY = static_cast<int32>(std::floor(max.y * m_InvCellSize));

		const int32 spanX = std::clamp(maxX - minX, 0, 32);
		const int32 spanY = std::clamp(maxY - minY, 0, 32);

		for (int32 y = minY; y <= minY + spanY; ++y)
		{
			for (int32 x = minX; x <= minX + spanX; ++x)
			{
				const uint64_t key = HashCell(x, y);
				auto it = m_Grid.find(key);
				if (it == m_Grid.end()) continue;

				for (size_t idx : it->second)
				{
					const auto& entry = m_Entries[idx];
					if (max.x >= entry.aabb.min.x && min.x <= entry.aabb.max.x &&
						max.y >= entry.aabb.min.y && min.y <= entry.aabb.max.y)
					{
						outResults.push_back(entry.id);
					}
				}
			}
		}

		std::sort(outResults.begin(), outResults.end());
		outResults.erase(std::unique(outResults.begin(), outResults.end()), outResults.end());
	}

	void QueryPoint(const vec2& point, std::vector<ID>& outResults) const
	{
		outResults.clear();
		const int32 cx = static_cast<int32>(std::floor(point.x * m_InvCellSize));
		const int32 cy = static_cast<int32>(std::floor(point.y * m_InvCellSize));

		const uint64_t key = HashCell(cx, cy);
		auto it = m_Grid.find(key);
		if (it == m_Grid.end()) return;

		for (size_t idx : it->second)
		{
			const auto& entry = m_Entries[idx];
			if (point.x >= entry.aabb.min.x && point.x <= entry.aabb.max.x &&
				point.y >= entry.aabb.min.y && point.y <= entry.aabb.max.y)
			{
				outResults.push_back(entry.id);
			}
		}

		std::sort(outResults.begin(), outResults.end());
		outResults.erase(std::unique(outResults.begin(), outResults.end()), outResults.end());
	}

	size_t GetEntryCount() const { return m_Entries.size(); }
	size_t GetActiveCellCount() const { return m_Grid.size(); }

private:
	static uint64_t HashCell(int32 x, int32 y)
	{
		const uint64_t ux = static_cast<uint64_t>(static_cast<uint32>(x));
		const uint64_t uy = static_cast<uint64_t>(static_cast<uint32>(y));
		return (ux * 73856093ULL) ^ (uy * 19349663ULL);
	}

	f32 m_CellSize = 2.0f;
	f32 m_InvCellSize = 0.5f;
	std::unordered_map<uint64_t, std::vector<size_t>> m_Grid;
	std::vector<Entry> m_Entries;
};

using SpatialBroadphase = SpatialBroadphase3D;

END
